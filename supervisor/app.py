"""Supervisório — Interface IHM Streamlit (SupervisorioApp).

Padrões de Projeto aplicados:
  - Repository : RepositorioHistorico (SQLite desacoplado da IHM)
  - Observer   : MonitorAlarmes → NotificadorTela + NotificadorEmail

Responsabilidade Única:
  Renderizar a interface gráfica, ler telemetria.json, persistir histórico
  e publicar solicitacoes_operador.json atomicamente.
  Não decide controle nem escreve diretamente em comando_planta.json.
"""

from __future__ import annotations

import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

import streamlit as st

from supervisor.autenticacao import verificar_senha
from supervisor.monitor_alarmes import MonitorAlarmes
from supervisor.notificador_email import NotificadorEmail
from supervisor.notificador_tela import NotificadorTela
from supervisor.repositorio_historico import RegistroHistorico, RepositorioHistorico

# ---------------------------------------------------------------------------
# Constantes
# ---------------------------------------------------------------------------
OBSOLESCENCIA_SEGUNDOS = 10.0  # snapshot > 10s é considerado obsoleto
DIR_DADOS = Path(os.environ.get("POO_DADOS", "dados"))
BANCO = DIR_DADOS / "historico.db"
TELEMETRIA = DIR_DADOS / "telemetria.json"
SOLICITACAO = DIR_DADOS / "solicitacao_operador.json"


# ---------------------------------------------------------------------------
# Inicialização do estado da sessão Streamlit (executado uma única vez)
# ---------------------------------------------------------------------------

def _inicializar_estado() -> None:
    """Cria objetos persistentes no st.session_state entre re-renders."""
    if "repo" not in st.session_state:
        repo = RepositorioHistorico(BANCO)
        repo.inicializar_banco()
        st.session_state["repo"] = repo

    if "monitor" not in st.session_state:
        monitor = MonitorAlarmes()
        notif_tela = NotificadorTela(max_eventos=50)
        st.session_state["notif_tela"] = notif_tela
        monitor.registrar_observador(notif_tela)
        st.session_state["monitor"] = monitor
        st.session_state["notif_email"] = None  # configurado no painel admin

    if "proximo_id_solicitacao" not in st.session_state:
        st.session_state["proximo_id_solicitacao"] = _carregar_id_solicitacao()

    if "admin_autenticado" not in st.session_state:
        st.session_state["admin_autenticado"] = False


def _carregar_id_solicitacao() -> int:
    """Carrega o ID mais recente de solicitação do arquivo (se existir)."""
    try:
        dados = json.loads(SOLICITACAO.read_text(encoding="utf-8"))
        return int(dados.get("id", 0)) + 1
    except Exception:
        return 1


# ---------------------------------------------------------------------------
# Leitura da telemetria
# ---------------------------------------------------------------------------

def _ler_telemetria() -> Optional[dict]:
    """Lê e valida o arquivo telemetria.json. Retorna None se inválido."""
    try:
        conteudo = TELEMETRIA.read_text(encoding="utf-8")
        t = json.loads(conteudo)
        if not isinstance(t, dict):
            return None
        if t.get("versao") != 1:
            return None
        return t
    except Exception:
        return None


