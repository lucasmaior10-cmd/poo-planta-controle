# Guia Definitivo de Preparação para a Entrevista de POO
## Projeto: Sistema de Controle e Supervisão do Reservatório Térmico

> Este guia foi estruturado especificamente para a defesa oral e entrevista técnica com o professor. O objetivo não é decorar sintaxe, mas demonstrar domínio dos princípios de Orientação a Objetos, Padrões de Projeto (GoF), Engenharia de Software e decisões arquiteturais do sistema.

---

## 1. Mapa Classe por Classe (Responsabilidade Única & Colaborações)

### 🔵 Subsistema Controlador (C++)

#### Camada de Domínio (`controlador/dominio/`)
1. **`Medicao`**:
   * *Responsabilidade Única:* Encapsular e validar leituras físicas contínuas (nível e temperatura), garantindo que pertençam a $[0.0, 100.0]$ e rejeitando valores negativos, superiores a 100 ou `NaN`.
   * *Com quem conversa:* É utilizada pela classe `Aquisicao` para conter os dados numéricos dos sensores e pelos testes unitários `autorais_medicao`.
2. **`Atuacao`**:
   * *Responsabilidade Única:* Modelar o par de atuadores (`bomba` e `aquecedor`), garantindo grandezas válidas em $[0.0, 100.0]$ e realizando serialização e desserialização JSON (`paraJson()` e `deJson()`).
   * *Com quem conversa:* Colabora com `Aquisicao` (leitura dos atuadores da planta), `IEstrategiaControle` (cálculo de decisão) e `ComandoAtuacao`.
3. **`Alarme`**:
   * *Responsabilidade Única:* Representar a ocorrência de uma anomalia operacional no domínio com código padronizado (ex.: `NIVEL_MUITO_ALTO`), severidade e mensagem descritiva.
   * *Com quem conversa:* É instanciada pelo `GerenciadorSeguranca` na violação de limites e empacotada pela `Telemetria` no JSON de saída.
4. **`Aquisicao`**:
   * *Responsabilidade Única:* Fazer o parse estrito e validação semântica do contrato `planta.json` (versão 1, tags obrigatórias LT-01 e TT-01, unidades % e °C).
   * *Com quem conversa:* É lida por `ControladorProcesso` para obter o estado do processo e fornece metadados para a `Telemetria`.
5. **`Telemetria`**:
   * *Responsabilidade Única:* Consolidar o snapshot oficial do processo (leituras atuais, atuadores, ciclo, sessão e lista de alarmes) e formatar o contrato de saída.
   * *Com quem conversa:* Recebe dados de `Aquisicao` e `GerenciadorSeguranca`, e é entregue ao `PublicadorJson` para gravação em disco.

#### Camada de Estratégias — Padrão Strategy (`controlador/estrategias/`)
6. **`ConfiguracaoControle`**:
   * *Responsabilidade Única:* Estrutura de dados (*DTO*) que agrupa as faixas aceitáveis de operação configuradas pelo operador (mínimo e máximo para nível e temperatura).
   * *Com quem conversa:* É alimentada por `ValidadorSolicitacao` e consumida pelo método `calcular()` das estratégias.
7. **`IEstrategiaControle`**:
   * *Responsabilidade Única:* Interface abstrata pura que padroniza o contrato de qualquer algoritmo de controle sobre o tanque (`calcular()`, `nome()`, `resetar()`).
   * *Com quem conversa:* É mantida por composição em `ControladorProcesso` via ponteiro polimórfico (`std::unique_ptr<IEstrategiaControle>`) e implementada pelas estratégias concretas.
8. **`EstrategiaFaixaHisterese`**:
   * *Responsabilidade Única:* Implementar a política de faixas com banda morta (controle binário *bang-bang* 0% ou 100%), memorizando o estado anterior dentro da faixa neutra para evitar repique de atuadores.
   * *Com quem conversa:* Realiza a interface `IEstrategiaControle` e é acionada pelo `ControladorProcesso`.
