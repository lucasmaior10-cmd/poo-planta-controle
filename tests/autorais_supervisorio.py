"""Testes autorais Python — Etapa 03.

Cobre:
  1. RepositorioHistorico (Repository + SQLite + deduplicação)
  2. MonitorAlarmes + NotificadorEmail (Observer + anti-spam)
  3. ValidadorSolicitacao via supervisor (faixas, senha adm)
  4. NotificadorTela (buffer de eventos)
"""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

# Garante que o pacote supervisor seja encontrado
_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_ROOT))

from supervisor.autenticacao import verificar_senha
from supervisor.monitor_alarmes import MonitorAlarmes
from supervisor.notificador_email import NotificadorEmail
from supervisor.notificador_tela import NotificadorTela
from supervisor.observador_base import EventoAlarme
from supervisor.repositorio_historico import RegistroHistorico, RepositorioHistorico


# ---------------------------------------------------------------------------
# Helpers de fixture
# ---------------------------------------------------------------------------

def _telemetria(ciclo: int, sessao: str = "s1", alarmes: list | None = None, nivel: float = 55.0) -> dict:
    return {
        "versao": 1,
        "sessao": sessao,
        "ciclo": ciclo,
        "timestamp": "2026-11-24T20:00:00Z",
        "leituras": [
            {"tag": "LT-01", "tipo": "nivel", "valor": nivel, "unidade": "%", "status": "operando"},
            {"tag": "TT-01", "tipo": "temperatura", "valor": 35.0, "unidade": "C", "status": "operando"},
        ],
        "atuadores": {"bomba": 33.3, "aquecedor": 10.0},
        "ultimo_comando": ciclo,
        "erro_comando": None,
        "motivo_seguro": None,
        "alarmes": alarmes if alarmes is not None else [],
    }


def _alarme(codigo: str, severidade: str = "critico") -> dict:
    return {"codigo": codigo, "mensagem": f"Alarme {codigo}", "severidade": severidade}


# ===========================================================================
# 1. RepositorioHistorico
# ===========================================================================

class TestRepositorioHistorico(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.banco = Path(self.tmp.name) / "test_historico.db"
        self.repo = RepositorioHistorico(self.banco)
        self.repo.inicializar_banco()

    def tearDown(self):
        self.tmp.cleanup()

    def test_salvar_e_recuperar_registro(self):
        tel = _telemetria(1)
        reg = RegistroHistorico.de_telemetria(tel)
        self.assertIsNotNone(reg)
        ok = self.repo.salvar_registro(reg)
        self.assertTrue(ok, "Primeiro insert deve retornar True")
        registros = self.repo.obter_ultimos(10)
        self.assertEqual(len(registros), 1)
        self.assertEqual(registros[0].ciclo, 1)
        self.assertEqual(registros[0].sessao, "s1")

    def test_deduplicacao_insert_or_ignore(self):
        """Inserir o mesmo (sessao, ciclo) duas vezes NÃO duplica."""
        reg = RegistroHistorico.de_telemetria(_telemetria(5))
        self.repo.salvar_registro(reg)
        segundo = self.repo.salvar_registro(reg)
        self.assertFalse(segundo, "Segunda inserção deve retornar False (duplicata)")
        registros = self.repo.obter_ultimos(100)
        self.assertEqual(len(registros), 1)

    def test_multiplos_ciclos_sem_duplicar(self):
        """100 chamadas idênticas com o mesmo ciclo geram 1 único registro."""
        tel = _telemetria(42)
        reg = RegistroHistorico.de_telemetria(tel)
        for _ in range(100):
            self.repo.salvar_registro(reg)
        registros = self.repo.obter_ultimos(200)
        self.assertEqual(len(registros), 1)

    def test_sessoes_distintas_sao_registros_diferentes(self):
        self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(1, "sessao-A")))
        self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(1, "sessao-B")))
        registros = self.repo.obter_ultimos(100)
        self.assertEqual(len(registros), 2)

    def test_exportar_csv_bytes(self):
        self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(1)))
        self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(2)))
        csv_bytes = self.repo.exportar_csv_bytes()
        linhas = csv_bytes.decode("utf-8").strip().splitlines()
        self.assertGreaterEqual(len(linhas), 3)  # cabeçalho + 2 linhas
        self.assertIn("sessao", linhas[0])
        self.assertIn("ciclo", linhas[0])

    def test_registro_invalido_retorna_none(self):
        """Telemetria malformada → RegistroHistorico.de_telemetria → None."""
        self.assertIsNone(RegistroHistorico.de_telemetria({}))
        self.assertIsNone(RegistroHistorico.de_telemetria({"leituras": []}))

    def test_salvar_none_retorna_false(self):
        self.assertFalse(self.repo.salvar_registro(None))

    def test_obter_sessao_atual_ordem_cronologica(self):
        for c in [3, 1, 2]:
            self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(c)))
        sessao = self.repo.obter_sessao_atual(10)
        ciclos = [r.ciclo for r in sessao]
        self.assertEqual(ciclos, sorted(ciclos))

    def test_obter_sessao_atual_retorna_ultimos_ciclos_com_limite(self):
        for c in range(1, 11):
            self.repo.salvar_registro(RegistroHistorico.de_telemetria(_telemetria(c)))
        sessao = self.repo.obter_sessao_atual(limite=3)
        self.assertEqual([r.ciclo for r in sessao], [8, 9, 10])


