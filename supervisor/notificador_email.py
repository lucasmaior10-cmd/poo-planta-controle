"""NotificadorEmail — Observador concreto com modo Real (SMTP) e Simulação (Mock).

Responsabilidade Única: formatar e despachar mensagens de alerta por e-mail
APENAS na transição de estado (borda de subida/descida), evitando spam.

Modo Real:  usa smtplib + TLS para enviar via SMTP.
Modo Mock:  grava as mensagens em dados/notificacoes_email.log (para testes e CI).
"""

from __future__ import annotations

import smtplib
from datetime import datetime, timezone
from email.message import EmailMessage
from pathlib import Path
from typing import Optional

from supervisor.observador_base import EventoAlarme, IAlarmeObserver


class NotificadorEmail(IAlarmeObserver):
    """Envia (ou simula) e-mails na transição de estado dos alarmes.

    Parâmetros
    ----------
    destinatario : str
        Endereço de e-mail de destino.
    remetente : str
        Endereço de e-mail do remetente.
    smtp_host : str
        Endereço do servidor SMTP (ex.: 'smtp.gmail.com').
    smtp_porta : int
        Porta SMTP (587 para TLS, 465 para SSL).
    smtp_usuario : str
        Login SMTP (geralmente o mesmo que remetente).
    smtp_senha : str
        Senha ou token de aplicativo.
    modo_simulacao : bool
        Se True, não envia e-mail real; apenas grava em log.
    caminho_log : Path
        Arquivo de log para o modo simulação.
    """

    def __init__(
        self,
        destinatario: str = "",
        remetente: str = "supervisorio@planta.local",
        smtp_host: str = "smtp.gmail.com",
        smtp_porta: int = 587,
        smtp_usuario: str = "",
        smtp_senha: str = "",
        modo_simulacao: bool = True,
        caminho_log: Optional[Path] = None,
    ) -> None:
        self._destinatario = destinatario
        self._remetente = remetente
        self._smtp_host = smtp_host
        self._smtp_porta = smtp_porta
        self._smtp_usuario = smtp_usuario
        self._smtp_senha = smtp_senha
        self._modo_simulacao = modo_simulacao
        self._caminho_log = caminho_log or Path("dados/notificacoes_email.log")
        self._caminho_log.parent.mkdir(parents=True, exist_ok=True)
        # Histórico de mensagens enviadas (modo mock para testes)
        self._historico_mock: list[dict] = []

    # ------------------------------------------------------------------
    # Implementação da interface IAlarmeObserver
    # ------------------------------------------------------------------

    def notificar_ativacao(self, evento: EventoAlarme) -> None:
        """Disparado UMA vez quando o alarme ativa (borda de subida)."""
        assunto = f"[ALARME ATIVO] {evento.codigo} — {evento.severidade.upper()}"
        corpo = self._formatar_corpo(evento, "ALARME ATIVADO")
        self._disparar(assunto, corpo, evento)

    def notificar_normalizacao(self, evento: EventoAlarme) -> None:
        """Disparado UMA vez quando o alarme normaliza (borda de descida)."""
        assunto = f"[NORMALIZADO] {evento.codigo}"
        corpo = self._formatar_corpo(evento, "ALARME NORMALIZADO")
        self._disparar(assunto, corpo, evento)

    # ------------------------------------------------------------------
    # Leitura do histórico (para testes)
    # ------------------------------------------------------------------

    def obter_historico_mock(self) -> list[dict]:
        """Retorna as mensagens registradas no modo simulação."""
        return list(self._historico_mock)

    def limpar_historico_mock(self) -> None:
        self._historico_mock.clear()

    # ------------------------------------------------------------------
    # Interno
    # ------------------------------------------------------------------

    def _formatar_corpo(self, evento: EventoAlarme, titulo: str) -> str:
        return (
            f"=== {titulo} ===\n\n"
            f"Código     : {evento.codigo}\n"
            f"Severidade : {evento.severidade}\n"
            f"Mensagem   : {evento.mensagem}\n"
            f"Timestamp  : {evento.timestamp}\n"
            f"Nível      : {evento.nivel:.2f}%\n"
            f"Temperatura: {evento.temperatura:.2f}°C\n\n"
            f"Este é um aviso automático do Supervisório da Planta Didática.\n"
        )

    def _disparar(self, assunto: str, corpo: str, evento: EventoAlarme) -> None:
        entrada_log = {
            "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "assunto": assunto,
            "corpo": corpo,
            "destinatario": self._destinatario,
        }

        if self._modo_simulacao:
            self._historico_mock.append(entrada_log)
            self._gravar_mock(assunto, corpo)
            return

        # Modo real: envia via SMTP com TLS
        if not self._destinatario:
            return
        try:
            msg = EmailMessage()
            msg["Subject"] = assunto
            msg["From"] = self._remetente
            msg["To"] = self._destinatario
            msg.set_content(corpo)
            with smtplib.SMTP(self._smtp_host, self._smtp_porta, timeout=10) as servidor:
                servidor.ehlo()
                servidor.starttls()
                if self._smtp_usuario and self._smtp_senha:
                    servidor.login(self._smtp_usuario, self._smtp_senha)
                servidor.send_message(msg)
        except Exception as exc:  # pragma: no cover — falhas SMTP são externas
            # Falha no envio nunca deve derrubar a interface
            self._gravar_mock(f"[ERRO SMTP] {assunto}", f"Erro: {exc}\n\n{corpo}")

    def _gravar_mock(self, assunto: str, corpo: str) -> None:
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        linha = f"\n--- {ts} ---\nAssunto: {assunto}\n{corpo}\n"
        try:
            with open(self._caminho_log, "a", encoding="utf-8") as f:
                f.write(linha)
        except OSError:
            pass
