# Escopo do Projeto — Sistema de Controle e Supervisão de um Reservatório Térmico

## 1. Visão geral

O projeto consiste no desenvolvimento de um sistema de automação e supervisão de um reservatório térmico, utilizando três programas independentes que se comunicam por meio de arquivos JSON.

O sistema deverá monitorar duas variáveis do processo:

- **nivel:** representa a quantidade de líquido presente no reservatório.
- **temperatura:** representa a temperatura do líquido armazenado.

O processo contará com dois atuadores:

- **bomba:** responsável por aumentar o nível do líquido.
- **aquecedor:** responsável por aumentar a temperatura do líquido.

O sistema deverá operar automaticamente, mantendo as variáveis dentro de faixas configuráveis pelo operador por meio da interface supervisória.

---

## 2. Arquitetura do sistema

O projeto será dividido em três aplicações independentes:

### 2.1. Simulador da planta

Programa fornecido pelo professor, responsável por simular o comportamento físico do reservatório.

Suas responsabilidades são:

- Simular a variação do nível do líquido.
- Simular a variação da temperatura.
- Receber os comandos dos atuadores.
- Atualizar os dados do processo.
- Disponibilizar as informações da planta para o controlador.

### 2.2. Controlador

Programa desenvolvido em C++, responsável pela lógica de controle automático.

Suas responsabilidades são:

- Ler os dados disponibilizados pela planta.
- Receber as configurações solicitadas pelo supervisório.
- Validar as configurações recebidas.
- Executar a lógica de controle do nível e da temperatura.
- Aplicar as regras de segurança.
- Enviar comandos para os atuadores da planta.
- Disponibilizar informações de telemetria para o supervisório.

O controlador será o único responsável por escrever o arquivo de comandos da planta.

### 2.3. Supervisório

Programa desenvolvido em Python, utilizando Streamlit para a interface gráfica.

Suas responsabilidades são:

- Exibir os valores atuais de nível e temperatura.
- Permitir a configuração das faixas de operação.
- Validar os valores informados pelo operador.
- Enviar solicitações de configuração ao controlador.
- Exibir o estado dos atuadores.
- Exibir alarmes e diagnósticos.
- Apresentar gráficos de tendências.
- Registrar o histórico do processo.
- Permitir a exportação dos dados históricos em CSV.

---

## 3. Comunicação entre os programas

A comunicação será realizada por meio de arquivos JSON locais.

Serão utilizados quatro arquivos:

| Arquivo | Origem | Destino | Finalidade |
|---|---|---|---|
| `planta.json` | Simulador | Controlador | Disponibilizar os dados atuais da planta. |
| `solicitacao_operador.json` | Supervisório | Controlador | Enviar as faixas de operação configuradas pelo operador. |
| `comando_planta.json` | Controlador | Simulador | Enviar os comandos dos atuadores. |
| `telemetria.json` | Controlador | Supervisório | Disponibilizar medições, estados, alarmes e diagnósticos. |

### 3.1. Fluxo de comunicação

O funcionamento geral seguirá o seguinte fluxo:

1. O simulador atualiza os dados do processo em `planta.json`.
2. O controlador lê os dados da planta.
3. O supervisório permite que o operador configure as faixas de operação.
4. O supervisório valida os valores e publica a solicitação em `solicitacao_operador.json`.
5. O controlador lê e valida a solicitação.
6. O controlador executa a lógica de controle.
7. O controlador publica os comandos em `comando_planta.json`.
8. O simulador recebe os comandos e atualiza o processo.
9. O controlador disponibiliza os dados de acompanhamento em `telemetria.json`.
10. O supervisório lê a telemetria e atualiza a interface e o histórico.

**Regra importante:** o supervisório não poderá escrever diretamente em `comando_planta.json`. Toda decisão de acionamento deverá passar pelo controlador.

---

## 4. Estratégias de controle automático (Padrão Strategy)

