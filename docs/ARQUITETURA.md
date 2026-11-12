# Guia Norteador de Arquitetura e Engenharia de Software

Este documento estabelece as diretrizes arquiteturais, os padrões de projeto (GoF), os princípios SOLID e as convenções de organização do código para o projeto **Sistema de Controle e Supervisão do Reservatório Térmico**.

---

## 1. Princípios Arquiteturais e Diretrizes Fundamentais

### 1.1. Separação Estrita de Responsabilidades (Processos Isolados)
O sistema opera distribuído em três processos sem compartilhamento de memória, comunicando-se exclusivamente por snapshots JSON no diretório `dados/`:

```text
+-----------------------+              planta.json             +-------------------------+
|   Simulador (Python)  | ───────────────────────────────────► |    Controlador (C++)    |
|   Física do Tanque    | ◄─────────────────────────────────── | Decisão, Regras, Modos  |
+-----------------------+          comando_planta.json         +-------------------------+
                                                                            │
                                                                            │ telemetria.json
                                   solicitacao_operador.json                ▼
                                 +──────────────────────────── +-------------------------+
                                 |                             |   Supervisório (Python) |
                                 +──────────────────────────── |  IHM Streamlit, SQLite, |
                                                               |   Notificações e CSV    |
                                                               +-------------------------+
```

1. **Planta (Python):** Simula física e ruído. **Não decide metas.**
2. **Controlador (C++):** Valida aquisição, decide controle, aplica intertravamentos e publica telemetria. **Não desenha telas nem forja medições.** Opera em modo autônomo (*fallback*) se o supervisório estiver fechado.
3. **Supervisório (Python/Streamlit):** Interface gráfica, histórico persistido em SQLite e notificações de alarme por e-mail. **Não recalcula controle nem escreve direto na planta.**

---

## 2. Aplicação dos Princípios SOLID

A estrutura de classes em C++ e Python segue rigorosamente os 5 princípios SOLID:

| Princípio | Aplicação Prática no Projeto | Exemplo no Código |
|---|---|---|
| **S — Single Responsibility (Responsabilidade Única)** | Cada classe tem um único motivo para mudar. Classes de dados apenas modelam; estratégias apenas calculam; gravadores apenas realizam I/O. | `Aquisicao` valida o schema de entrada; `EstrategiaFaixaHisterese` apenas computa a regra de faixa; `RepositorioHistorico` apenas interage com o banco SQLite. |
| **O — Open/Closed (Aberto para Extensão, Fechado para Modificação)** | Novas estratégias de controle (ex.: Estratégia C ou D) ou novos canais de notificação (ex.: SMS) são adicionados criando novas classes derivadas, sem modificar as classes existentes. | Adicionar uma nova política exige apenas implementar `IEstrategiaControle`, mantendo o controlador intacto. |
| **L — Liskov Substitution (Substituição de Liskov)** | Qualquer classe filha pode substituir sua interface base sem quebrar o comportamento do chamador. | O controlador invoca `calcularAtuacao()` sem precisar saber se a instância em execução é `EstrategiaFaixaHisterese` ou `EstrategiaProporcionalModulada`. |
| **I — Interface Segregation (Segregação de Interfaces)** | Interfaces pequenas e especializadas, evitando contratos inflados com métodos desnecessários. | A interface `IEstrategiaControle` expõe apenas métodos de decisão; `IComando` expõe apenas métodos de execução e confirmação. |
| **D — Dependency Inversion (Inversão de Dependência)** | Módulos de alto nível dependem de abstrações (interfaces puras), nunca de implementações concretas diretas. | O loop do controlador recebe um ponteiro/referência para `IEstrategiaControle` e `IComando`. |

---

## 3. Padrões de Projeto (GoF) Implementados

