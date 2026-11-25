# Relatório de Status do Projeto

**Data:** 2026-11-25  
**Branch atual:** `projeto/03-supervisorio` (`ETAPA_ATUAL = 03`)  
**Último commit:** `8cf7b76` (`feat: progresso parcial antes do limite de cota`)

---

## 1. Avanço até o Momento

Todas as etapas planejadas do projeto (**00 a 03**) foram implementadas e validadas com sucesso na suíte automatizada.

### Etapa 00 — Infraestrutura (`projeto/00-ambiente`)
- Estrutura de build (`Makefile`), compilação C++17 (`-Wall -Wextra -Werror -pedantic`), simulador Python da planta (`planta/simulador.py`) e documentação base em `docs/`.

### Etapa 01 — Aquisição e Telemetria C++ (`projeto/01-aquisicao`)
- Modelos de domínio e validação em `controlador/dominio/`:
  - `Medicao`, `Atuacao`, `Alarme`, `Aquisicao` (leitura e validação estrita de `dados/planta.json`) e `Telemetria` (montagem de `dados/telemetria.json`).
- Escrita atômica em `controlador/servicos/publicador_json.cpp` (`PublicadorJson::publicarAtomico`).
- Testes autorais C++: `tests/cpp/autorais_medicao.cpp` e `tests/cpp/autorais_aquisicao.cpp`.

### Etapa 02 — Controle, Strategy, Command e Segurança C++ (`projeto/02-controle`)
- **Padrão Strategy (`controlador/estrategias/`):**
  - Interface `IEstrategiaControle`, structs `ConfiguracaoControle` e `DecisaoAtuacao`.
  - `EstrategiaFaixaHisterese` (Estratégia A — bang-bang com preservação de estado na banda morta).
  - `EstrategiaProporcionalModulada` (Estratégia B — atuação contínua proporcional ao desvio do centro da faixa).
- **Padrão Command (`controlador/comandos/`):**
  - Interface `IComando` com rastreamento por `id`, `sessao`, `foiConfirmado()` e `gerarJson()`.
  - `ComandoAtuacao` (comando normal com ID incremental positivo) e `ComandoSeguranca` (intertravamento prioritário).
- **Segurança e Orquestração (`controlador/servicos/`):**
  - `GerenciadorSeguranca`: avaliação dos 4 alarmes críticos com histerese de rearme (`NIVEL_MUITO_BAIXO` ≤ 15% / rearme 20%, `NIVEL_MUITO_ALTO` ≥ 85% / rearme 80%, `TEMP_MUITO_BAIXA` ≤ 15°C / rearme 18°C, `TEMP_MUITO_ALTA` ≥ 75°C / rearme 70°C) e intertravamentos.
  - `ValidadorSolicitacao`: leitura e validação de `dados/solicitacao_operador.json`, preservando a última configuração válida em caso de erro ou ausência.
  - `ControladorProcesso` e `controlador/main.cpp`: suporte a `--uma-vez` e modo contínuo (`--intervalo`), idempotência para o mesmo ciclo, reconciliação de sessão e hierarquia de decisão (Intertravamento > Fallback > Estratégia).
- Testes autorais C++: `tests/cpp/autorais_estrategias.cpp`, `tests/cpp/autorais_comandos.cpp` e `tests/cpp/autorais_seguranca.cpp`.

### Etapa 03 — Supervisório Python/Streamlit (`projeto/03-supervisorio`)
- **Padrão Repository (`supervisor/repositorio_historico.py`):**
  - `RepositorioHistorico` e `RegistroHistorico` com persistência em SQLite (`dados/historico.db`), deduplicação via `PRIMARY KEY (sessao, ciclo)` + `INSERT OR IGNORE`, consulta dos últimos registros e exportação CSV.
- **Padrão Observer (`supervisor/observador_base.py`, `monitor_alarmes.py`, `notificador_tela.py`, `notificador_email.py`):**
  - `MonitorAlarmes` (Subject) disparando notificações estritamente nas transições de estado (borda de subida/descida, evitando spam).
  - `NotificadorTela` (buffer circular para a interface) e `NotificadorEmail` (modo Real via SMTP/TLS e modo Simulação/Mock gravando em `dados/notificacoes_email.log`).
- **Interface Streamlit e Autenticação (`supervisor/app.py`, `supervisor/autenticacao.py`):**
  - Painel de monitoramento em tempo real (nível, temperatura, faixas ativas, estratégia, atuadores, idade da medição e aviso de obsolescência).
  - Gráficos temporais com linhas de referência, painel de alarmes e transições, histórico com download CSV, configuração de faixas/estratégia com gravação atômica de `dados/solicitacao_operador.json` e painel administrativo protegido pela senha `adm123`.
- **Ambiente e Testes Python:**
  - Dependências de `requirements-dev.txt` (`streamlit`, `coverage`, `gcovr`, `pandas`, etc.) instaladas na `.venv` via `uv`.
  - Testes autorais Python em `tests/autorais_supervisorio.py` (24 casos de teste).

---

## 2. Resultados da Última Validação

Executado em `2026-11-25` na branch `projeto/03-supervisorio`:

- **Testes completos (`make test ETAPA=03 PYTHON=.venv/bin/python`):**
  - **43 testes executados — TODOS PASSARAM (`OK`)**:
    - 5 executáveis autorais C++ (`autorais_aquisicao`, `autorais_comandos`, `autorais_estrategias`, `autorais_medicao`, `autorais_seguranca`)
    - 12 testes da planta (`tests/test_planta.py`)
    - 5 testes do controlador (`tests/test_controlador.py`)
    - 2 testes da interface Streamlit (`tests/test_supervisor.py` via `AppTest`)
    - 24 testes autorais Python (`tests/autorais_supervisorio.py`)
- **Cobertura (`make cobertura ETAPA=03 PYTHON=.venv/bin/python`):**
  - **C++ (`gcovr`):** `70.3%` de linhas (`471/670`), `94.6%` de funções (`70/74`). Relatório em `build/relatorios/cpp/index.html`.
  - **Python (`coverage`):** `69%` total (`720` statements). Relatório em `build/relatorios/python/index.html`.

---

## 3. Próximos Passos

1. **Commitar este relatório e consolidar as branches (PRs para `main`):**
   - Abrir/finalizar os Pull Requests das branches `projeto/01-aquisicao`, `projeto/02-controle` e `projeto/03-supervisorio` para `main` no fork, mantendo o histórico limpo conforme orientado em `README.md`.
2. **Teste de integração ponta a ponta (3 terminais simultâneos):**
   - Executar os três processos juntos e validar visualmente a troca de snapshots, gráficos e notificações:
     - Terminal 1: `make planta PYTHON=.venv/bin/python`
     - Terminal 2: `./build/controlador --dados dados`
     - Terminal 3: `make supervisor PYTHON=.venv/bin/python`
3. **Aumentar cobertura (opcional/recomendado antes da entrega):**
   - Elevar a cobertura de testes em `supervisor/app.py` (atualmente em `53%`, testando fluxos dos formulários de configuração/admin via `AppTest`) e em `controlador/servicos/controlador_processo.cpp` / `validador_solicitacao.cpp` (cenários de erro e leitura de `solicitacao_operador.json`).
4. **Revisão para apresentação / entrevista:**
   - Revisar `docs/GUIA_ENTREVISTA.md`, `docs/UML.md` e `docs/diagrama_classes.svg` para garantir alinhamento total com os arquivos finais implementados..
