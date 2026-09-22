#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

const char *VERSAO_ATUAL = "1.0";
const char *SSID = "Wokwi-GUEST";
const char *URL_MANIFESTO = "https://raw.githubusercontent.com/vitorlimeira22/motiva-ota-esp32-cp2/main/version.json";

const uint8_t PINO_LED_VERMELHO = 25;
const uint8_t PINO_LED_VERDE = 26;
const uint8_t PINO_LED_AZUL = 27;

const unsigned long INTERVALO_LEITURA_MS = 2000;
const unsigned long INTERVALO_SESSAO_MS = 48000;
const uint8_t TOTAL_LEITURAS = 5;
const uint8_t SESSOES_ANTES_OTA = 3;

int leituras[TOTAL_LEITURAS];
uint8_t indiceLeitura = 0;
uint8_t sessoesConcluidas = 0;
unsigned long proximaLeituraEm = 0;
unsigned long proximaSessaoEm = 0;
bool sessaoAtiva = false;
bool verificarOta = false;

// Semente fixa para uma demonstracao reproduzivel. Os valores continuam
// sendo gerados por um algoritmo pseudoaleatorio.
uint32_t estadoPseudoaleatorio = 87;

void definirLed(bool vermelho, bool verde, bool azul) {
  digitalWrite(PINO_LED_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(PINO_LED_VERDE, verde ? HIGH : LOW);
  digitalWrite(PINO_LED_AZUL, azul ? HIGH : LOW);
}

int gerarAltura() {
  estadoPseudoaleatorio = 1664525UL * estadoPseudoaleatorio + 1013904223UL;
  return 10 + (estadoPseudoaleatorio % 11);
}

bool instanteAtingido(unsigned long agora, unsigned long alvo) {
  return static_cast<long>(agora - alvo) >= 0;
}

void iniciarSessao(unsigned long inicioProgramado) {
  indiceLeitura = 0;
  sessaoAtiva = true;
  proximaLeituraEm = inicioProgramado;

  Serial.println();
  Serial.println("----------------------------------------");
  Serial.printf("INICIO DA SESSAO %u - FW %s\n", sessoesConcluidas + 1, VERSAO_ATUAL);
  Serial.println("----------------------------------------");
}

void concluirSessao() {
  long soma = 0;
  for (uint8_t i = 0; i < TOTAL_LEITURAS; i++) {
    soma += leituras[i];
  }

  float media = soma / static_cast<float>(TOTAL_LEITURAS);
  sessoesConcluidas++;
  sessaoAtiva = false;

  Serial.printf("Media da sessao: %.1f cm\n", media);
  Serial.printf("Sessao %u concluida.\n", sessoesConcluidas);
  Serial.println("Proxima sessao no marco de 48 segundos.");

  if (sessoesConcluidas >= SESSOES_ANTES_OTA) {
    verificarOta = true;
  }
}

void processarLeituras(unsigned long agora) {
  if (!sessaoAtiva || indiceLeitura >= TOTAL_LEITURAS ||
      !instanteAtingido(agora, proximaLeituraEm)) {
    return;
  }

  leituras[indiceLeitura] = gerarAltura();
  Serial.printf("Leitura %u: %d cm\n", indiceLeitura + 1, leituras[indiceLeitura]);
  indiceLeitura++;
  proximaLeituraEm += INTERVALO_LEITURA_MS;

  if (indiceLeitura == TOTAL_LEITURAS) {
    concluirSessao();
  }
}

bool conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.println("[OTA] Conectando a rede Wokwi-GUEST...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, "", 6);

  unsigned long limite = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED && !instanteAtingido(millis(), limite)) {
    delay(100);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA][ERRO] Nao foi possivel conectar ao Wi-Fi.");
    return false;
  }

  Serial.print("[OTA] Wi-Fi conectado. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool extrairCampoJson(const String &json, const String &campo, String &valor) {
  String chave = "\"" + campo + "\"";
  int inicioChave = json.indexOf(chave);
  if (inicioChave < 0) {
    return false;
  }

  int doisPontos = json.indexOf(':', inicioChave + chave.length());
  int primeiraAspa = json.indexOf('"', doisPontos + 1);
  int segundaAspa = json.indexOf('"', primeiraAspa + 1);
  if (doisPontos < 0 || primeiraAspa < 0 || segundaAspa < 0) {
    return false;
  }

  valor = json.substring(primeiraAspa + 1, segundaAspa);
  valor.trim();
  return valor.length() > 0;
}

