"""Envia uma solicitação de operador para o controlador C++ via CLI."""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from planta.simulador import numero


def _carregar_atual(caminho: Path) -> dict:
    try:
        dados = json.loads(caminho.read_text(encoding="utf-8"))
        if isinstance(dados, dict):
            return dados
    except Exception:
        pass
    return {}


def _gravar_atomico(caminho: Path, payload: dict) -> None:
    caminho.parent.mkdir(parents=True, exist_ok=True)
    conteudo = json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    fd, tmp = tempfile.mkstemp(dir=caminho.parent, prefix=".solicitacao.", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as arquivo:
            arquivo.write(conteudo)
        os.replace(tmp, caminho)
    except Exception:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dados", type=Path, default=Path("dados"))
    parser.add_argument("--estrategia", choices=["histerese", "modulada"], default="histerese")
    parser.add_argument("--nivel-min", type=float, default=40.0)
    parser.add_argument("--nivel-max", type=float, default=70.0)
    parser.add_argument("--temp-min", type=float, default=30.0)
    parser.add_argument("--temp-max", type=float, default=50.0)
    args = parser.parse_args()

    if not (numero(args.nivel_min, 0, 100) and numero(args.nivel_max, 0, 100)
            and args.nivel_min < args.nivel_max):
        parser.error("faixa de nivel invalida: exige 0 <= nivel-min < nivel-max <= 100")
    if not (numero(args.temp_min, 0, 100) and numero(args.temp_max, 0, 100)
            and args.temp_min < args.temp_max):
        parser.error("faixa de temperatura invalida: exige 0 <= temp-min < temp-max <= 100")

    destino = args.dados / "solicitacao_operador.json"
    atual = _carregar_atual(destino)
    proximo_id = int(atual.get("id", 0)) + 1
    alarmes = atual.get("alarmes", {
        "nivel_muito_baixo": {"habilitado": True, "limite": 15.0},
        "nivel_muito_alto": {"habilitado": True, "limite": 85.0},
        "temp_muito_baixa": {"habilitado": True, "limite": 15.0},
        "temp_muito_alta": {"habilitado": True, "limite": 75.0},
    })

    payload = {
        "versao": 1,
        "id": proximo_id,
        "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "estrategia": args.estrategia,
        "faixas": {
            "nivel": {"min": args.nivel_min, "max": args.nivel_max},
            "temperatura": {"min": args.temp_min, "max": args.temp_max},
        },
        "alarmes": alarmes,
    }
    _gravar_atomico(destino, payload)
    print(json.dumps(payload, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