# ===========================================================================
# 2. Observer — MonitorAlarmes + NotificadorEmail (anti-spam)
# ===========================================================================

class TestObserverAlarmes(unittest.TestCase):

    def _criar_monitor_com_email(self, tmp_dir: Path):
        monitor = MonitorAlarmes()
        email = NotificadorEmail(
            destinatario="teste@empresa.com",
            modo_simulacao=True,
            caminho_log=tmp_dir / "emails.log",
        )
        monitor.registrar_observador(email)
        return monitor, email

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self.tmp.name)
        self.monitor, self.email = self._criar_monitor_com_email(self.tmp_path)

    def tearDown(self):
        self.tmp.cleanup()

    def test_sem_alarmes_sem_email(self):
        """Ciclos sem alarme não devem disparar nenhum e-mail."""
        for c in range(5):
            self.monitor.processar_telemetria(_telemetria(c))
        self.assertEqual(len(self.email.obter_historico_mock()), 0)

    def test_ativacao_dispara_email_uma_vez(self):
        """O alarme ativa apenas na borda de subida (ciclo 1 → ativo)."""
        self.monitor.processar_telemetria(_telemetria(0))  # sem alarme
        # Alarme ativa no ciclo 1
        self.monitor.processar_telemetria(
            _telemetria(1, alarmes=[_alarme("NIVEL_MUITO_ALTO")])
        )
        historico = self.email.obter_historico_mock()
        self.assertEqual(len(historico), 1)
        self.assertIn("ALARME ATIVO", historico[0]["assunto"])
        self.assertIn("NIVEL_MUITO_ALTO", historico[0]["assunto"])

    def test_anti_spam_multiplos_ciclos_ativos(self):
        """Manter alarme ativo por N ciclos NÃO dispara N e-mails."""
        self.monitor.processar_telemetria(_telemetria(0))
        alarme_ativo = [_alarme("NIVEL_MUITO_ALTO")]
        for c in range(1, 20):
            self.monitor.processar_telemetria(_telemetria(c, alarmes=alarme_ativo))
        # Apenas 1 e-mail de ativação
        self.assertEqual(len(self.email.obter_historico_mock()), 1)

    def test_normalizacao_dispara_email_uma_vez(self):
        """Na borda de descida, um e-mail de normalização é enviado."""
        alarme_ativo = [_alarme("NIVEL_MUITO_ALTO")]
        self.monitor.processar_telemetria(_telemetria(0, alarmes=alarme_ativo))
        # Normaliza no ciclo 1
        self.monitor.processar_telemetria(_telemetria(1))
        historico = self.email.obter_historico_mock()
        self.assertEqual(len(historico), 2)
        assuntos = [h["assunto"] for h in historico]
        self.assertTrue(any("ALARME ATIVO" in a for a in assuntos))
        self.assertTrue(any("NORMALIZADO" in a for a in assuntos))

    def test_multiplos_alarmes_distintos(self):
        """Dois alarmes diferentes geram duas notificações de ativação."""
        self.monitor.processar_telemetria(_telemetria(0))
        self.monitor.processar_telemetria(
            _telemetria(1, alarmes=[
                _alarme("NIVEL_MUITO_ALTO"),
                _alarme("TEMP_MUITO_ALTA"),
            ])
        )
        self.assertEqual(len(self.email.obter_historico_mock()), 2)

    def test_reset_limpa_estado(self):
        """Após resetar, re-ativar o mesmo alarme gera novo e-mail."""
        alarme_ativo = [_alarme("NIVEL_MUITO_ALTO")]
        self.monitor.processar_telemetria(_telemetria(0, alarmes=alarme_ativo))
        self.monitor.resetar()
        self.monitor.processar_telemetria(_telemetria(1, alarmes=alarme_ativo))
        # 2 ativações: 1 antes do reset, 1 depois
        self.assertEqual(len(self.email.obter_historico_mock()), 2)