9. **`EstrategiaProporcionalModulada`**:
   * *Responsabilidade Única:* Implementar a política de controle proporcional suave, ajustando a bomba continuamente em torno do ponto de equilíbrio físico (~33.3%) e o aquecedor conforme a perda térmica.
   * *Com quem conversa:* Realiza a interface `IEstrategiaControle` e é acionada pelo `ControladorProcesso`.

#### Camada de Comandos — Padrão Command (`controlador/comandos/`)
10. **`IComando`**:
    * *Responsabilidade Única:* Interface abstrata pura que define o contrato de uma ordem de atuação despachada para a planta (`id()`, `sessao()`, atuadores, `gerarJson()`, `foiConfirmado()`).
    * *Com quem conversa:* É utilizada pelo `ControladorProcesso` para despachar ações polimórficas para a planta.
11. **`ComandoAtuacao`**:
    * *Responsabilidade Única:* Representar o comando normal de controle, associando um ID sequencial estritamente positivo aos valores de bomba e aquecedor calculados pela estratégia ativa.
    * *Com quem conversa:* Realiza a interface `IComando`, sendo criada por `ControladorProcesso` e serializada para `comando_planta.json`.
12. **`ComandoSeguranca`**:
    * *Responsabilidade Única:* Representar um comando de emergência prioritário (intertravamento físico), forçando atuadores para estados seguros (ex.: corte imediato do aquecedor ou bomba 0%).
    * *Com quem conversa:* Realiza a interface `IComando`, sendo gerada por `GerenciadorSeguranca` e despachada com prioridade máxima por `ControladorProcesso`.

#### Camada de Serviços e Orquestração (`controlador/servicos/` e `main.cpp`)
13. **`GerenciadorSeguranca`**:
    * *Responsabilidade Única:* Avaliar nível e temperatura contra limites críticos, gerenciar a histerese de rearme e instanciar `ComandoSeguranca` e `Alarme`.
    * *Com quem conversa:* É consultada a cada ciclo por `ControladorProcesso` antes de qualquer cálculo de estratégia.
14. **`ValidadorSolicitacao`**:
    * *Responsabilidade Única:* Fazer leitura segura e validação rigorosa de `solicitacao_operador.json`, garantindo versão 1, ID incremental positivo e faixas operacionais coerentes ($min < max$).
    * *Com quem conversa:* Lê do disco e atualiza a configuração ativa mantida pelo `ControladorProcesso`.
15. **`PublicadorJson`**:
    * *Responsabilidade Única:* Executar a escrita atômica no sistema de arquivos, gravando primeiro em arquivo temporário (`.tmp`) e depois renomeando atomicamente (`rename/replace`).
    * *Com quem conversa:* É chamada por `ControladorProcesso` para persistir com segurança `comando_planta.json` e `telemetria.json`.
16. **`ControladorProcesso`**:
    * *Responsabilidade Única:* Atuar como o Maestro/Controlador de Fachada do processo C++, orquestrando o ciclo completo: aquisição, segurança, decisão de estratégia e despacho atômico.
    * *Com quem conversa:* Coordena todas as classes do subsistema C++ e é instanciada pela função `main()` em `main.cpp`.

---

### 🟢 Subsistema Supervisório e Simulação (Python)

17. **`Planta` (`planta/simulador.py` - Fornecida)**:
    * *Responsabilidade Única:* Simular a física do reservatório térmico (escoamento, dinâmica térmica e ruído de medição) e aplicar os comandos validados recebidos.
    * *Com quem conversa:* Lê `comando_planta.json` e gera `planta.json`.
18. **`RegistroHistorico` (`supervisor/repositorio_historico.py`)**:
    * *Responsabilidade Única:* Entidade de domínio do supervisório que encapsula uma linha histórica consolidada de processo, fornecendo chave primária composta `(sessao, ciclo)`.
    * *Com quem conversa:* É persistida e recuperada pelo `RepositorioHistorico`.
