from pathlib import Path
import json
import re
import unittest


ROOT = Path(__file__).resolve().parent


def gerar_sequencia(seed: int, quantidade: int) -> list[int]:
    estado = seed
    valores = []
    for _ in range(quantidade):
        estado = (1664525 * estado + 1013904223) & 0xFFFFFFFF
        valores.append(10 + estado % 11)
    return valores


def aplicar_histerese(mediana: int, estado_anterior: str) -> str:
    if mediana >= 16:
        return "ALERTA"
    if mediana <= 14:
        return "NORMAL"
    return estado_anterior


class TesteFirmware(unittest.TestCase):
    def test_gerador_fica_entre_10_e_20(self):
        valores = gerar_sequencia(87, 1000)
        self.assertTrue(all(10 <= valor <= 20 for valor in valores))

    def test_primeiras_sessoes_cobrem_histerese(self):
        valores = gerar_sequencia(87, 15)
        medianas = [sorted(valores[i:i + 5])[2] for i in range(0, 15, 5)]
        self.assertEqual([18, 15, 14], medianas)

        estado = "NORMAL"
        estados = []
        for mediana in medianas:
            estado = aplicar_histerese(mediana, estado)
            estados.append(estado)
        self.assertEqual(["ALERTA", "ALERTA", "NORMAL"], estados)

    def test_temporizacao_das_sessoes(self):
        inicios = [0 + indice * 48000 for indice in range(4)]
        leituras = [[inicio + passo * 2000 for passo in range(5)] for inicio in inicios]
        self.assertEqual([0, 2000, 4000, 6000, 8000], leituras[0])
        self.assertEqual(48000, leituras[1][0])
        self.assertEqual(48000, inicios[1] - inicios[0])

    def test_manifesto_tem_campos_obrigatorios(self):
        manifesto = json.loads((ROOT / "version.json").read_text(encoding="utf-8"))
        self.assertEqual("2.0", manifesto["version"])
        self.assertRegex(manifesto["url"], r"^https://")

    def test_codigo_contem_tratamentos_minimos(self):
        codigo = (ROOT / "firmware_v1.ino").read_text(encoding="utf-8")
        mensagens = [
            "Nao foi possivel conectar ao Wi-Fi",
            "Manifesto indisponivel",
            "A versao instalada ja e a mais recente",
            "Download retornou HTTP",
            "Falha ao finalizar a atualizacao",
        ]
        for mensagem in mensagens:
            self.assertIn(mensagem, codigo)

    def test_codigo_v2_possui_evolucao_real(self):
        codigo = (ROOT / "firmware_v2.ino").read_text(encoding="utf-8")
        for elemento in ["ordenarCopia", "Mediana da sessao", "aplicarHisterese", "LED VERMELHO"]:
            self.assertIn(elemento, codigo)
        self.assertIsNone(re.search(r"delay\s*\(\s*48000\s*\)", codigo))


if __name__ == "__main__":
    unittest.main(verbosity=2)
