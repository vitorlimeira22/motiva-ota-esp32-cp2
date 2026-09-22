#include <Arduino.h>

const char *VERSAO_ATUAL = "2.0";

const uint8_t PINO_LED_VERMELHO = 25;
const uint8_t PINO_LED_VERDE = 26;
const uint8_t PINO_LED_AZUL = 27;

const unsigned long INTERVALO_LEITURA_MS = 2000;
const unsigned long INTERVALO_SESSAO_MS = 48000;
const uint8_t TOTAL_LEITURAS = 5;

enum EstadoVegetacao {
  NORMAL,
  ALERTA
};

int leituras[TOTAL_LEITURAS];
uint8_t indiceLeitura = 0;
uint16_t sessoesConcluidas = 0;
unsigned long proximaLeituraEm = 0;
unsigned long proximaSessaoEm = 0;
bool sessaoAtiva = false;
EstadoVegetacao estadoAtual = NORMAL;

// Semente fixa para que os tres primeiros ciclos comprovem os tres ramos
// da histerese: ALERTA, manutencao do estado e NORMAL.
uint32_t estadoPseudoaleatorio = 87;

void definirLed(bool vermelho, bool verde, bool azul) {
  digitalWrite(PINO_LED_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(PINO_LED_VERDE, verde ? HIGH : LOW);
  digitalWrite(PINO_LED_AZUL, azul ? HIGH : LOW);
}

void atualizarLedDoEstado() {
  if (estadoAtual == ALERTA) {
    definirLed(true, false, false);
  } else {
    definirLed(false, true, false);
  }
}

const char *nomeDoEstado(EstadoVegetacao estado) {
  return estado == ALERTA ? "ALERTA" : "NORMAL";
}

int gerarAltura() {
  estadoPseudoaleatorio = 1664525UL * estadoPseudoaleatorio + 1013904223UL;
  return 10 + (estadoPseudoaleatorio % 11);
}

bool instanteAtingido(unsigned long agora, unsigned long alvo) {
  return static_cast<long>(agora - alvo) >= 0;
}

void ordenarCopia(const int origem[], int destino[], uint8_t tamanho) {
  for (uint8_t i = 0; i < tamanho; i++) {
    destino[i] = origem[i];
  }

  for (uint8_t i = 0; i < tamanho - 1; i++) {
    for (uint8_t j = 0; j < tamanho - i - 1; j++) {
      if (destino[j] > destino[j + 1]) {
        int temporario = destino[j];
        destino[j] = destino[j + 1];
        destino[j + 1] = temporario;
      }
    }
  }
}

void imprimirVetor(const char *rotulo, const int valores[], uint8_t tamanho) {
  Serial.print(rotulo);
  for (uint8_t i = 0; i < tamanho; i++) {
    Serial.print(valores[i]);
    if (i < tamanho - 1) {
      Serial.print(' ');
    }
  }
  Serial.println();
}

void aplicarHisterese(int mediana) {
  EstadoVegetacao estadoAnterior = estadoAtual;

  if (mediana >= 16) {
    estadoAtual = ALERTA;
  } else if (mediana <= 14) {
    estadoAtual = NORMAL;
  }

  Serial.printf("Estado anterior: %s\n", nomeDoEstado(estadoAnterior));
  if (mediana > 14 && mediana < 16) {
    Serial.println("Faixa de histerese: estado anterior mantido.");
  } else if (estadoAnterior == estadoAtual) {
    Serial.println("Limiar confirmado: estado permanece inalterado.");
  } else {
    Serial.println("Limiar cruzado: estado alterado.");
  }
  Serial.printf("Estado atual: %s\n", nomeDoEstado(estadoAtual));

  atualizarLedDoEstado();
  Serial.println(estadoAtual == ALERTA
                     ? "LED VERMELHO: ESTADO ALERTA"
                     : "LED VERDE: ESTADO NORMAL");
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
  int leiturasOrdenadas[TOTAL_LEITURAS];
  ordenarCopia(leituras, leiturasOrdenadas, TOTAL_LEITURAS);

  long soma = 0;
  for (uint8_t i = 0; i < TOTAL_LEITURAS; i++) {
    soma += leituras[i];
  }

  float media = soma / static_cast<float>(TOTAL_LEITURAS);
  int mediana = leiturasOrdenadas[TOTAL_LEITURAS / 2];

  imprimirVetor("Leituras na ordem original: ", leituras, TOTAL_LEITURAS);
  imprimirVetor("Leituras em ordem crescente: ", leiturasOrdenadas, TOTAL_LEITURAS);
  Serial.printf("Media da sessao: %.1f cm\n", media);
  Serial.printf("Mediana da sessao: %d cm\n", mediana);
  aplicarHisterese(mediana);

  sessoesConcluidas++;
  sessaoAtiva = false;
  Serial.printf("Sessao %u concluida.\n", sessoesConcluidas);
  Serial.println("Proxima sessao no marco de 48 segundos.");
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

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  pinMode(PINO_LED_VERDE, OUTPUT);
  pinMode(PINO_LED_AZUL, OUTPUT);
  estadoAtual = NORMAL;
  atualizarLedDoEstado();

  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  Serial.println("MEDIA + ORDENACAO + MEDIANA + HISTERESE");
  Serial.println("========================================");
  Serial.println("Firmware atualizado por OTA e reiniciado com sucesso.");

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

  delay(10);
}