19. **`RepositorioHistorico` (`supervisor/repositorio_historico.py`)**:
    * *Responsabilidade Única:* Implementar o Padrão Repository, isolando comandos SQL, conexões SQLite (`historico.db`) e exportações CSV do restante da aplicação.
    * *Com quem conversa:* É invocada por `SupervisorioApp` para salvar ciclos de telemetria e carregar dados para gráficos.
20. **`EventoAlarme` (`supervisor/monitor_alarmes.py`)**:
    * *Responsabilidade Única:* Objeto de valor (*Event Payload*) que modela uma transição de alarme (ativação ou normalização), seu código, severidade e timestamp.
    * *Com quem conversa:* É gerada por `MonitorAlarmes` e entregue aos métodos de `IAlarmeObserver`.
21. **`IAlarmeObserver` (`supervisor/observador_base.py`)**:
    * *Responsabilidade Única:* Interface abstrata do Padrão Observer que padroniza os métodos de reação a alarmes (`notificar_ativacao` e `notificar_normalizacao`).
    * *Com quem conversa:* É implementada pelos observadores concretos e registrada em `MonitorAlarmes`.
22. **`MonitorAlarmes` (`supervisor/monitor_alarmes.py`)**:
    * *Responsabilidade Única:* Atuar como o *Subject* do Padrão Observer, comparando os alarmes do ciclo atual com os do ciclo anterior para disparar eventos exclusivamente na transição de estado (anti-spam).
    * *Com quem conversa:* Recebe a telemetria do `SupervisorioApp` e notifica os observadores (`NotificadorTela` e `NotificadorEmail`).
23. **`NotificadorTela` (`supervisor/notificador_tela.py`)**:
    * *Responsabilidade Única:* Observador concreto que armazena em memória os eventos de alarme recentes formatados para exibição na IHM.
    * *Com quem conversa:* Implementa `IAlarmeObserver` e fornece listas de alertas para o `SupervisorioApp`.
24. **`NotificadorEmail` (`supervisor/notificador_email.py`)**:
    * *Responsabilidade Única:* Observador concreto que monta e envia e-mails de alerta crítico via SMTP (modo real) ou grava em arquivo de log (modo mock para testes).
    * *Com quem conversa:* Implementa `IAlarmeObserver` e é acionado por `MonitorAlarmes`.
25. **`SupervisorioApp` (`supervisor/app.py`)**:
    * *Responsabilidade Única:* Interface Homem-Máquina (IHM) em Streamlit que exibe indicadores, gráficos temporais, exportador CSV e painel administrativo de controle.
    * *Com quem conversa:* Integra `RepositorioHistorico`, `MonitorAlarmes` e lê `telemetria.json`.

---

## 2. Onde Está a POO na Prática no Nosso Código

### A. Encapsulamento
* **Onde está:**
  * Em **`Medicao`** (`controlador/dominio/medicao.hpp`): O atributo `double valor_` é estritamente privado. A única forma de alterá-lo é pelo método `atualizar(double v)`, que valida as fronteiras $[0, 100]$ e finitez. Se o valor for inválido, lança exceção e preserva o estado anterior íntegro.
  * Em **`GerenciadorSeguranca`** (`controlador/servicos/gerenciador_seguranca.hpp`): Os booleanos de estado de histerese (`ativoNivelBaixo_`, `ativoNivelAlto_`, etc.) são privados. O mundo externo só acessa o resultado limpo e imutável retornado por `avaliar()`.
  * Em **`RepositorioHistorico`** (`supervisor/repositorio_historico.py`): O caminho do banco e os cursores SQL são privados. O supervisório não executa `SELECT` ou `INSERT` na mão, chamando métodos de alto nível como `salvar_registro()`.

