"""MonitorAlarmes — Subject do Padrão Observer.

Responsabilidade Única: comparar a lista de alarmes do ciclo atual com
o estado anterior e disparar eventos APENAS na transição de estado
(borda de subida ao ativar, borda de descida ao normalizar).

Isso garante anti-spam: o operador recebe UMA notificação quando o
problema nasce e UMA quando é resolvido, não uma por segundo.
"""

from __future__ import annotations

from typing import List, Optional, Set

from supervisor.observador_base import EventoAlarme, IAlarmeObserver


class MonitorAlarmes:
    """Subject do Padrão Observer para transições de estado de alarmes.

    Uso:
        monitor = MonitorAlarmes()
        monitor.registrar_observador(NotificadorTela())
        monitor.registrar_observador(NotificadorEmail(...))
        monitor.processar_telemetria(telemetria_dict)
    """

    def __init__(self) -> None:
        self._observadores: List[IAlarmeObserver] = []
        self._codigos_ativos: Set[str] = set()
        self._ultimo_nivel: float = 0.0
        self._ultima_temperatura: float = 0.0

    # ------------------------------------------------------------------
    # Gerenciamento de observadores
    # ------------------------------------------------------------------

    def registrar_observador(self, obs: IAlarmeObserver) -> None:
        if obs not in self._observadores:
            self._observadores.append(obs)

    def remover_observador(self, obs: IAlarmeObserver) -> None:
        if obs in self._observadores:
            self._observadores.remove(obs)

    # ------------------------------------------------------------------
    # Processamento de telemetria
    # ------------------------------------------------------------------

    def processar_telemetria(self, telemetria: dict) -> None:
        """Extrai alarmes da telemetria e notifica observadores nas bordas."""
        alarmes_lista = telemetria.get("alarmes", [])
        if not isinstance(alarmes_lista, list):
            alarmes_lista = []

        # Atualiza contexto de leitura
        leituras = {l.get("tag"): l for l in telemetria.get("leituras", [])}
        self._ultimo_nivel = float(leituras.get("LT-01", {}).get("valor", 0.0) or 0.0)
        self._ultima_temperatura = float(leituras.get("TT-01", {}).get("valor", 0.0) or 0.0)

        # Conjunto dos códigos ativos neste ciclo
        codigos_novos: Set[str] = set()
        mapa_alarmes: dict[str, dict] = {}
        for a in alarmes_lista:
            codigo = a.get("codigo", "")
            if codigo:
                codigos_novos.add(codigo)
                mapa_alarmes[codigo] = a

        # Bordas de SUBIDA: alarmes que acabaram de ativar
        ativados = codigos_novos - self._codigos_ativos
        for codigo in sorted(ativados):
            info = mapa_alarmes[codigo]
            evento = EventoAlarme(
                codigo=codigo,
                severidade=info.get("severidade", "aviso"),
                mensagem=info.get("mensagem", ""),
                transicao="ativacao",
                nivel=self._ultimo_nivel,
                temperatura=self._ultima_temperatura,
            )
            self._notificar_todos_ativacao(evento)

        # Bordas de DESCIDA: alarmes que acabaram de normalizar
        normalizados = self._codigos_ativos - codigos_novos
        for codigo in sorted(normalizados):
            evento = EventoAlarme(
                codigo=codigo,
                severidade="info",
                mensagem=f"Alarme {codigo} normalizado",
                transicao="normalizacao",
                nivel=self._ultimo_nivel,
                temperatura=self._ultima_temperatura,
            )
            self._notificar_todos_normalizacao(evento)

        self._codigos_ativos = codigos_novos

    def alarmes_ativos(self) -> Set[str]:
        """Retorna o conjunto de códigos de alarmes atualmente ativos."""
        return set(self._codigos_ativos)

    def resetar(self) -> None:
        """Limpa o estado de alarmes (nova sessão ou reset manual)."""
        self._codigos_ativos.clear()

    # ------------------------------------------------------------------
    # Notificação interna
    # ------------------------------------------------------------------

    def _notificar_todos_ativacao(self, evento: EventoAlarme) -> None:
        for obs in self._observadores:
            try:
                obs.notificar_ativacao(evento)
            except Exception:
                pass  # Falha de um observador não bloqueia os demais

    def _notificar_todos_normalizacao(self, evento: EventoAlarme) -> None:
        for obs in self._observadores:
            try:
                obs.notificar_normalizacao(evento)
            except Exception:
                pass