Para atender ao Requisito 3 e demonstrar o padrão de projeto *Strategy*, o sistema contará com uma interface abstrata de política de controle e **duas estratégias concretas com comportamentos distintos e testáveis**.

O operador poderá selecionar qual estratégia utilizar por meio do supervisório (ou o controlador utilizará a estratégia padrão em modo autônomo).

### 4.1. Estratégia 1: Controle por Faixas com Histerese (Bang-Bang por Faixas)

Esta estratégia foca na simplicidade e na redução do número de acionamentos dos atuadores, mantendo as grandezas dentro de uma janela de operação definida por limites inferior e superior.

* **Parâmetros da estratégia:**
  * Nível: $N_{min}$ e $N_{max}$ (exemplo padrão: 40% a 70%).
  * Temperatura: $T_{min}$ e $T_{max}$ (exemplo padrão: 30°C a 50°C).

* **Lógica de atuação da Bomba:**
  | Condição do Nível | Ação na Bomba | Justificativa |
  |---|---|---|
  | $\text{nível} \le N_{min}$ | 100.0% (Ligada) | Recuperar o volume do tanque com vazão máxima. |
  | $N_{min} < \text{nível} < N_{max}$ | Manter estado anterior | Banda morta (histerese) que evita liga/desliga excessivo. |
  | $\text{nível} \ge N_{max}$ | 0.0% (Desligada) | Cessar enchimento para não ultrapassar a faixa. |

* **Lógica de atuação do Aquecedor:**
  | Condição da Temperatura | Ação no Aquecedor | Justificativa |
  |---|---|---|
  | $\text{temp} \le T_{min}$ | 100.0% (Ligado) | Elevar a temperatura com potência máxima. |
  | $T_{min} < \text{temp} < T_{max}$ | Manter estado anterior | Manter aquecimento até o teto ou esfriamento passivo. |
  | $\text{temp} \ge T_{max}$ | 0.0% (Desligado) | Cessar aquecimento ao atingir o limite superior. |

---

### 4.2. Estratégia 2: Controle Proporcional Modulado / Suave (Modo Estabilizador)

Diferente da Estratégia 1 (que atua em degraus binários de 0% ou 100%), a Estratégia 2 calcula potências contínuas e proporcionais ao erro em relação ao centro da faixa desejada ($SetPoint = \frac{Min + Max}{2}$), promovendo uma operação mais suave e próxima do ponto de equilíbrio físico do tanque.

* **Dinâmica física de referência:**
  * Ponto de equilíbrio estático do nível: a perda contínua é de $1.0\%$ por ciclo. Como a bomba fornece até $3.0\%$, a bomba em $\approx 33.33\%$ mantém o nível estável.
  * Ponto de equilíbrio térmico: perda passiva em direção a $20.0^\circ\text{C}$ de taxa $0.04 \times (T - 20)$.

* **Lógica de atuação da Bomba:**
  * Calcula o erro em relação ao setpoint central: $e_N = SP_N - \text{nível}$.
  * Aplica ganho proporcional somado à potência de equilíbrio base ($33.33\%$):
    $$\text{bomba} = \text{clamp}\left(33.33 + K_p \times e_N,\ 0.0,\ 100.0\right)$$
  * Resultado: à medida que o nível se aproxima do setpoint, a bomba reduz gradualmente a potência para a vazão de equilíbrio (~33%), evitando ultrapassagens bruscas (*overshoot*).

* **Lógica de atuação do Aquecedor:**
  * Calcula o erro térmico: $e_T = SP_T - \text{temperatura}$.
  * Se $\text{temperatura} \ge SP_T$, o aquecedor é zerado ($0.0\%$).
  * Se $\text{temperatura} < SP_T$, a potência é dosada proporcionalmente à distância térmica:
    $$\text{aquecedor} = \text{clamp}\left(K_{pT} \times e_T,\ 0.0,\ 100.0\right)$$

