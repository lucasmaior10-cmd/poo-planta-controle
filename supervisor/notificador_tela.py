"""NotificadorTela — Observador concreto que acumula eventos para a IHM Streamlit.

Responsabilidade Única: armazenar em memória os eventos de alarme recentes,
disponibilizando-os à interface sem depender de Streamlit diretamente.
"""

from __future__ import annotations

from collections import deque
from typing import Deque, List

from supervisor.observador_base import EventoAlarme, IAlarmeObserver


class NotificadorTela(IAlarmeObserver):
    """Acumula eventos de alarme para exibição na interface Streamlit.

    Mantém um buffer circular de tamanho configurável. A IHM lê
    `obter_eventos_recentes()` a cada ciclo de renderização.
    """

    def __init__(self, max_eventos: int = 50) -> None:
        self._eventos: Deque[EventoAlarme] = deque(maxlen=max_eventos)

    # ------------------------------------------------------------------
    # Implementação da interface IAlarmeObserver
    # ------------------------------------------------------------------

    def notificar_ativacao(self, evento: EventoAlarme) -> None:
        """Registra ativação de alarme no buffer da tela."""
        self._eventos.appendleft(evento)

    def notificar_normalizacao(self, evento: EventoAlarme) -> None:
        """Registra normalização de alarme no buffer da tela."""
        self._eventos.appendleft(evento)

    # ------------------------------------------------------------------
    # API de leitura para a IHM
    # ------------------------------------------------------------------

    def obter_eventos_recentes(self) -> List[EventoAlarme]:
        """Retorna os eventos mais recentes (do mais novo para o mais antigo)."""
        return list(self._eventos)

    def limpar(self) -> None:
        """Zera o buffer (usado em testes ou reset de sessão)."""
        self._eventos.clear()