### 3.1. Padrão Strategy (No Controlador C++: *Como Calcular?*)
* **Objetivo:** Isolar e intercambiar em tempo de execução os algoritmos de controle sobre o tanque.
* **Contrato (`IEstrategiaControle`):**
  ```cpp
  class IEstrategiaControle {
  public:
      virtual ~IEstrategiaControle() = default;
      virtual std::string nome() const = 0;
      virtual DecisaoAtuacao calcular(double nivel, double temp, const ConfiguracaoControle& config) = 0;
      virtual void resetar() = 0;
  };
  ```
* **Classes Concretas:**
  * `EstrategiaFaixaHisterese`: Atuação binária (0% ou 100%) por faixas operacionais com histerese (Estratégia A).
  * `EstrategiaProporcionalModulada`: Atuação contínua modulada em torno da vazão de equilíbrio (~33.3% na bomba) e perda térmica passiva (Estratégia B).
* **Benefício:** Para criar uma Estratégia C (ex: PID) ou D (ex: Econômica), cria-se um novo arquivo `.hpp` e `.cpp` sem tocar no loop do controlador.

---

### 3.2. Padrão Command (Na Atuação C++: *O Que Executar e Rastrear?*)
* **Objetivo:** Encapsular a ordem de atuação em um objeto rastreável, separando a decisão do transporte JSON e gerenciando a confirmação da planta (`ultimo_comando`).
* **Contrato (`IComando`):**
  ```cpp
  class IComando {
  public:
      virtual ~IComando() = default;
      virtual int id() const = 0;
      virtual const std::string& sessao() const = 0;
      virtual double valorBomba() const = 0;
      virtual double valorAquecedor() const = 0;
      virtual bool foiConfirmado(int ultimoComandoConfirmado) const = 0;
      virtual std::string gerarJson() const = 0;
  };
  ```
* **Classes Concretas:**
  * `ComandoAtuacao`: Comando normal de controle gerado pela estratégia ativa, contendo `id` incremental positivo e verificação de confirmação.
  * `ComandoSeguranca`: Comando prioritário de intertravamento (ex.: corte imediato do aquecedor ou corte da bomba).
* **Benefício:** Implementa a diretriz *"emissão não prova aplicação"*, associando cada atuação ao seu ID e reconciliando a resposta no ciclo seguinte.

---

### 3.3. Padrão Repository (Na Persistência Python: *Como Isolar os Dados?*)
* **Objetivo:** Desacoplar a interface visual do Streamlit dos comandos SQL, conexões e arquivos do SQLite (`dados/historico.db`).
* **Classe `RepositorioHistorico` (Python):**
  * `inicializar_banco()`: Cria a tabela `historico` com chave primária composta `PRIMARY KEY (sessao, ciclo)`.
  * `salvar_telemetria(telemetria)`: Executa `INSERT OR IGNORE`, garantindo que re-renderizações do Streamlit nunca dupliquem registros de um mesmo ciclo.
  * `obter_ultimos_registros(limite)`: Retorna dados estruturados em DataFrame ou lista de objetos para os gráficos de tendência.
  * `exportar_csv(caminho)`: Extrai os dados diretamente da tabela para arquivo CSV.
* **Benefício:** Testável 100% via linha de comando (`tests/autorais_repositorio.py`) sem necessitar abrir a tela do Streamlit.

---

### 3.4. Padrão Observer (Nos Alarmes e Notificações Python: *Como Comunicar Mudanças?*)
* **Objetivo:** Desacoplar a detecção de alarmes dos múltiplos canais de saída (tela, e-mail, log), evitando estritamente o envio repetitivo (*spam/flood* a cada ciclo de 1 segundo).
* **Estrutura:**
  * **Subject (`MonitorAlarmes`):** Monitora a lista de alarmes da `telemetria.json`, compara o estado atual com o estado anterior e só emite eventos na **mudança de estado** (borda de subida ao ativar, borda de descida ao normalizar).
  * **Interface (`IAlarmeObserver`):**
    ```python
    class IAlarmeObserver(ABC):
        @abstractmethod
        def notificar_ativacao(self, alarme: dict, contexto: dict) -> None: ...
        @abstractmethod
        def notificar_normalizacao(self, alarme: dict, contexto: dict) -> None: ...
    ```
  * **Observador Concreto 1 (`NotificadorTela`):** Envia mensagens visuais e alertas destacados para o Streamlit.
  * **Observador Concreto 2 (`NotificadorEmail`):** Formata mensagem de e-mail e despacha via SMTP (modo real) ou grava em log/memória (modo simulação/mock para testes automatizados).