int compararVersoes(const String &a, const String &b) {
  int posicaoA = 0;
  int posicaoB = 0;

  while (posicaoA < a.length() || posicaoB < b.length()) {
    int fimA = a.indexOf('.', posicaoA);
    int fimB = b.indexOf('.', posicaoB);
    if (fimA < 0) fimA = a.length();
    if (fimB < 0) fimB = b.length();

    int parteA = a.substring(posicaoA, fimA).toInt();
    int parteB = b.substring(posicaoB, fimB).toInt();
    if (parteA != parteB) {
      return parteA > parteB ? 1 : -1;
    }

    posicaoA = fimA + 1;
    posicaoB = fimB + 1;
  }
  return 0;
}

bool baixarEInstalarFirmware(const String &urlFirmware) {
  WiFiClientSecure clienteSeguro;
  clienteSeguro.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(clienteSeguro, urlFirmware)) {
    Serial.println("[OTA][ERRO] Nao foi possivel iniciar o download do firmware.");
    return false;
  }

  Serial.println("[OTA] Baixando firmware_v2.bin...");
  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    Serial.printf("[OTA][ERRO] Download retornou HTTP %d.\n", codigoHttp);
    http.end();
    return false;
  }

  int tamanho = http.getSize();
  if (tamanho <= 0) {
    Serial.println("[OTA][ERRO] O servidor nao informou um tamanho de firmware valido.");
    http.end();
    return false;
  }

  if (!Update.begin(tamanho)) {
    Serial.printf("[OTA][ERRO] Espaco insuficiente. Codigo %u.\n", Update.getError());
    http.end();
    return false;
  }

  size_t gravados = Update.writeStream(*http.getStreamPtr());
  if (gravados != static_cast<size_t>(tamanho)) {
    Serial.printf("[OTA][ERRO] Foram gravados %u de %d bytes.\n",
                  static_cast<unsigned int>(gravados), tamanho);
    Update.abort();
    http.end();
    return false;
  }

  if (!Update.end() || !Update.isFinished()) {
    Serial.printf("[OTA][ERRO] Falha ao finalizar a atualizacao. Codigo %u.\n",
                  Update.getError());
    http.end();
    return false;
  }

  http.end();
  Serial.println("[OTA] Firmware 2.0 gravado com sucesso.");
  Serial.println("[OTA] Reiniciando o ESP32...");
  delay(1000);
  ESP.restart();
  return true;
}

void consultarAtualizacao() {
  verificarOta = false;

  Serial.println();
  Serial.printf("[OTA] Tres sessoes concluidas. Versao instalada: %s\n", VERSAO_ATUAL);
  if (!conectarWifi()) {
    return;
  }

  WiFiClientSecure clienteSeguro;
  clienteSeguro.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  if (!http.begin(clienteSeguro, URL_MANIFESTO)) {
    Serial.println("[OTA][ERRO] Nao foi possivel preparar a consulta ao manifesto.");
    return;
  }

  Serial.println("[OTA] Consultando version.json...");
  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    Serial.printf("[OTA][ERRO] Manifesto indisponivel. HTTP %d.\n", codigoHttp);
    http.end();
    return;
  }

  String manifesto = http.getString();
  http.end();

  String versaoDisponivel;
  String urlFirmware;
  if (!extrairCampoJson(manifesto, "version", versaoDisponivel) ||
      !extrairCampoJson(manifesto, "url", urlFirmware)) {
    Serial.println("[OTA][ERRO] Manifesto invalido: campos version e url sao obrigatorios.");
    return;
  }

  Serial.printf("[OTA] Versao disponivel: %s\n", versaoDisponivel.c_str());
  if (compararVersoes(versaoDisponivel, VERSAO_ATUAL) <= 0) {
    Serial.println("[OTA] A versao instalada ja e a mais recente.");
    return;
  }

  Serial.println("[OTA] Nova versao encontrada. Iniciando atualizacao.");
  baixarEInstalarFirmware(urlFirmware);
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  pinMode(PINO_LED_VERDE, OUTPUT);
  pinMode(PINO_LED_AZUL, OUTPUT);
  definirLed(false, false, true);

  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("LED AZUL: FIRMWARE 1.0 EM EXECUCAO");
  Serial.println("========================================");

  unsigned long agora = millis();
  iniciarSessao(agora);
  proximaSessaoEm = agora + INTERVALO_SESSAO_MS;
}

void loop() {
  unsigned long agora = millis();
  processarLeituras(agora);

  if (!sessaoAtiva && instanteAtingido(agora, proximaSessaoEm)) {
    unsigned long inicioProgramado = proximaSessaoEm;
    proximaSessaoEm += INTERVALO_SESSAO_MS;
    iniciarSessao(inicioProgramado);
  }

  if (verificarOta && !sessaoAtiva) {
    consultarAtualizacao();
  }

  delay(10);
}