### B. Herança
* **Onde está:**
  * **Comandos:** `ComandoAtuacao` e `ComandoSeguranca` herdam publicamente da classe base abstrata `IComando` (`class ComandoAtuacao : public IComando`).
  * **Estratégias:** `EstrategiaFaixaHisterese` e `EstrategiaProporcionalModulada` herdam publicamente de `IEstrategiaControle` (`class EstrategiaFaixaHisterese : public IEstrategiaControle`).
  * **Observadores Python:** `NotificadorTela` e `NotificadorEmail` herdam de `IAlarmeObserver` (`class NotificadorEmail(IAlarmeObserver):`).

### C. Polimorfismo
* **Onde está:**
  * Em **`ControladorProcesso`**: O atributo `std::unique_ptr<IEstrategiaControle> estrategiaAtiva_` armazena um ponteiro para a interface. No ciclo do controlador, ele executa:
    ```cpp
    DecisaoAtuacao decisao = estrategiaAtiva_->calcular(aq.nivel(), aq.temperatura(), config);
    ```
    O controlador não sabe nem precisa saber qual algoritmo está rodando. Em tempo de execução, a chamada é resolvida dinamicamente pela tabela de funções virtuais (*vtable*).
  * No despacho de comandos: O controlador manipula `std::unique_ptr<IComando>`. Seja um `ComandoAtuacao` ou um `ComandoSeguranca`, ele simplesmente invoca `cmd->gerarJson()`.
  * Em **`MonitorAlarmes.notificar_todos()`**: O método itera sobre a lista de `IAlarmeObserver` invocando `obs.notificar_ativacao(evento)`. A tela atualiza seu buffer e o e-mail dispara o SMTP sem acoplamento.

### D. Interfaces e Classes Abstratas
* **Onde está:**
  * Em C++: Classes que contêm métodos virtuais puros (`= 0`) e destrutor virtual default (`virtual ~IEstrategiaControle() = default;`). Impedem instanciação direta e definem contratos obrigatórios (`IEstrategiaControle`, `IComando`).
  * Em Python: Classes derivadas de `abc.ABC` utilizando o decorador `@abstractmethod` (`IAlarmeObserver`). Se uma subclasse esquecer de implementar `notificar_ativacao()`, o interpretador Python impede a instanciação da classe em tempo de execução.

---

## 3. Defesa dos 4 Padrões de Projeto (GoF)

```text
+----------------------------------------------------------------------------------------------------+
| 1. STRATEGY (C++): Isola e intercambia algoritmos de controle                                      |
|    Participantes: IEstrategiaControle (Strategy), EstrategiaFaixaHisterese / Modulada (Concretas) |
|    Contexto: ControladorProcesso                                                                   |
+----------------------------------------------------------------------------------------------------+
| 2. COMMAND (C++): Encapsula ordens como objetos auditáveis com ID e reconciliação                  |
|    Participantes: IComando (Command), ComandoAtuacao / ComandoSeguranca (Concretos)                 |
|    Invocador: ControladorProcesso | Receptor: Planta                                               |
+----------------------------------------------------------------------------------------------------+
| 3. REPOSITORY (Python): Isola operações de banco de dados SQLite da tela gráfica                   |
|    Participantes: RepositorioHistorico (Repository), RegistroHistorico (Entidade), SQLite          |
|    Cliente: SupervisorioApp                                                                        |
+----------------------------------------------------------------------------------------------------+
| 4. OBSERVER (Python): Notificação desacoplada e anti-spam de múltiplos canais                      |
|    Participantes: MonitorAlarmes (Subject), IAlarmeObserver (Interface)                            |
|    Observadores Concretos: NotificadorTela, NotificadorEmail                                       |
+----------------------------------------------------------------------------------------------------+
```