---

## 4. Estrutura Modular de Diretórios e Arquivos

Seguindo o princípio de **um arquivo por classe/objeto**, o código é organizado de forma modular, permitindo compilação incremental e independência de testes:

```text
poo-planta-controle-hugo/
├── Makefile
├── ETAPA_ATUAL
├── dados/                              # Snapshots JSON locais e SQLite
│   ├── planta.json
│   ├── comando_planta.json
│   ├── telemetria.json
│   ├── solicitacao_operador.json
│   └── historico.db
│
├── controlador/                        # Aplicação C++
│   ├── main.cpp                        # Ponto de entrada CLI (--dados, --uma-vez)
│   │
│   ├── dominio/                        # Objetos de Valor e Entidades
│   │   ├── medicao.hpp / .cpp          # Modelagem de leituras [0, 100]
│   │   ├── atuacao.hpp / .cpp          # Percentuais válidos de bomba e aquecedor
│   │   ├── aquisicao.hpp / .cpp        # Parse e validação do planta.json
│   │   ├── telemetria.hpp / .cpp       # Modelo de saída telemetria.json
│   │   └── alarme.hpp / .cpp           # Entidade de alarme (código, severidade, regra)
│   │
│   ├── estrategias/                    # Padrão STRATEGY
│   │   ├── i_estrategia_controle.hpp   # Interface da política
│   │   ├── estrategia_histerese.hpp    # Estratégia A: Faixas com histerese
│   │   ├── estrategia_histerese.cpp
│   │   ├── estrategia_modulada.hpp     # Estratégia B: Proporcional suave
│   │   └── estrategia_modulada.cpp
│   │
│   ├── comandos/                       # Padrão COMMAND
│   │   ├── i_comando.hpp               # Interface do comando de atuação
│   │   ├── comando_atuacao.hpp         # Comando normal com ID sequencial
│   │   ├── comando_atuacao.cpp
│   │   ├── comando_seguranca.hpp       # Comando prioritário de intertravamento
│   │   └── comando_seguranca.cpp
│   │
│   └── servicos/                       # Orquestração e I/O
│       ├── gerenciador_seguranca.hpp   # Avaliação de limites críticos e alarmes
│       ├── gerenciador_seguranca.cpp
│       ├── validador_solicitacao.hpp   # Leitura segura de solicitacao_operador.json
│       ├── validador_solicitacao.cpp
│       ├── publicador_json.hpp         # Escrita atômica temporário -> rename
│       └── publicador_json.cpp
│
├── supervisor/                         # Aplicação Python / Streamlit
│   ├── app.py                          # Interface IHM Streamlit
│   ├── autenticacao.py                 # Validação de senha de admin ("adm123")
│   ├── repositorio_historico.py        # Padrão REPOSITORY (SQLite e CSV)
│   ├── monitor_alarmes.py              # Padrão OBSERVER (Subject)
│   ├── observador_base.py              # Interface IAlarmeObserver
│   ├── notificador_tela.py             # Observador para a UI
│   └── notificador_email.py            # Observador de e-mail (Real SMTP + Mock)
│
├── tools/                              # Ferramentas CLI de apoio
│   ├── enviar_comando.py               # Envio manual direto à planta (fornecido)
│   ├── enviar_solicitacao.py           # Simula envio de operador sem abrir Streamlit
│   ├── testar.py                       # Runner do make test
│   └── cobertura.py                    # Runner do make cobertura
│
└── tests/                              # Suíte de Testes Automatizados
    ├── cpp/                            # Testes C++ descobertos pelo Makefile
    │   ├── autorais_medicao.cpp        # Teste de fronteiras de valores
    │   ├── autorais_aquisicao.cpp      # Teste de validação e recusa de dados
    │   ├── autorais_estrategias.cpp    # Teste de Strategy (A vs B)
    │   ├── autorais_comandos.cpp       # Teste de Command e rastreamento de ID
    │   └── autorais_seguranca.cpp      # Teste de intertravamentos e alarmes
    │
    ├── autorais_repositorio.py         # Teste de Repository (SQLite e deduplicação)
    ├── autorais_observer_email.py      # Teste de Observer (anti-spam de e-mail)
    └── autorais_solicitacao.py         # Teste de validação de regras do operador
```