# ===========================================================================
# 3. NotificadorTela
# ===========================================================================

class TestNotificadorTela(unittest.TestCase):

    def test_buffer_acumula_eventos(self):
        tela = NotificadorTela(max_eventos=10)
        evento = EventoAlarme(
            codigo="TEST", severidade="critico",
            mensagem="Teste", transicao="ativacao"
        )
        tela.notificar_ativacao(evento)
        tela.notificar_normalizacao(
            EventoAlarme(codigo="TEST", severidade="info",
                         mensagem="Normal", transicao="normalizacao")
        )
        eventos = tela.obter_eventos_recentes()
        self.assertEqual(len(eventos), 2)

    def test_buffer_circular_limita_tamanho(self):
        tela = NotificadorTela(max_eventos=3)
        for i in range(10):
            tela.notificar_ativacao(
                EventoAlarme(codigo=f"E{i}", severidade="aviso",
                             mensagem="", transicao="ativacao")
            )
        self.assertEqual(len(tela.obter_eventos_recentes()), 3)

    def test_limpar_zera_buffer(self):
        tela = NotificadorTela()
        tela.notificar_ativacao(
            EventoAlarme(codigo="X", severidade="aviso",
                         mensagem="", transicao="ativacao")
        )
        tela.limpar()
        self.assertEqual(len(tela.obter_eventos_recentes()), 0)


# ===========================================================================
# 4. Autenticação
# ===========================================================================

class TestAutenticacao(unittest.TestCase):

    def test_senha_correta(self):
        self.assertTrue(verificar_senha("adm123"))

    def test_senha_errada(self):
        self.assertFalse(verificar_senha(""))
        self.assertFalse(verificar_senha("senha"))
        self.assertFalse(verificar_senha("ADM123"))
        self.assertFalse(verificar_senha("adm1234"))

    def test_senha_case_sensitive(self):
        self.assertFalse(verificar_senha("Adm123"))
        self.assertFalse(verificar_senha("ADM123"))


# ===========================================================================
# 5. EventoAlarme
# ===========================================================================

class TestEventoAlarme(unittest.TestCase):

    def test_ativacao(self):
        ev = EventoAlarme(codigo="X", severidade="critico",
                          mensagem="m", transicao="ativacao")
        self.assertTrue(ev.eh_ativacao())
        self.assertFalse(ev.eh_normalizacao())

    def test_normalizacao(self):
        ev = EventoAlarme(codigo="X", severidade="info",
                          mensagem="m", transicao="normalizacao")
        self.assertFalse(ev.eh_ativacao())
        self.assertTrue(ev.eh_normalizacao())

    def test_timestamp_preenchido_automaticamente(self):
        ev = EventoAlarme(codigo="X", severidade="aviso",
                          mensagem="m", transicao="ativacao")
        self.assertRegex(ev.timestamp, r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")


# ===========================================================================
# Runner
# ===========================================================================

def suite(etapa: int):
    if etapa < 3:
        return unittest.TestSuite()
    loader = unittest.TestLoader()
    s = unittest.TestSuite()
    for cls in [
        TestRepositorioHistorico,
        TestObserverAlarmes,
        TestNotificadorTela,
        TestAutenticacao,
        TestEventoAlarme,
    ]:
        s.addTests(loader.loadTestsFromTestCase(cls))
    return s


if __name__ == "__main__":
    unittest.main()
