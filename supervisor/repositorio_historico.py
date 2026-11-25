"""Padrão Repository — persistência histórica em SQLite (dados/historico.db).

Responsabilidade Única: isolar operações SQL da interface gráfica Streamlit.
Deduplicação garantida por PRIMARY KEY (sessao, ciclo) + INSERT OR IGNORE.
"""

from __future__ import annotations

import csv
import io
import json
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


# ---------------------------------------------------------------------------
# Entidade de domínio (DTO)
# ---------------------------------------------------------------------------

@dataclass
class RegistroHistorico:
    """Representa uma linha histórica consolidada de processo."""
    sessao: str
    ciclo: int
    timestamp: str
    nivel: float
    temperatura: float
    bomba: float
    aquecedor: float
    ultimo_comando: int
    alarmes: str  # JSON serializado da lista de alarmes

    def chave(self) -> tuple:
        return (self.sessao, self.ciclo)

    @staticmethod
    def de_telemetria(t: dict) -> Optional["RegistroHistorico"]:
        """Constrói um RegistroHistorico a partir de um dict de telemetria.
        Retorna None se a telemetria for inválida ou incompleta."""
        try:
            leituras = {l["tag"]: l for l in t.get("leituras", [])}
            nivel = float(leituras["LT-01"]["valor"])
            temperatura = float(leituras["TT-01"]["valor"])
            atuadores = t.get("atuadores", {})
            alarmes_json = json.dumps(t.get("alarmes", []), ensure_ascii=False)
            return RegistroHistorico(
                sessao=str(t["sessao"]),
                ciclo=int(t["ciclo"]),
                timestamp=str(t["timestamp"]),
                nivel=nivel,
                temperatura=temperatura,
                bomba=float(atuadores.get("bomba", 0.0)),
                aquecedor=float(atuadores.get("aquecedor", 0.0)),
                ultimo_comando=int(t.get("ultimo_comando", 0)),
                alarmes=alarmes_json,
            )
        except (KeyError, TypeError, ValueError):
            return None


# ---------------------------------------------------------------------------
# Padrão Repository
# ---------------------------------------------------------------------------

class RepositorioHistorico:
    """Isola todas as operações SQLite da interface gráfica.

    Uso:
        repo = RepositorioHistorico(Path("dados/historico.db"))
        repo.inicializar_banco()
        repo.salvar_registro(RegistroHistorico.de_telemetria(telemetria))
        registros = repo.obter_ultimos(100)
        csv_bytes = repo.exportar_csv_bytes()
    """

    _SCHEMA = """
        CREATE TABLE IF NOT EXISTS historico (
            sessao          TEXT    NOT NULL,
            ciclo           INTEGER NOT NULL,
            timestamp       TEXT    NOT NULL,
            nivel           REAL    NOT NULL,
            temperatura     REAL    NOT NULL,
            bomba           REAL    NOT NULL,
            aquecedor       REAL    NOT NULL,
            ultimo_comando  INTEGER NOT NULL,
            alarmes         TEXT,
            PRIMARY KEY (sessao, ciclo)
        );
    """

    def __init__(self, caminho_banco: Path) -> None:
        self._caminho = Path(caminho_banco)
        self._caminho.parent.mkdir(parents=True, exist_ok=True)

    # ------------------------------------------------------------------
    # API pública
    # ------------------------------------------------------------------

    def inicializar_banco(self) -> None:
        """Cria a tabela se ainda não existir."""
        with self._conexao() as conn:
            conn.execute(self._SCHEMA)

    def salvar_registro(self, registro: Optional[RegistroHistorico]) -> bool:
        """Insere um registro usando INSERT OR IGNORE (deduplicação automática).
        Retorna True se inseriu, False se já existia ou se o registro é inválido."""
        if registro is None:
            return False
        try:
            with self._conexao() as conn:
                cursor = conn.execute(
                    """INSERT OR IGNORE INTO historico
                       (sessao, ciclo, timestamp, nivel, temperatura,
                        bomba, aquecedor, ultimo_comando, alarmes)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        registro.sessao,
                        registro.ciclo,
                        registro.timestamp,
                        registro.nivel,
                        registro.temperatura,
                        registro.bomba,
                        registro.aquecedor,
                        registro.ultimo_comando,
                        registro.alarmes,
                    ),
                )
                return cursor.rowcount > 0
        except sqlite3.Error:
            return False

    def obter_ultimos(self, limite: int = 200) -> List[RegistroHistorico]:
        """Retorna os registros mais recentes, ordenados por ciclo descendente."""
        try:
            with self._conexao() as conn:
                rows = conn.execute(
                    """SELECT sessao, ciclo, timestamp, nivel, temperatura,
                              bomba, aquecedor, ultimo_comando, alarmes
                       FROM historico
                       ORDER BY sessao DESC, ciclo DESC
                       LIMIT ?""",
                    (limite,),
                ).fetchall()
            return [RegistroHistorico(*row) for row in rows]
        except sqlite3.Error:
            return []

    def obter_sessao_atual(self, limite: int = 200) -> List[RegistroHistorico]:
        """Retorna os registros da sessão mais recente, ordenados cronologicamente."""
        try:
            with self._conexao() as conn:
                row_sessao = conn.execute(
                    "SELECT sessao FROM historico ORDER BY rowid DESC LIMIT 1"
                ).fetchone()
                if not row_sessao:
                    return []
                sessao = row_sessao[0]
                rows = conn.execute(
                    """SELECT sessao, ciclo, timestamp, nivel, temperatura,
                              bomba, aquecedor, ultimo_comando, alarmes
                       FROM historico
                       WHERE sessao = ?
                       ORDER BY ciclo DESC
                       LIMIT ?""",
                    (sessao, limite),
                ).fetchall()
            return [RegistroHistorico(*row) for row in reversed(rows)]
        except sqlite3.Error:
            return []

    def exportar_csv_bytes(self) -> bytes:
        """Gera os bytes de um CSV a partir de toda a tabela historico."""
        registros = self.obter_ultimos(limite=10000)
        buf = io.StringIO()
        writer = csv.writer(buf)
        writer.writerow([
            "sessao", "ciclo", "timestamp", "nivel", "temperatura",
            "bomba", "aquecedor", "ultimo_comando", "alarmes",
        ])
        for r in registros:
            writer.writerow([
                r.sessao, r.ciclo, r.timestamp, r.nivel, r.temperatura,
                r.bomba, r.aquecedor, r.ultimo_comando, r.alarmes,
            ])
        return buf.getvalue().encode("utf-8")

    # ------------------------------------------------------------------
    # Interno
    # ------------------------------------------------------------------

    def _conexao(self) -> sqlite3.Connection:
        conn = sqlite3.connect(str(self._caminho), timeout=5)
        conn.execute("PRAGMA journal_mode=WAL")  # Permite leituras simultâneas
        return conn
