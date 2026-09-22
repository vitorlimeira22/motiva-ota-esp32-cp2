# Projeto Motiva - Atualizacao remota de firmware (OTA)

Checkpoint 2 da disciplina ministrada pelo professor Marcelo Fernando Morgantini. A solucao simula um no de monitoramento de vegetacao em um ESP32 no Wokwi. O equipamento executa tres sessoes com o Firmware 1.0, consulta um manifesto remoto e instala o Firmware 2.0 por OTA.

## Integrantes - sala CCPX

| Integrante | RM |
|---|---:|
| Vitor Limeira dos Santos | 565280 |
| Pedro Del Neri Correia | 562168 |
| Lucas de Freitas Barbosa | 564685 |
| Arthur da Silva Alecar | 563684 |
| Felipe Paula Burba Molonhoni | 564395 |

## Links

- Projeto Wokwi: https://wokwi.com/projects/475874796669478913
- Repositorio publico: https://github.com/vitorlimeira22/motiva-ota-esp32-cp2
- Manifesto usado pelo Firmware 1.0: https://raw.githubusercontent.com/vitorlimeira22/motiva-ota-esp32-cp2/main/version.json
- Relatorio de entrega: [Relatorio_CP2_Projeto_Motiva_OTA_CCPX.pdf](./Relatorio_CP2_Projeto_Motiva_OTA_CCPX.pdf)

## Arquitetura

```text
ESP32 no Wokwi (Firmware 1.0)
  |-- 5 leituras pseudoaleatorias a cada sessao
  |-- nova sessao a cada 48 segundos
  |-- apos 3 sessoes, conecta a Wokwi-GUEST
  v
version.json no repositorio remoto
  |-- version: 2.0
  |-- URL direta do firmware_v2.bin
  v
Download HTTPS -> gravacao OTA -> ESP.restart()
  v
ESP32 no Wokwi (Firmware 2.0)
  |-- media, ordenacao e mediana
  |-- histerese NORMAL/ALERTA
  |-- LED verde/vermelho conforme o estado
```

O LED RGB usa os GPIOs 25, 26 e 27. No Firmware 1.0 ele permanece azul. No Firmware 2.0 fica verde no estado NORMAL e vermelho no estado ALERTA.

## Temporizacao

Cada sessao usa uma agenda baseada em `millis()`:

- leitura 1 no segundo 0;
- leitura 2 no segundo 2;
- leitura 3 no segundo 4;
- leitura 4 no segundo 6;
- leitura 5 no segundo 8;
- nova sessao no segundo 48, contado do inicio da sessao anterior.

Nao existe `delay(48000)`. Assim, o tempo das cinco leituras nao e somado ao intervalo entre sessoes.

## Como executar

1. Abra o link publico do Wokwi.
2. Inicie a simulacao e mantenha o Serial Monitor visivel em 115200 baud.
3. Observe o Firmware 1.0 executar tres sessoes. O LED permanece azul.
4. Apos a terceira sessao, o ESP32 conecta a `Wokwi-GUEST`, consulta `version.json`, baixa `firmware_v2.bin`, grava a atualizacao e reinicia.
5. Confirme no Serial Monitor o cabecalho `FW 2.0`, a media, a ordem original, a ordem crescente, a mediana e o estado da histerese.
6. Nas tres primeiras sessoes do Firmware 2.0, as medianas reproduziveis sao 18, 15 e 14 cm. Isso comprova ALERTA, manutencao do estado anterior e retorno a NORMAL.

## Arquivos principais

- `sketch.ino`: copia do Firmware 1.0 usada pelo projeto Wokwi.
- `firmware_v1.ino`: codigo-fonte da versao inicial.
- `firmware_v2.ino`: codigo-fonte da versao atualizada.
- `version.json`: manifesto remoto de atualizacao.
- `firmware_v2.bin`: binario obtido pela compilacao do Firmware 2.0.
- `diagram.json`: circuito do ESP32 com LED RGB.
- `test_logic.py`: verificacoes reproduziveis de temporizacao, faixa de valores e histerese.

## Testes cobertos

| Teste | Comprovacao |
|---|---|
| Firmware 1.0 | Cinco leituras, media e LED azul |
| Sessao completa | Agenda 0, 2, 4, 6 e 8 s; proxima sessao em 48 s |
| Atualizacao disponivel | Consulta somente depois de tres sessoes |
| OTA | Download, `Update.writeStream`, finalizacao e reinicio |
| Firmware 2.0 | Media, ordenacao, mediana e histerese |
| Mediana maior ou igual a 16 | Estado ALERTA e LED vermelho |
| Mediana entre 14 e 16 | Estado anterior mantido |
| Mediana menor ou igual a 14 | Estado NORMAL e LED verde |

## Tratamento de erros

O Firmware 1.0 informa no Serial Monitor quando:

- nao consegue conectar ao Wi-Fi;
- o manifesto esta indisponivel ou invalido;
- a versao instalada ja e a mais recente;
- o arquivo de firmware nao pode ser baixado;
- a gravacao ou a finalizacao da atualizacao falha.

## Bibliotecas

Todas fazem parte do Arduino-ESP32:

- `WiFi.h`: conexao a rede Wokwi-GUEST;
- `WiFiClientSecure.h`: cliente HTTPS usado com o repositorio publico;
- `HTTPClient.h`: requisicoes do manifesto e do binario;
- `Update.h`: escrita do novo firmware na particao OTA.