### 1. Padrão Strategy (No Controlador C++)
* **Problema Resolvido:** O sistema precisava suportar políticas de controle completamente distintas (Controle Binário por Histerese vs Controle Contínuo Proporcional) e permitir a troca em tempo de execução via comando do operador. Sem o Strategy, o loop do controlador se tornaria um emaranhado de `if (modo == HISTERESE) ... else if (modo == MODULADA)` difícil de manter e violando o Princípio Aberto/Fechado (OCP).
* **Benefício:** Para adicionar uma terceira estratégia (ex.: controle PID ou Fuzzy), basta criar uma nova classe derivada de `IEstrategiaControle`. O orquestrador central não sofre nenhuma alteração.

### 2. Padrão Command (Na Atuação C++)
* **Problema Resolvido:** A atuação sobre uma planta física não pode ser tratada como uma simples escrita de valores soltos. É necessário rastreabilidade, prevenção de comandos duplicados, suporte a comandos de emergência prioritários e verificação de que a planta realmente aplicou a ordem (*"emissão não prova aplicação"*).
* **Benefício:** Cada ordem vira um objeto rastreável com `id` incremental positivo. O comando sabe responder ao método `foiConfirmado(ultimoComandoConfirmado)`, permitindo reconciliação mestre-escravo determinística.

### 3. Padrão Repository (Na Persistência Python)
* **Problema Resolvido:** Interfaces construídas com Streamlit rodam seu script do início ao fim a cada clique do usuário. Se as queries SQL fossem escritas diretamente nos botões e componentes da tela, haveria duplicação de código, conexões abertas inadequadamente e risco de concorrência.
* **Benefício:** O `RepositorioHistorico` encapsula as tabelas, conexões e a regra `INSERT OR IGNORE` baseada na chave primária `(sessao, ciclo)`. A UI solicita dados como objetos Python limpos.

### 4. Padrão Observer (Nos Alarmes e Notificações Python)
* **Problema Resolvido:** A telemetria chega a cada 1 segundo. Se o nível do reservatório permanecer alto por 10 minutos (600 ciclos), o operador receberia 600 e-mails na caixa de entrada (*inundação/spam*). Além disso, o motor de alarmes não deveria depender da tecnologia de envio (e-mail, tela, SMS, Telegram).
* **Benefício:** O `MonitorAlarmes` guarda o estado anterior e notifica os observadores registrados **exclusivamente na borda de transição** (quando o alarme nasce e quando normaliza). Novos canais de alerta entram no sistema apenas implementando `IAlarmeObserver`.

---

## 4. Fluxo de Ponta a Ponta: A História de um Ciclo

Acompanhe o caminho de um dado em um ciclo real de operação de 1 segundo:

1. **Geração Física (Planta):**
   * O simulador calcula as equações do reservatório térmico e gera o snapshot em disco com nome temporário, renomeando-o para `dados/planta.json` (escrita atômica).
2. **Entrada e Validação Semântica (C++):**
   * O `ControladorProcesso` lê o arquivo através de `Aquisicao::carregarDeArquivo()`.
   * A classe `Aquisicao` confere se o JSON possui `versao: 1`, sessão não vazia, ciclo estritamente positivo, leituras LT-01 e TT-01 com valores $\in [0.0, 100.0]$ e unidades válidas.
3. **Idempotência e Mudança de Sessão:**
   * O controlador checa se `aq.ciclo() == ultimoCicloProcessado_`. Se o simulador ainda não gerou um novo ciclo, o controlador ignora e não reprocessa.
   * Se a sessão mudou (simulador reiniciado), o controlador limpa o histórico e sincroniza o próximo ID de comando.
4. **Leitura da Intenção do Operador:**
   * O `ValidadorSolicitacao` verifica se existe um novo `dados/solicitacao_operador.json` com `id` maior que o anterior. Se for válido, atualiza a estratégia ativa (`histerese` ou `modulada`) e os limites de alarme.
5. **Avaliação de Segurança (Prioridade Absoluta):**
   * O `GerenciadorSeguranca` avalia nível e temperatura contra os limites críticos.
   * **Se houver violação crítica:** Gera a lista de `Alarme` e instancia um `ComandoSeguranca` (ex.: corte total de bomba e aquecedor). A estratégia normal é ignorada.
   * **Se estiver tudo seguro:** Invoca `estrategiaAtiva_->calcular()` e instancia um `ComandoAtuacao` com ID incremental.