* **Diferença de comportamento testável:**
  Em testes automatizados com as mesmas leituras de entrada (ex: nível em 45%, $N_{min}=40\%$, $N_{max}=70\%$), a Estratégia 1 mantém o estado anterior (0% ou 100%), enquanto a Estratégia 2 calcula uma potência modulada intermediária (ex: 48.3%). Essa divergência quantitativa e qualitativa atende plenamente ao Requisito 3.

---

## 5. Atualização das configurações de operação e modo autônomo

### 5.1. Operação autônoma por padrão (*Fallback*)
O controlador é projetado para operar de forma autônoma e desacoplada do supervisório:
* **Inicialização sem supervisório:** Caso o arquivo `solicitacao_operador.json` ainda não exista (como nos testes da CLI `make test`, `--uma-vez`), o controlador inicia utilizando **parâmetros padrão seguros**:
  * Faixa de nível padrão: $N_{min} = 40.0\%$, $N_{max} = 70.0\%$
  * Faixa de temperatura padrão: $T_{min} = 30.0^\circ\text{C}$, $T_{max} = 50.0^\circ\text{C}$
  * Estratégia padrão: `histerese` (Estratégia 1)
  * Limites de alarmes padrão de segurança ativados.
* **Persistência da última solicitação:** Uma vez que o supervisório envie uma configuração válida, o controlador a mantém em memória (e pode registrar em `dados/controlador_estado.json`). Se o supervisório for fechado ou parar de publicar, o controlador **continua operando normalmente com base na última solicitação válida**.