def _idade_telemetria(telemetria: Optional[dict]) -> Optional[float]:
    """Retorna a idade em segundos do snapshot, ou None se inválido."""
    if not telemetria:
        return None
    try:
        ts = telemetria.get("timestamp", "")
        dt = datetime.strptime(ts, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        return (datetime.now(timezone.utc) - dt).total_seconds()
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Publicação atômica de solicitacao_operador.json
# ---------------------------------------------------------------------------

def _publicar_solicitacao(payload: dict) -> bool:
    """Grava solicitacao_operador.json atomicamente (tmp → rename)."""
    try:
        DIR_DADOS.mkdir(parents=True, exist_ok=True)
        conteudo = json.dumps(payload, ensure_ascii=False, indent=2)
        fd, caminho_tmp = tempfile.mkstemp(
            dir=DIR_DADOS, prefix=".solicitacao.", suffix=".tmp"
        )
        try:
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                f.write(conteudo)
                f.write("\n")
            os.replace(caminho_tmp, SOLICITACAO)
        except Exception:
            try:
                os.unlink(caminho_tmp)
            except OSError:
                pass
            raise
        return True
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Validação do formulário de configuração
# ---------------------------------------------------------------------------

def _validar_configuracao(
    nivel_min: float,
    nivel_max: float,
    temp_min: float,
    temp_max: float,
    alarmes: dict,
) -> list[str]:
    """Retorna lista de erros de validação (vazia = tudo OK)."""
    erros = []
    if not (0.0 <= nivel_min < nivel_max <= 100.0):
        erros.append("Faixa de Nível inválida: exige 0 ≤ min < max ≤ 100.")
    if not (0.0 <= temp_min < temp_max <= 100.0):
        erros.append("Faixa de Temperatura inválida: exige 0 ≤ min < max ≤ 100.")
    lnb = alarmes["nivel_muito_baixo"]["limite"]
    lna = alarmes["nivel_muito_alto"]["limite"]
    ltb = alarmes["temp_muito_baixa"]["limite"]
    lta = alarmes["temp_muito_alta"]["limite"]
    if lnb >= lna:
        erros.append("Limite nivel_baixo deve ser menor que limite nivel_alto.")
    if ltb >= lta:
        erros.append("Limite temp_baixa deve ser menor que limite temp_alta.")
    return erros


def _ler_solicitacao_atual() -> dict:
    """Lê solicitacao_operador.json ou retorna configuração padrão."""
    padrao = {
        "estrategia": "histerese",
        "faixas": {
            "nivel": {"min": 40.0, "max": 70.0},
            "temperatura": {"min": 30.0, "max": 50.0},
        },
        "alarmes": {
            "nivel_muito_baixo": {"habilitado": True, "limite": 15.0},
            "nivel_muito_alto": {"habilitado": True, "limite": 85.0},
            "temp_muito_baixa": {"habilitado": True, "limite": 15.0},
            "temp_muito_alta": {"habilitado": True, "limite": 75.0},
        },
    }
    try:
        dados = json.loads(SOLICITACAO.read_text(encoding="utf-8"))
        if isinstance(dados, dict):
            return {
                "estrategia": dados.get("estrategia", padrao["estrategia"]),
                "faixas": dados.get("faixas", padrao["faixas"]),
                "alarmes": dados.get("alarmes", padrao["alarmes"]),
            }
    except Exception:
        pass
    return padrao


# ---------------------------------------------------------------------------
# Componentes de UI
# ---------------------------------------------------------------------------

def _renderizar_metricas(telemetria: dict, idade: float, cfg_atual: dict) -> None:
    """Painel de métricas principais, estratégia, faixas ativas e leituras."""
    leituras_lista = telemetria.get("leituras", [])
    leituras = {l["tag"]: l for l in leituras_lista if isinstance(l, dict) and "tag" in l}
    nivel = leituras.get("LT-01", {}).get("valor", 0.0) or 0.0
    temperatura = leituras.get("TT-01", {}).get("valor", 0.0) or 0.0
    atuadores = telemetria.get("atuadores", {})
    bomba = atuadores.get("bomba", 0.0)
    aquecedor = atuadores.get("aquecedor", 0.0)

    faixas = cfg_atual.get("faixas", {})
    n_cfg = faixas.get("nivel", {"min": 40.0, "max": 70.0})
    t_cfg = faixas.get("temperatura", {"min": 30.0, "max": 50.0})
    estrategia = cfg_atual.get("estrategia", "histerese")

    cor_idade = "normal" if idade < OBSOLESCENCIA_SEGUNDOS else "inverse"
    col1, col2, col3, col4, col5 = st.columns(5)
    col1.metric("💧 Nível", f"{nivel:.1f}%", f"Faixa {n_cfg['min']:.0f}–{n_cfg['max']:.0f}%")
    col2.metric("🌡️ Temperatura", f"{temperatura:.1f}°C", f"Faixa {t_cfg['min']:.0f}–{t_cfg['max']:.0f}°C")
    col3.metric("⚙️ Bomba", f"{bomba:.1f}%")
    col4.metric("🔥 Aquecedor", f"{aquecedor:.1f}%")
    col5.metric("⏱️ Idade", f"{idade:.0f}s", delta_color=cor_idade)

    st.caption(
        f"🎛️ Estratégia ativa: **{estrategia}** | "
        f"Ciclo: **{telemetria.get('ciclo', '—')}** | "
        f"Último comando confirmado: **{telemetria.get('ultimo_comando', '—')}**"
    )

    if telemetria.get("motivo_seguro"):
        st.error(f"🛡️ **Modo Seguro Ativo:** {telemetria['motivo_seguro']}")
    if telemetria.get("erro_comando"):
        st.warning(f"⚠️ **Erro no último comando:** {telemetria['erro_comando']}")

    if idade >= OBSOLESCENCIA_SEGUNDOS:
        st.warning(
            f"⚠️ **AQUISIÇÃO OBSOLETA** — snapshot tem {idade:.0f}s "
            f"(limite: {OBSOLESCENCIA_SEGUNDOS:.0f}s). "
            "Verifique se o controlador C++ está em execução.",
            icon="🚨",
        )

    if leituras_lista:
        st.subheader("📡 Leituras dos Sensores")
        st.dataframe(leituras_lista, use_container_width=True)


def _renderizar_alarmes(telemetria: dict, notif_tela: NotificadorTela) -> None:
    """Painel de alarmes ativos e histórico de eventos."""
    alarmes = telemetria.get("alarmes", [])
    st.subheader("🚨 Alarmes e Diagnósticos")

    if alarmes:
        for a in alarmes:
            sev = a.get("severidade", "aviso")
            icone = "🔴" if sev == "critico" else "🟡"
            st.error(
                f"{icone} **{a.get('codigo')}** — {a.get('mensagem')} "
                f"[{sev.upper()}]"
            )
    else:
        st.success("✅ Nenhum alarme ativo no momento.")

    eventos = notif_tela.obter_eventos_recentes()
    if eventos:
        with st.expander(f"📋 Histórico de Transições ({len(eventos)} eventos)", expanded=False):
            for ev in eventos:
                icone = "🔴" if ev.eh_ativacao() else "🟢"
                st.write(
                    f"{icone} `{ev.timestamp}` **{ev.codigo}** "
                    f"({ev.transicao.upper()}) — {ev.mensagem}"
                )


def _renderizar_graficos(historico: list, cfg_atual: dict) -> None:
    """Gráficos temporais de Nível e Temperatura com linhas de referência."""
    if not historico:
        st.info("📊 Sem dados históricos para exibir. Aguardando primeiros ciclos…")
        return

    faixas = cfg_atual.get("faixas", {})
    n_cfg = faixas.get("nivel", {"min": 40.0, "max": 70.0})
    t_cfg = faixas.get("temperatura", {"min": 30.0, "max": 50.0})
    alarmes_cfg = cfg_atual.get("alarmes", {})

    # Tenta importar pandas (melhor experiência), cai em fallback simples
    try:
        import pandas as pd

        dados_nivel = {
            "ciclo": [r.ciclo for r in historico],
            "Nível (%)": [r.nivel for r in historico],
            "Faixa Mín (%)": [float(n_cfg["min"])] * len(historico),
            "Faixa Máx (%)": [float(n_cfg["max"])] * len(historico),
        }
        if alarmes_cfg.get("nivel_muito_baixo", {}).get("habilitado", True):
            dados_nivel["Alarme Baixo (%)"] = [
                float(alarmes_cfg.get("nivel_muito_baixo", {}).get("limite", 15.0))
            ] * len(historico)
        if alarmes_cfg.get("nivel_muito_alto", {}).get("habilitado", True):
            dados_nivel["Alarme Alto (%)"] = [
                float(alarmes_cfg.get("nivel_muito_alto", {}).get("limite", 85.0))
            ] * len(historico)

        dados_temp = {
            "ciclo": [r.ciclo for r in historico],
            "Temperatura (°C)": [r.temperatura for r in historico],
            "Faixa Mín (°C)": [float(t_cfg["min"])] * len(historico),
            "Faixa Máx (°C)": [float(t_cfg["max"])] * len(historico),
        }
        if alarmes_cfg.get("temp_muito_baixa", {}).get("habilitado", True):
            dados_temp["Alarme Baixa (°C)"] = [
                float(alarmes_cfg.get("temp_muito_baixa", {}).get("limite", 15.0))
            ] * len(historico)
        if alarmes_cfg.get("temp_muito_alta", {}).get("habilitado", True):
            dados_temp["Alarme Alta (°C)"] = [
                float(alarmes_cfg.get("temp_muito_alta", {}).get("limite", 75.0))
            ] * len(historico)

        df_nivel = pd.DataFrame(dados_nivel).set_index("ciclo")
        df_temp = pd.DataFrame(dados_temp).set_index("ciclo")
        df_atuadores = pd.DataFrame({
            "ciclo": [r.ciclo for r in historico],
            "Bomba (%)": [r.bomba for r in historico],
            "Aquecedor (%)": [r.aquecedor for r in historico],
        }).set_index("ciclo")

        col1, col2 = st.columns(2)
        with col1:
            st.subheader("📈 Nível do Reservatório")
            st.line_chart(df_nivel)
        with col2:
            st.subheader("🌡️ Temperatura")
            st.line_chart(df_temp)

        with st.expander("⚙️ Atuadores", expanded=False):
            st.line_chart(df_atuadores)

    except ImportError:
        # Sem pandas: usa dado bruto do Streamlit
        niveis = [r.nivel for r in historico]
        temperaturas = [r.temperatura for r in historico]
        col1, col2 = st.columns(2)
        with col1:
            st.subheader("📈 Nível (%)")
            st.line_chart({"Nível": niveis})
        with col2:
            st.subheader("🌡️ Temperatura (°C)")
            st.line_chart({"Temperatura": temperaturas})


def _renderizar_configuracao(telemetria: Optional[dict]) -> None:
    """Painel de configuração de faixas e estratégia."""
    st.subheader("⚙️ Configuração do Controlador")

    # Carrega configuração atual (se existir)
    cfg_atual = {}
    try:
        cfg_atual = json.loads(SOLICITACAO.read_text(encoding="utf-8"))
    except Exception:
        pass

    faixas = cfg_atual.get("faixas", {})
    nivel_cfg = faixas.get("nivel", {"min": 40.0, "max": 70.0})
    temp_cfg = faixas.get("temperatura", {"min": 30.0, "max": 50.0})
    estrategia_atual = cfg_atual.get("estrategia", "histerese")

    with st.form("form_configuracao"):
        st.write("**Faixas Operacionais**")
        col1, col2 = st.columns(2)
        with col1:
            nivel_min = st.number_input(
                "Nível Mín (%)", 0.0, 99.0, float(nivel_cfg["min"]), 1.0
            )
            nivel_max = st.number_input(
                "Nível Máx (%)", 1.0, 100.0, float(nivel_cfg["max"]), 1.0
            )
        with col2:
            temp_min = st.number_input(
                "Temp Mín (°C)", 0.0, 99.0, float(temp_cfg["min"]), 1.0
            )
            temp_max = st.number_input(
                "Temp Máx (°C)", 1.0, 100.0, float(temp_cfg["max"]), 1.0
            )

        estrategia = st.radio(
            "Estratégia de Controle",
            ["histerese", "modulada"],
            index=0 if estrategia_atual == "histerese" else 1,
            horizontal=True,
        )

        # Alarmes padrão (mantém da última solicitação se existir)
        alarmes_cfg = cfg_atual.get("alarmes", {
            "nivel_muito_baixo": {"habilitado": True, "limite": 15.0},
            "nivel_muito_alto": {"habilitado": True, "limite": 85.0},
            "temp_muito_baixa": {"habilitado": True, "limite": 15.0},
            "temp_muito_alta": {"habilitado": True, "limite": 75.0},
        })

        enviado = st.form_submit_button("📤 Enviar Configuração ao Controlador")

    if enviado:
        erros = _validar_configuracao(nivel_min, nivel_max, temp_min, temp_max, alarmes_cfg)
        if erros:
            for e in erros:
                st.error(f"❌ {e}")
        else:
            prox_id = st.session_state["proximo_id_solicitacao"]
            payload = {
                "versao": 1,
                "id": prox_id,
                "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "estrategia": estrategia,
                "faixas": {
                    "nivel": {"min": nivel_min, "max": nivel_max},
                    "temperatura": {"min": temp_min, "max": temp_max},
                },
                "alarmes": alarmes_cfg,
            }
            if _publicar_solicitacao(payload):
                st.session_state["proximo_id_solicitacao"] = prox_id + 1
                st.success(
                    f"✅ Solicitação #{prox_id} enviada! "
                    f"Estratégia: **{estrategia}** | "
                    f"Nível: [{nivel_min}%, {nivel_max}%] | "
                    f"Temperatura: [{temp_min}°C, {temp_max}°C]"
                )
            else:
                st.error("❌ Falha ao gravar solicitacao_operador.json.")


def _renderizar_painel_admin() -> None:
    """Painel administrativo protegido por senha para alarmes e e-mail."""
    st.subheader("🔐 Painel de Administração")

    if not st.session_state["admin_autenticado"]:
        with st.form("form_login"):
            senha = st.text_input("Senha de Administrador", type="password")
            login = st.form_submit_button("Entrar")
        if login:
            if verificar_senha(senha):
                st.session_state["admin_autenticado"] = True
                st.rerun()
            else:
                st.error("❌ Senha incorreta.")
        return

    st.success("🔓 Autenticado como Administrador.")
    if st.button("Sair da sessão admin"):
        st.session_state["admin_autenticado"] = False
        st.rerun()

    # Carrega config atual
    cfg_atual = {}
    try:
        cfg_atual = json.loads(SOLICITACAO.read_text(encoding="utf-8"))
    except Exception:
        pass
    alarmes_cfg = cfg_atual.get("alarmes", {
        "nivel_muito_baixo": {"habilitado": True, "limite": 15.0},
        "nivel_muito_alto": {"habilitado": True, "limite": 85.0},
        "temp_muito_baixa": {"habilitado": True, "limite": 15.0},
        "temp_muito_alta": {"habilitado": True, "limite": 75.0},
    })

    st.write("**🚨 Configuração de Alarmes Críticos**")
    with st.form("form_alarmes_admin"):
        col1, col2 = st.columns(2)
        alarmes_novos = {}

        with col1:
            st.write("*Nível muito baixo*")
            nmb_hab = st.checkbox(
                "Habilitado",
                value=alarmes_cfg["nivel_muito_baixo"]["habilitado"],
                key="nmb_hab",
            )
            nmb_lim = st.number_input(
                "Limite (%)", 0.0, 50.0,
                float(alarmes_cfg["nivel_muito_baixo"]["limite"]), 0.5, key="nmb_lim"
            )
            alarmes_novos["nivel_muito_baixo"] = {"habilitado": nmb_hab, "limite": nmb_lim}

            st.write("*Temperatura muito baixa*")
            tmb_hab = st.checkbox(
                "Habilitado",
                value=alarmes_cfg["temp_muito_baixa"]["habilitado"],
                key="tmb_hab",
            )
            tmb_lim = st.number_input(
                "Limite (°C)", 0.0, 50.0,
                float(alarmes_cfg["temp_muito_baixa"]["limite"]), 0.5, key="tmb_lim"
            )
            alarmes_novos["temp_muito_baixa"] = {"habilitado": tmb_hab, "limite": tmb_lim}

        with col2:
            st.write("*Nível muito alto*")
            nma_hab = st.checkbox(
                "Habilitado",
                value=alarmes_cfg["nivel_muito_alto"]["habilitado"],
                key="nma_hab",
            )
            nma_lim = st.number_input(
                "Limite (%)", 50.0, 100.0,
                float(alarmes_cfg["nivel_muito_alto"]["limite"]), 0.5, key="nma_lim"
            )
            alarmes_novos["nivel_muito_alto"] = {"habilitado": nma_hab, "limite": nma_lim}

            st.write("*Temperatura muito alta*")
            tma_hab = st.checkbox(
                "Habilitado",
                value=alarmes_cfg["temp_muito_alta"]["habilitado"],
                key="tma_hab",
            )
            tma_lim = st.number_input(
                "Limite (°C)", 50.0, 100.0,
                float(alarmes_cfg["temp_muito_alta"]["limite"]), 0.5, key="tma_lim"
            )
            alarmes_novos["temp_muito_alta"] = {"habilitado": tma_hab, "limite": tma_lim}

        enviou_alarmes = st.form_submit_button("💾 Salvar Configuração de Alarmes")

    if enviou_alarmes:
        erros = _validar_configuracao(
            cfg_atual.get("faixas", {}).get("nivel", {}).get("min", 40.0),
            cfg_atual.get("faixas", {}).get("nivel", {}).get("max", 70.0),
            cfg_atual.get("faixas", {}).get("temperatura", {}).get("min", 30.0),
            cfg_atual.get("faixas", {}).get("temperatura", {}).get("max", 50.0),
            alarmes_novos,
        )
        if erros:
            for e in erros:
                st.error(f"❌ {e}")
        else:
            prox_id = st.session_state["proximo_id_solicitacao"]
            faixas = cfg_atual.get("faixas", {
                "nivel": {"min": 40.0, "max": 70.0},
                "temperatura": {"min": 30.0, "max": 50.0},
            })
            payload = {
                "versao": 1,
                "id": prox_id,
                "timestamp": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "estrategia": cfg_atual.get("estrategia", "histerese"),
                "faixas": faixas,
                "alarmes": alarmes_novos,
            }
            if _publicar_solicitacao(payload):
                st.session_state["proximo_id_solicitacao"] = prox_id + 1
                st.success(f"✅ Alarmes atualizados (solicitação #{prox_id}).")
            else:
                st.error("❌ Falha ao gravar solicitacao_operador.json.")

    # Configuração de e-mail
    st.write("---")
    st.write("**📧 Configuração de Notificações por E-mail**")
    with st.form("form_email"):
        destinatario = st.text_input("E-mail de destino", placeholder="operador@empresa.com")
        col1, col2 = st.columns(2)
        with col1:
            smtp_host = st.text_input("Servidor SMTP", value="smtp.gmail.com")
            smtp_porta = st.number_input("Porta SMTP", min_value=1, max_value=65535, value=587, step=1)
            smtp_usuario = st.text_input("Usuário SMTP")
        with col2:
            smtp_senha = st.text_input("Senha / Token", type="password")
            modo_sim = st.checkbox("Modo Simulação (Mock)", value=True)

        salvar_email = st.form_submit_button("💾 Aplicar Configuração de E-mail")

    if salvar_email:
        notif_email = NotificadorEmail(
            destinatario=destinatario,
            smtp_host=smtp_host,
            smtp_porta=int(smtp_porta),
            smtp_usuario=smtp_usuario,
            smtp_senha=smtp_senha,
            modo_simulacao=modo_sim,
            caminho_log=DIR_DADOS / "notificacoes_email.log",
        )
        monitor: MonitorAlarmes = st.session_state["monitor"]
        # Remove notificador de e-mail antigo se existir
        antigo = st.session_state.get("notif_email")
        if antigo:
            monitor.remover_observador(antigo)
        monitor.registrar_observador(notif_email)
        st.session_state["notif_email"] = notif_email
        modo_str = "Simulação (Mock)" if modo_sim else "Real (SMTP)"
        st.success(f"✅ Notificador de e-mail configurado em modo **{modo_str}**.")
        if modo_sim and destinatario:
            st.info(f"📄 Logs gravados em: `{DIR_DADOS / 'notificacoes_email.log'}`")


# ---------------------------------------------------------------------------
# Ponto de entrada principal
# ---------------------------------------------------------------------------

def main() -> None:
    st.set_page_config(
        page_title="Supervisório — Reservatório Térmico",
        page_icon="🏭",
        layout="wide",
        initial_sidebar_state="expanded",
    )
    st.title("🏭 Supervisório — Reservatório Térmico")

    _inicializar_estado()

    repo: RepositorioHistorico = st.session_state["repo"]
    monitor: MonitorAlarmes = st.session_state["monitor"]
    notif_tela: NotificadorTela = st.session_state["notif_tela"]

    # ------------------------------------------------------------------
    # Sidebar — info e download
    # ------------------------------------------------------------------
    with st.sidebar:
        st.header("📡 Status do Sistema")
        telemetria = _ler_telemetria()
        if telemetria:
            idade = _idade_telemetria(telemetria) or 0.0
            st.metric("Ciclo", telemetria.get("ciclo", "—"))
            st.metric("Sessão", str(telemetria.get("sessao", "—"))[:8] + "…")
            st.metric("Idade", f"{idade:.0f}s")
            if idade >= OBSOLESCENCIA_SEGUNDOS:
                st.error("⚠️ DADO OBSOLETO")
            else:
                st.success("✅ Dados Atualizados")
        else:
            st.error("❌ telemetria.json não encontrado ou inválido.")
            st.info("Inicie o controlador C++ com:\n```\nmake controlador\n```")

        st.divider()
        st.header("📥 Exportar Histórico")
        if st.button("📊 Gerar CSV"):
            csv_bytes = repo.exportar_csv_bytes()
            st.download_button(
                label="⬇️ Baixar historico.csv",
                data=csv_bytes,
                file_name="historico.csv",
                mime="text/csv",
            )

        st.divider()
        atualizar_auto = st.checkbox("🔄 Atualização automática (5s)", value=True)
        if st.button("🔄 Atualizar agora"):
            st.rerun()

    # ------------------------------------------------------------------
    # Área principal
    # ------------------------------------------------------------------
    if telemetria is None:
        st.warning(
            "🔌 Aguardando dados do Controlador C++…\n\n"
            "Para iniciar o sistema completo execute em terminais separados:\n"
            "```bash\nmake planta          # Terminal 1: simulador\n"
            "make controlador     # Terminal 2: controlador C++\n"
            "make supervisor      # Terminal 3: este supervisório\n```"
        )
        return

    def _ciclo_monitoramento() -> None:
        tel = _ler_telemetria()
        if tel is None:
            st.warning("Telemetria indisponível no momento.")
            return
        idade_atual = _idade_telemetria(tel) or 0.0
        cfg_atual = _ler_solicitacao_atual()
        registro = RegistroHistorico.de_telemetria(tel)
        repo.salvar_registro(registro)
        monitor.processar_telemetria(tel)
        _renderizar_metricas(tel, idade_atual, cfg_atual)
        st.divider()
        _renderizar_alarmes(tel, notif_tela)
        st.divider()
        historico = repo.obter_sessao_atual(limite=200)
        _renderizar_graficos(historico, cfg_atual)

    # ------------------------------------------------------------------
    # Renderização das seções
    # ------------------------------------------------------------------
    tab_mon, tab_hist, tab_cfg, tab_admin = st.tabs([
        "📊 Monitoramento",
        "📋 Histórico",
        "⚙️ Configuração",
        "🔐 Administração",
    ])

    with tab_mon:
        if atualizar_auto and hasattr(st, "fragment"):
            st.fragment(run_every=5)(_ciclo_monitoramento)()
        else:
            _ciclo_monitoramento()

    with tab_hist:
        st.subheader("📋 Histórico de Ciclos (Sessão Atual)")
        historico_exib = repo.obter_sessao_atual(limite=500)
        if historico_exib:
            try:
                import pandas as pd
                dados_df = [
                    {
                        "Ciclo": r.ciclo,
                        "Timestamp": r.timestamp,
                        "Nível (%)": r.nivel,
                        "Temp (°C)": r.temperatura,
                        "Bomba (%)": r.bomba,
                        "Aquecedor (%)": r.aquecedor,
                        "Último Cmd": r.ultimo_comando,
                        "Alarmes": r.alarmes,
                    }
                    for r in historico_exib
                ]
                st.dataframe(pd.DataFrame(dados_df), use_container_width=True)
            except ImportError:
                for r in historico_exib[:20]:
                    st.write(
                        f"Ciclo {r.ciclo} | Nível: {r.nivel:.1f}% "
                        f"| Temp: {r.temperatura:.1f}°C"
                    )
        else:
            st.info("Nenhum registro histórico ainda.")

        st.divider()
        csv_bytes = repo.exportar_csv_bytes()
        st.download_button(
            label="⬇️ Baixar Histórico Completo (CSV)",
            data=csv_bytes,
            file_name="historico_completo.csv",
            mime="text/csv",
        )

    with tab_cfg:
        _renderizar_configuracao(telemetria)

    with tab_admin:
        _renderizar_painel_admin()


if __name__ == "__main__":
    main()
