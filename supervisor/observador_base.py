"""Interface IAlarmeObserver — base abstrata do Padrão Observer.

Qualquer canal de notificação (tela, e-mail, SMS, Telegram…) deve herdar
desta classe e implementar os dois métodos abstratos.
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime, timezone


# ---------------------------------------------------------------------------
# Objeto de valor: EventoAlarme
# ---------------------------------------------------------------------------

@dataclass
class EventoAlarme:
    """Payload imutável que representa uma transição de estado de alarme."""
    codigo: str
    severidade: str
    mensagem: str
    transicao: str          # "ativacao" | "normalizacao"
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
    nivel: float = 0.0
    temperatura: float = 0.0

    def eh_ativacao(self) -> bool:
        return self.transicao == "ativacao"

    def eh_normalizacao(self) -> bool:
        return self.transicao == "normalizacao"


# ---------------------------------------------------------------------------
# Interface do observador
# ---------------------------------------------------------------------------

class IAlarmeObserver(ABC):
    """Interface abstrata: todo observador de alarme deve implementar estes dois métodos."""

    @abstractmethod
    def notificar_ativacao(self, evento: EventoAlarme) -> None:
        """Chamado UMA única vez quando o alarme transita de inativo → ativo."""
        ...

    @abstractmethod
    def notificar_normalizacao(self, evento: EventoAlarme) -> None:
        """Chamado UMA única vez quando o alarme transita de ativo → inativo."""
        ...