### 5.2. Testes de controle sem interface gráfica (Ferramenta CLI)
Para viabilizar o teste da integração e das políticas sem a necessidade de abrir o Streamlit, o projeto disponibiliza um utilitário CLI em Python ([`tools/enviar_solicitacao.py`](file:////wsl.localhost/Ubuntu/home/hugo/curso-poo/trabalho/poo-planta-controle-hugo/tools/enviar_solicitacao.py)) que permite simular solicitações de operador diretamente pelo terminal, gerando `dados/solicitacao_operador.json` de forma controlada.

### 5.3. Ciclo de atualização
1. O operador edita faixas, estratégia ou alarmes no supervisório.
2. O supervisório valida os valores antes de publicá-los.
3. O supervisório publica o snapshot em `solicitacao_operador.json` de forma atômica.
4. O controlador lê e valida a solicitação.
5. **A nova configuração somente será aplicada na próxima medição válida recebida da planta.** Não há recálculo baseado em medições obsoletas.

### 5.4. Rejeição de configurações inválidas
Caso a configuração recebida seja inválida:
- O controlador rejeita a solicitação e emite diagnóstico textual na telemetria.
- A última configuração válida (ou a configuração padrão) é rigorosamente preservada.
- Nenhuma ação é executada com base em valores corrompidos ou incoerentes.

---

## 6. Validação e formato das configurações do operador

### 6.1. Formato do arquivo `solicitacao_operador.json`
O arquivo gravado pelo supervisório seguirá o contrato JSON abaixo:

```json
{
  "versao": 1,
  "id": 1,
  "timestamp": "2026-11-11T15:00:00Z",
  "estrategia": "histerese",
  "faixas": {
    "nivel": {"min": 40.0, "max": 70.0},
    "temperatura": {"min": 30.0, "max": 50.0}
  },
  "alarmes": {
    "nivel_muito_baixo": {"habilitado": true, "limite": 15.0},
    "nivel_muito_alto": {"habilitado": true, "limite": 85.0},
    "temp_muito_baixa": {"habilitado": true, "limite": 15.0},
    "temp_muito_alta": {"habilitado": true, "limite": 75.0}
  }
}
```

* `versao`: número inteiro 1.
* `id`: identificador sequencial positivo da solicitação.
* `timestamp`: instante UTC no formato ISO 8601.
* `estrategia`: string identificando a estratégia ativa (`"histerese"` ou `"modulada"`).
* `faixas`: limites mínimos e máximos para o controle normal.
* `alarmes`: configuração individual de limites críticos e flags de habilitação (*bypass*).

### 6.2. Regras de validação (Supervisório e Controlador)
1. **Tipos e completude:** todos os campos obrigatórios presentes e números finitos (sem `NaN` ou `null`).
2. **Coerência de faixas operacionais:** $min < max$ para nível e temperatura ($0.0 \le min < max \le 100.0$).
3. **Coerência dos limites de alarme:**
   * $\text{limite\_nivel\_baixo} < \text{limite\_nivel\_alto}$
   * $\text{limite\_temp\_baixa} < \text{limite\_temp\_alta}$
   * Limite de alarme baixo deve ser menor ou igual ao mínimo operacional correspondente ($\text{limite\_baixo} \le min$).
   * Limite de alarme alto deve ser maior ou igual ao máximo operacional correspondente ($\text{limite\_alto} \ge max$).
4. **Estratégia suportada:** campo `estrategia` deve ser `"histerese"` ou `"modulada"`.

---

## 7. Alarmes e intertravamentos de segurança (Requisito 7)

O sistema possui quatro alarmes configuráveis pelo operador/administrador, com regras, fronteiras numéricas explícitas, histerese para rearme e ações de segurança mandatórias. As ações de segurança possuem **prioridade absoluta** sobre os comandos normais de controle.

### 7.1. Catálogo de Alarmes de Processo

| Identificador | Severidade | Condição de Ativação | Condição de Rearme (Histerese) | Ação Mandatória do Controlador |
|---|---|---|---|---|
| `NIVEL_MUITO_BAIXO` | crítico | $\text{nível} \le \text{limite}$ (padrão: $15.0\%$) | $\text{nível} \ge \text{limite} + 5.0\%$ ($20.0\%$) | **Intertravamento:** desliga aquecedor ($0.0\%$) para evitar queima a seco; comanda bomba a $100.0\%$. |
| `NIVEL_MUITO_ALTO` | crítico | $\text{nível} \ge \text{limite}$ (padrão: $85.0\%$) | $\text{nível} \le \text{limite} - 5.0\%$ ($80.0\%$) | **Intertravamento:** força desligamento da bomba ($0.0\%$) para evitar transbordamento. |
| `TEMP_MUITO_BAIXA` | aviso | $\text{temp} \le \text{limite}$ (padrão: $15.0^\circ\text{C}$) | $\text{temp} \ge \text{limite} + 3.0^\circ\text{C}$ ($18.0^\circ\text{C}$) | Emite aviso operacional de risco de sub-resfriamento; liga aquecedor se o nível for seguro. |
| `TEMP_MUITO_ALTA` | crítico | $\text{temp} \ge \text{limite}$ (padrão: $75.0^\circ\text{C}$) | $\text{temp} \le \text{limite} - 5.0^\circ\text{C}$ ($70.0^\circ\text{C}$) | **Intertravamento:** desliga aquecedor ($0.0\%$) imediatamente para evitar superaquecimento. |

### 7.2. Diagnóstico de Comunicação: Aquisição Obsoleta (`AQUISICAO_OBSOLETA`)
* **Condição de ativação:** idade do snapshot de medição $> 5.0\text{ segundos}$ ou ciclo congelado na mesma sessão.
* **Severidade:** aviso.
* **Ações:** aviso visual destacado na interface e retenção/suspensão de novas atuações no controlador.

### 7.3. Habilitação de Alarmes e Proteção por Senha no Supervisório
* O operador pode habilitar ou desabilitar cada alarme individualmente (*bypass* de segurança operacional).
* **Controle de Acesso:** para alterar os limites críticos dos alarmes ou habilitar/desabilitar alarmes na interface Streamlit, é exigida autenticação de administrador com a senha `"adm123"`. Alterações não autenticadas são bloqueadas pela tela.

### 7.4. Hierarquia e Prioridade de Decisão
1. **Intertravamentos de Segurança / Alarmes Críticos Habilitados** (prioridade máxima).
2. **Proteção contra Falha de Aquisição / Dados Inválidos** (modo seguro com atuadores zerados).
3. **Política de Controle da Estratégia Ativa** (faixa de operação solicitada).

---

## 8. Diagnóstico de aquisição de dados e comportamento seguro

O controlador identifica e trata proativamente situações anômalas na leitura de dados da planta:
- Arquivo `planta.json` ausente.
- Conteúdo vazio, corrompido ou JSON sintaticamente inválido.
- Tipos, unidades ou campos obrigatórios ausentes/incompatíveis.
- Ciclo repetido ou sessão diferente da sessão ativa em processamento.
- Medição desatualizada (idade $> 5\text{ segundos}$).

### 8.1. Comportamento Seguro em Caso de Falha
* **No Controlador:** recusa imediatamente os dados com retorno diferente de zero na CLI (`main.cpp`), emite diagnóstico de erro na telemetria e **não gera comandos fictícios nem atualiza medições**. A última leitura válida é preservada para fins de diagnóstico.
* **No Simulador:** o simulador da planta entra automaticamente em estado seguro (`motivo_seguro = "sem_arquivo"`, `"comando_invalido"` ou `"comando_expirado"`) e **zera ambos os atuadores (`bomba = 0.0`, `aquecedor = 0.0`)** se ficar mais de 5 ciclos sem um comando novo válido, evitando riscos físicos ao reservatório.

---

## 9. Interface supervisória

A interface será desenvolvida em Python utilizando Streamlit.

Deverá apresentar as seguintes informações e funcionalidades:

### 9.1. Monitoramento
- Nível atual do reservatório (%) e Temperatura atual (°C).
- Faixas de operação ativas e Estratégia de controle corrente.
- Estado atual dos atuadores observados (bomba e aquecedor).
- Idade da medição (tempo decorrido desde a última leitura válida da planta).

### 9.2. Configuração e Controle de Acesso
- Seleção da estratégia de controle ativa (`histerese` ou `modulada`).
- Edição de faixas normais ($N_{min}, N_{max}, T_{min}, T_{max}$).
- Painel de Administração de Segurança protegido por senha (`"adm123"`):
  - Habilitação/Desabilitação individual dos 4 alarmes (*bypass* de segurança).
  - Configuração dos limites numéricos críticos de cada alarme.
  - Configuração de Notificações por E-mail: endereço de destino, chave liga/desliga e seleção entre modo Real (SMTP) e modo Simulação (Mock).
- Validação automática dos campos antes do envio.

### 9.3. Alarmes e diagnósticos
- Painel visual de alarmes ativos com código, severidade e descrição clara.
- Indicação visual imediata em caso de aquisição obsoleta ou falha de comunicação.

### 9.4. Gráficos
- Gráficos temporais de Nível e Temperatura com linhas de referência das faixas operacionais e dos limites de alarme configurados.

### 9.5. Histórico, SQLite e Exportação CSV (Padrão Repository)
O sistema mantém o registro histórico das medições utilizando o banco de dados local SQLite (`dados/historico.db`).

* **Esquema relacional da tabela `historico`:**
  ```sql
  CREATE TABLE IF NOT EXISTS historico (
      sessao TEXT NOT NULL,
      ciclo INTEGER NOT NULL,
      timestamp TEXT NOT NULL,
      nivel REAL NOT NULL,
      temperatura REAL NOT NULL,
      bomba REAL NOT NULL,
      aquecedor REAL NOT NULL,
      ultimo_comando INTEGER NOT NULL,
      alarmes TEXT,
      PRIMARY KEY (sessao, ciclo)
  );
  ```
* **Garantia de Deduplicação:** A chave primária composta `PRIMARY KEY (sessao, ciclo)` impede estritamente que múltiplos re-renderings ou atualizações de tela do Streamlit dupliquem registros no banco de dados.
* **Exportação CSV:** O supervisório oferece um botão direto de download de arquivo CSV gerado a partir de consulta ao SQLite, permitindo análise externa.

### 9.6. Serviço de Notificações de Alarme por E-mail (Padrão Observer)
O supervisório conta com um serviço autônomo de envio de e-mails para notificar a equipe de operação quando alarmes forem disparados.
* **Disparo em Transição de Estado (Borda de Subida):** Para evitar envio excessivo de mensagens (*flood/spam* a cada ciclo de 1 segundo), o e-mail é disparado **estritamente na transição de estado** (quando o alarme muda de inativo para ativo). Quando a condição se normaliza e o alarme desarma por histerese, um e-mail de encerramento/normalização é emitido uma única vez.
* **Dois Modos de Operação:**
  * **Modo Real (SMTP):** Utiliza as bibliotecas nativas de Python (`smtplib` e `email.message`) para enviar a mensagem via servidor SMTP configurado.
  * **Modo Simulação / Mock:** Para testes automatizados e execução em ambientes sem internet ou sem credenciais SMTP (como a esteira de CI do GitHub), o notificador registra a mensagem formatada em memória e em arquivo (`dados/notificacoes_email.log`), permitindo validar e testar 100% da lógica.

---

## 10. Organização e padrões de projeto (POO)

Para atender plenamente aos Requisitos 3, 4 e 10, a arquitetura aplica conceitos fundamentais de Orientação a Objetos e quatro padrões clássicos de projeto (GoF):

### 10.1. Padrão Strategy (No Controlador C++: Como Calcular?)
* **Problema resolvido:** Desacoplar o algoritmo de decisão de controle do fluxo de aquisição e comunicação, permitindo substituir ou acrescentar novas estratégias de forma polimórfica sem alterar o controlador.
* **Classes participantes:**
  * `IEstrategiaControle`: Interface puramente virtual com o método `calcularAtuacao(leituras, metas)`.
  * `EstrategiaFaixaHisterese`: Implementação concreta da Estratégia A (atuação binária tudo-ou-nada por faixa com histerese).
  * `EstrategiaProporcionalModulada`: Implementação concreta da Estratégia B (atuação contínua modulada em torno do ponto de equilíbrio físico).
* **Alternativa mais simples descartada:** Estruturas `switch/case` ou `if/else` no loop principal, o que violaria o princípio Aberto/Fechado (OCP).

### 10.2. Padrão Command (Na Atuação C++: O Que Executar e Rastrear?)
* **Problema resolvido:** Encapsular a ordem de atuação como um objeto independente. A estratégia decide a ação, mas é o `Command` que valida as invariantes físicas dos atuadores ($[0.0, 100.0]$), associa um identificador sequencial `id` crescente, gerencia a serialização em `comando_planta.json` e rastreia se a ordem foi confirmada pela planta via `ultimo_comando` ("emissão não prova aplicação").
* **Classes participantes:**
  * `IComando`: Interface com métodos `executar()` e `foiConfirmado(ultimo_comando_planta)`.
  * `ComandoAtuacao`: Comando normal de controle gerado pelas estratégias.
  * `ComandoSeguranca`: Comando de prioridade que força intertravamentos (ex.: desligar aquecedor ou bomba imediatamente).
* **Alternativa mais simples descartada:** Gravação direta de valores numéricos em variáveis globais ou direto no arquivo JSON, sem identificador de comando nem rastreamento de confirmação.

### 10.3. Padrão Repository (Na Persistência Python: Como Isolar os Dados?)
* **Problema resolvido:** Desacoplar o código de interface gráfica Streamlit das operações SQL e do banco SQLite, facilitando testes e manutenção.
* **Classes participantes:**
  * `RepositorioHistorico`: Classe responsável pela conexão, criação da tabela `historico`, inserção idempotente (`salvar_registro`) e consulta com paginação/filtro para exportação CSV.
* **Alternativa mais simples descartada:** Execução de comandos SQL espalhados diretamente nos arquivos de renderização da tela.

### 10.4. Padrão Observer (Nos Alarmes e Notificações Python: Como Comunicar Mudanças?)
* **Problema resolvido:** Desacoplar o processamento e a detecção de alarmes dos múltiplos canais de notificação (interface gráfica, console, envio de e-mail), com garantia mandatória de **evitar notificações duplicadas a cada ciclo**.
* **Classes participantes:**
  * `MonitorAlarmes` (Subject): Acompanha o estado dos alarmes recebidos da telemetria e gerencia o histórico de transições de estado.
  * `IAlarmeObserver` (Interface do Observador): Declara o método `atualizar_alarme(evento_alarme)`.
  * `NotificadorTela` (Observador Concreto): Atualiza os componentes visuais e mensagens da interface Streamlit.
  * `NotificadorEmail` (Observador Concreto): Formata e despacha mensagens de alerta por e-mail apenas na mudança de estado (borda de subida/descida).
* **Alternativa mais simples descartada:** Funções proceduralmente aninhadas disparando e-mails a cada iteração do Streamlit, o que geraria centenas de e-mails repetidos por minuto.

---

## 11. Testes previstos e evidências

A validação do projeto é orientada a testes automatizados integrados ao `make test`:

### 11.1. Testes de Unidade C++ (`tests/cpp/autorais_*.cpp`)
* **Políticas de Controle (Strategy):**
  * Comparação lado a lado: com a mesma leitura de nível/temperatura, comprovar decisões distintas entre `EstrategiaFaixaHisterese` e `EstrategiaProporcionalModulada`.
  * Fronteiras de histerese e preservação de estado da bomba/aquecedor na banda morta.
* **Comandos e Rastreamento (Command):**
  * Verificação de geração de `id` crescente.
  * Rastreamento da confirmação através de `ultimo_comando`.
  * Garantia de idempotência: reler o mesmo ciclo não gera novo comando com id renovado.
* **Validação e Contrato:**
  * Recusa de leituras com unidades inválidas, strings, `NaN` ou infinito.
  * Validação de `solicitacao_operador.json` e preservação da configuração anterior em caso de erro.
* **Alarmes e Intertravamentos:**
  * Teste do intertravamento de segurança: aquecedor forçado a 0% quando nível $\le 15\%$, mesmo se temperatura for baixa.
  * Teste do alarme de nível alto: bomba forçada a 0% quando nível $\ge 85\%$.
  * Teste de desativação (histerese) dos alarmes.

### 11.2. Testes de Unidade Python (`tests/autorais_*.py`)
* **Repositório e SQLite (Repository):**
  * Inserção de medições idênticas não gera duplicação no banco de dados (`PRIMARY KEY (sessao, ciclo)`).
  * Consulta de histórico e consistência dos dados gerados para exportação CSV.
* **Serviço de Notificações e Anti-Spam (Observer):**
  * Disparo de e-mail ao ocorrer a transição de alarme inativo para ativo.
  * Garantia de não-duplicação: manter o alarme ativo em múltiplos ciclos consecutivos não dispara novos e-mails.
  * Envio de e-mail de encerramento na normalização do processo.
* **Validação de Interface:**
  * Validação dos limites de alarme com senha de administração (`"adm123"`).
  * Rejeição de faixas onde $min \ge max$.

### 11.3. Testes de Integração
* Execução em conjunto: simulador, controlador e supervisório trocando snapshots e demonstrando conformidade com `docs/CONTRATO.md`.

---

## 12. Decisões de projeto e definições fechadas

Todos os pontos operacionais e arquiteturais estão formalmente definidos no projeto:

1. **Limites Físicos e Operacionais:** Nível [0, 100]%, Temperatura [0, 100]°C.
2. **Faixas Padrão Iniciais:** Nível entre 40% e 70%; Temperatura entre 30°C e 50°C.
3. **Limites Críticos de Alarme:**
   * Nível muito baixo: $15.0\%$ (rearme em $20.0\%$).
   * Nível muito alto: $85.0\%$ (rearme em $80.0\%$).
   * Temperatura muito baixa: $15.0^\circ\text{C}$ (rearme em $18.0^\circ\text{C}$).
   * Temperatura muito alta: $75.0^\circ\text{C}$ (rearme em $70.0^\circ\text{C}$).
4. **Histerese de Segurança:** 5.0% para nível e 5.0°C / 3.0°C para temperatura.
5. **Comportamento Seguro na Perda de Dados:** Simulador desliga atuadores após 5 ciclos sem comando; controlador encerra ciclo com código de saída != 0 sem forjar medições.
6. **Operação Autônoma / Fallback:** O controlador inicia com configuração padrão e preserva o último pedido válido do supervisório em memória/estado.
7. **Versionamento de Solicitações:** `solicitacao_operador.json` usa `versao = 1`, `id` incremental positivo e timestamp UTC.
8. **Múltiplas Solicitações:** O controlador lê a versão mais recente e aplica na próxima medição válida recebida da planta.
9. **Estratégias Concretas (Strategy):** Estratégia A (Faixas/Histerese) e Estratégia B (Proporcional Modulada).
10. **Encapsulamento de Atuação (Command):** Ordens encapsuladas em objetos `ComandoAtuacao` e `ComandoSeguranca` com reconciliação de confirmação via `ultimo_comando`.
11. **Persistência Desacoplada (Repository):** Banco SQLite com tabela `historico` deduplicada por `PRIMARY KEY (sessao, ciclo)`.
12. **Comunicação de Eventos (Observer):** Monitor de alarmes no supervisório com notificação por e-mail em transição de estado e proteção anti-spam.
13. **Prioridade de Decisão:** 1º Intertravamentos Críticos $\rightarrow$ 2º Segurança por falha $\rightarrow$ 3º Estratégia de controle ativa.
14. **Recuperação de Sessão:** Controlador detecta nova sessão (`sessao`), reinicia contadores internos e sincroniza com `ultimo_comando`.
15. **Proteção de Acesso:** Senha `"adm123"` para alteração de alarmes e parâmetros de e-mail no supervisório.
16. **Ferramenta de Teste CLI:** Disponibilização de `tools/enviar_solicitacao.py` para testes sem o Streamlit.

---

## 13. Roteiro de desenvolvimento por etapas (`ETAPA_ATUAL`)

O desenvolvimento será guiado pelas branches e etapas do repositório:

* **Etapa 00 (Infraestrutura):** Ambiente configurado, build funcional e testes base da planta passando (`make test ETAPA=00`).
* **Etapa 01 (Aquisição C++):** Controlador lê `planta.json`, valida tipos/unidades, emite `telemetria.json` com lista de alarmes vazia e trata arquivos ausentes ou corrompidos. Testes autorais C++ cobrindo aquisição e validação.
* **Etapa 02 (Controle, Strategy e Command em C++):** Implementação das classes de estratégia (`Strategy`), objetos de comando e confirmação (`Command`), alarmes e intertravamentos de segurança. Testes autorais C++ cobrindo decisões distintas e rearme de alarmes.
* **Etapa 03 (Supervisório, Repository e Observer em Python):** Interface Streamlit completa, persistência SQLite (`Repository`), notificações de alarme por e-mail (`Observer`), exportação CSV e testes autorais Python (`tests/autorais_*.py`). Verificação de cobertura total via `make cobertura`.

---

## 14. Resultado esperado

Ao final do projeto, entrega-se um sistema robusto, modular e testado, demonstrando princípios avançados de POO (encapsulamento, herança, polimorfismo, Strategy, Command, Repository e Observer), garantindo total separação de responsabilidades e rastreabilidade através de testes automatizados e relatórios de cobertura.