---

## 5. Diretrizes para Testes e Integração com o Makefile

### 5.1. Como o Makefile Compila os Testes C++
O `Makefile` fornecido já possui regras automáticas para compilação isolada:
1. Compila todas as implementações em `controlador/**/*.cpp` como uma biblioteca de objetos (`CPP_LIBRARY`), **excluindo `main.o`**.
2. Procura automaticamente por qualquer arquivo `tests/cpp/autorais_*.cpp`.
3. Gera um executável para cada teste autoral em `build/autorais/autorais_*`, ligando-o à `CPP_LIBRARY`.
4. Cada teste autoral C++ possui seu próprio `int main()`, retornando `0` para sucesso e `!= 0` para falha.

### 5.2. Como o Runner Python Descobre os Testes
* O comando `make test` executa `tools/testar.py`:
  * Roda todos os executáveis em `build/autorais/`.
  * Descobre e executa via `unittest` todos os arquivos `tests/autorais_*.py`.
* **Regra de Cobertura (`make cobertura`):**
  * Nenhuma classe pode ser instanciada sem asserções sobre seu comportamento.
  * Testar casos de sucesso, valores de fronteira (ex: 0.0, 100.0) e casos de falha (ex: -1.0, strings, arquivos vazios), verificando a preservação do estado anterior.

---

## 6. Fluxo de Execução e Ciclo de Decisão (Controlador C++)

A cada ciclo de execução (ou em invocação única com `--uma-vez`), o controlador segue o fluxo orquestrado:

```text
1. Leitura e Parse de planta.json
   ├─ Se arquivo ausente ou corrompido ──► Retorna != 0 na CLI e preserva estado
   └─ Se ciclo repetido ─────────────────► Não emite novo comando e encerra ciclo

2. Reconciliação do Comando Anterior
   └─ Verifica se planta.json["ultimo_comando"] confirma o ID do último comando emitido

3. Atualização das Configurações do Operador
   ├─ Lê solicitacao_operador.json (se presente e válido)
   └─ Caso ausente ou inválido ──────────► Mantém última solicitação válida (ou padrão)

4. Avaliação de Segurança e Intertravamentos
   ├─ Se nivel <= 15.0% ────────────────► Gera ComandoSeguranca (Bomba 100%, Aquecedor 0%)
   ├─ Se nivel >= 85.0% ────────────────► Gera ComandoSeguranca (Bomba 0%)
   ├─ Se temp >= 75.0C  ────────────────► Gera ComandoSeguranca (Aquecedor 0%)
   └─ Se nenhum alarme crítico ─────────► Prossegue para a Estratégia de Controle

5. Execução da Estratégia Ativa (Padrão Strategy)
   ├─ Instância ativa (Histerese ou Modulada) calcula a atuação
   └─ Retorna objeto ComandoAtuacao com novo ID incremental

6. Despacho e Publicação Atômica
   ├─ Grava comando_planta.json via arquivo temporário + renomeação
   └─ Grava telemetria.json com medições, confirmação e lista de alarmes
```

Este norteador garante conformidade total com os requisitos do curso, excelência em design orientado a objetos e facilidade para evoluir e demonstrar o projeto com segurança.