6. **Publicação Atômica:**
   * O `PublicadorJson` grava `comando_planta.json` e `telemetria.json` via escrita atômica (`.tmp` seguido de `rename`), garantindo que nenhum leitor pegue arquivos pela metade.
7. **Captura no Supervisório (Python):**
   * O `SupervisorioApp` lê `telemetria.json` no ciclo de atualização da tela.
8. **Persistência Histórica Desacoplada (SQLite):**
   * O `RepositorioHistorico` insere o registro na tabela SQLite via `INSERT OR IGNORE INTO historico ...`. Mesmo que a tela do Streamlit renderize 5 vezes no mesmo segundo, nenhuma linha duplicada é gravada.
9. **Detecção de Transição e Notificação (Observer):**
   * O `MonitorAlarmes` compara a lista de alarmes da telemetria com a memória do ciclo anterior.
   * Se um alarme acabou de disparar (borda de subida), gera um `EventoAlarme` e notifica os observadores: o `NotificadorTela` coloca o alerta visual e o `NotificadorEmail` dispara o e-mail ou grava o mock.
10. **Fechamento do Ciclo na Planta:**
    * A planta lê `comando_planta.json`, verifica o ID, atualiza seus atuadores e reflete o número no campo `ultimo_comando` do ciclo seguinte, confirmando a execução da ordem.

---

## 5. Simulado de Perguntas Difíceis do Professor (Com Respostas de Ouro)

### P1: *"Por que vocês se preocuparam com Escrita Atômica usando arquivo `.tmp` e `rename`? Não bastava dar `ofstream << json`?"*
> **Resposta de Ouro:**
> *"Professor, o nosso sistema opera com processos concorrentes e independentes sem memória compartilhada. Se o Controlador C++ gravasse diretamente no arquivo de destino, haveria uma janela de tempo em que o arquivo estaria incompleto em disco. Se a Planta ou o Supervisório lessem o arquivo nesse milissegundo exato, o parser de JSON quebraria com erro de sintaxe. A escrita atômica grava tudo primeiro em um arquivo temporário e usa a primitiva `std::filesystem::rename`, que no nível do sistema operacional é uma operação atômica de ponteiro de inode: o arquivo novo substitui o antigo instantaneamente, garantindo consistência estrita."*

---

### P2: *"Como vocês garantem Idempotência? O que impede que o banco SQLite fique cheio de lixo se a tela atualizar várias vezes no mesmo segundo?"*
> **Resposta de Ouro:**
> *"Garantimos a idempotência em duas camadas. No C++, o `ControladorProcesso` guarda o `ultimoCicloProcessado_`. Se ele ler um `planta.json` com o mesmo ciclo já tratado, ele encerra o ciclo sem emitir novos comandos nem alterar o estado. No Python, a tabela do SQLite foi projetada com chave primária composta `PRIMARY KEY (sessao, ciclo)` e o `RepositorioHistorico` utiliza `INSERT OR IGNORE`. Mesmo que o Streamlit recarregue dezenas de vezes, o banco rejeita duplicatas silenciosamente e preserva a integridade."*

---

### P3: *"E se o Controlador C++ travar ou o processo for morto com `kill -9`? O que acontece com a caldeira?"*
> **Resposta de Ouro:**
> *"Nós projetamos o sistema respeitando o requisito de segurança contra perda de comando (Heartbeat). A classe `Planta` possui um contador interno de expiração (`timeout_comando = 5`). Se passarem 5 ciclos consecutivos sem que a planta receba um comando com ID novo, ela automaticamente aciona o método `seguro('comando_expirado')`, desligando a bomba e o aquecedor por conta própria. A segurança não depende exclusivamente da saúde do processo de controle."*

---

### P4: *"Por que vocês implementaram Histerese de Rearme no Gerenciador de Segurança? Não era só desligar o alarme quando o nível voltasse a ficar aceitável?"*
> **Resposta de Ouro:**
> *"Se não usássemos histerese, o sistema entraria em um regime destrutivo de repique (*chattering*). Como o simulador possui ruído estocástico de sensor (marola de $\pm 0.2\%$), se o limite for $15\%$ e o nível estiver oscilando em $15.05\%$ e $14.95\%$, o alarme e o intertravamento ficariam ligando e desligando a cada segundo. A histerese exige que o nível atinja $15\%$ para ativar, mas só permite desarmar quando subir com margem segura até $20\%$, garantindo estabilidade operacional."*

---

### P5: *"Qual a diferença conceitual e prática entre o Strategy e o Command no projeto de vocês? Eles não fazem quase a mesma coisa?"*
> **Resposta de Ouro:**
> *"Eles resolvem problemas totalmente diferentes. O **Strategy** responde à pergunta: *'COMO CALCULAR?'*. Ele encapsula o algoritmo matemático de decisão (faixa com banda morta versus proporcional modulada) e produz uma sugestão de atuação. Já o **Command** responde à pergunta: *'O QUE EXECUTAR E RASTREAR?'*. Ele pega essa decisão, adiciona um ID sequencial obrigatório, vincula à sessão da planta e implementa o protocolo de confirmação mestre-escravo. Além disso, o Command permite criar comandos de emergência (`ComandoSeguranca`) que atropelam a decisão do Strategy quando a segurança física exige."*

---

### P6: *"Por que o destrutor de `IEstrategiaControle` e `IComando` DEVE ser virtual (`virtual ~IEstrategiaControle() = default`)? O que aconteceria se você esquecesse disso?"*
> **Resposta de Ouro:**
> *"Como usamos ponteiros polimórficos (`std::unique_ptr<IEstrategiaControle>`), quando o ponteiro da classe base for destruído ou substituído, o C++ precisa saber que deve chamar o destrutor da classe filha correspondente (`EstrategiaFaixaHisterese`). Se o destrutor da classe base não fosse virtual, ocorreria comportamento indefinido (*undefined behavior*) e vazamento de memória (*memory leak*), pois apenas o pedaço da classe base seria desalocado, ignorando os atributos da classe derivada."*

---

### P7: *"Por que o padrão Observer no Supervisório só notifica na borda de subida/descida em vez de notificar a cada ciclo que o alarme estiver ativo?"*
> **Resposta de Ouro:**
> *"Porque a telemetria chega a cada 1 segundo. Se o alarme permanecesse ativo por 10 minutos e notificássemos a cada ciclo, o operador receberia 600 e-mails na caixa de entrada, configurando ataque de negação de serviço na infraestrutura de e-mail e inutilizando o alerta. O `MonitorAlarmes` guarda o conjunto de alarmes do ciclo anterior e só emite `EventoAlarme` na transição: ativação na borda de subida (quando o problema nasce) e normalização na borda de descida (quando a situação é resolvida)."*

---

### P8: *"Se a diretoria da fábrica pedir amanhã para implementar uma terceira estratégia (ex.: controle PID) e enviar notificações por Telegram, quantos arquivos existentes você precisa alterar?"*
> **Resposta de Ouro:**
> *"Nenhum arquivo de lógica existente precisará ser modificado, respeitando rigorosamente o Princípio Aberto/Fechado (OCP). Para o controle PID, basta criar `estrategia_pid.hpp/.cpp` implementando a interface `IEstrategiaControle`. Para o Telegram, basta criar `notificador_telegram.py` implementando `IAlarmeObserver` e registrá-lo com `monitor.registrar_observador()`. Apenas o ponto de instanciação/fábrica reconhece as novas classes; o orquestrador, o loop de controle e a tela permanecem 100% intactos."*
