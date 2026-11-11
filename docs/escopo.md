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
O controlador é projetado para operar de forma autônoma. Caso o arquivo `solicitacao_operador.json` não exista no diretório de dados (por exemplo, durante a execução dos testes automatizados da CLI com `--uma-vez` ou antes de o supervisório ser aberto), o controlador utilizará **parâmetros padrão embutidos**:
* **Faixa de nível padrão:** $N_{min} = 40.0\%$, $N_{max} = 70.0\%$
* **Faixa de temperatura padrão:** $T_{min} = 30.0^\circ\text{C}$, $T_{max} = 50.0^\circ\text{C}$
* **Estratégia padrão:** `histerese` (Estratégia 1)

Isso assegura que o controlador seja totalmente desacoplado da interface gráfica, garantindo a aprovação nos testes automatizados (`make test`).

### 5.2. Ciclo de atualização via supervisório
Quando o operador alterar uma ou ambas as faixas ou a estratégia no supervisório, o fluxo segue as etapas:
1. O supervisório valida os novos valores antes de publicá-los.
2. O supervisório publica o snapshot em `solicitacao_operador.json` de forma atômica.
3. O controlador lê e valida a solicitação.
4. **A nova configuração somente será aplicada na próxima medição válida recebida da planta.**
5. O controlador não recalcula decisões com medições antigas; a decisão ocorre sincronizada ao ciclo da planta.

### 5.3. Rejeição de configurações inválidas
Caso a configuração recebida seja inválida:
- O controlador rejeita a solicitação e emite diagnóstico na telemetria.
- A última configuração válida (ou a configuração padrão) é preservada.
- Nenhuma ação é executada com base nos valores incorretos.

---

## 6. Validação e formato das configurações do operador

### 6.1. Formato do arquivo `solicitacao_operador.json`
O arquivo gravado pelo supervisório seguirá a seguinte estrutura:

```json
{
  "versao": 1,
  "id": 1,
  "timestamp": "2026-11-11T15:00:00Z",
  "estrategia": "histerese",
  "faixas": {
    "nivel": {"min": 40.0, "max": 70.0},
    "temperatura": {"min": 30.0, "max": 50.0}
  }
}
```

* `versao`: número inteiro 1.
* `id`: inteiro sequencial crescente das solicitações do operador.
* `timestamp`: instante UTC no formato ISO 8601.
* `estrategia`: string identificando a estratégia ativa (`"histerese"` ou `"modulada"`).
* `faixas`: limites mínimos e máximos para nível e temperatura.

### 6.2. Regras de validação (Supervisório e Controlador)
Ambas as aplicações realizam validações estritas:
1. **Tipos e completude:** todos os campos obrigatórios presentes e numéricos finitos (rejeição de `NaN`, `null` ou strings nos valores).
2. **Coerência de limites:** $min < max$ para cada grandeza.
3. **Fronteiras operacionais permitidas:**
   * Nível: $0.0 \le min < max \le 100.0$
   * Temperatura: $0.0 \le min < max \le 100.0$
4. **Estratégia suportada:** campo `estrategia` deve pertencer ao conjunto `{"histerese", "modulada"}`.

---

## 7. Alarmes e intertravamentos de segurança (Requisito 7)

O sistema possui alarmes de segurança com regras, fronteiras numéricas explícitas e histerese para rearme. As ações de segurança possuem **prioridade absoluta** sobre os comandos normais de controle.

### 7.1. Alarme 1: Nível Criticamente Baixo (`NIVEL_CRITICO_BAIXO`)
Evita a queima por funcionamento a seco dos componentes e danos ao reservatório.
* **Condição de disparo:** $\text{nível} \le 15.0\%$
* **Condição de rearme (histerese):** $\text{nível} \ge 20.0\%$
* **Severidade:** `"critico"`
* **Ações automáticas mandatórias:**
  * **Intertravamento imediato do aquecedor:** aquecedor forçado a $0.0\%$, mesmo que a temperatura esteja abaixo do mínimo.
  * **Recuperação de nível:** a bomba é comandada a $100.0\%$ para restabelecer o volume mínimo com urgência.

### 7.2. Alarme 2: Temperatura Excessiva (`TEMP_EXCESSIVA`)
Evita superaquecimento, fervura ou degradação do líquido e dos sensores.
* **Condição de disparo:** $\text{temperatura} \ge 75.0^\circ\text{C}$
* **Condição de rearme (histerese):** $\text{temperatura} \le 70.0^\circ\text{C}$
* **Severidade:** `"critico"`
* **Ações automáticas mandatórias:**
  * **Intertravamento imediato do aquecedor:** aquecedor forçado a $0.0\%$.

### 7.3. Diagnóstico e Alarme de Comunicação: Aquisição Obsoleta (`AQUISICAO_OBSOLETA`)
Identifica perda de comunicação ou travamento do simulador da planta.
* **Condição de disparo:** idade da última medição válida $> 5.0\text{ segundos}$ ou ciclo congelado por mais de 5 ciclos de amostragem.
* **Severidade:** `"aviso"`
* **Ações automáticas:** exibição visual de alerta de dados antigos no supervisório e suspensão de novas atuações pelo controlador.

### 7.4. Prioridade de Decisão
A ordem hierárquica de decisão dos atuadores é estrita:
1. **Intertravamentos de Segurança / Alarmes Críticos** (prioridade máxima).
2. **Proteção contra dados inválidos / timeout** (modo seguro com atuadores zerados).
3. **Política de Controle da Estratégia Ativa** (faixa de operação solicitada).

---

## 8. Diagnóstico de aquisição de dados

O controlador deverá identificar situações em que não seja possível obter medições válidas da planta.

Exemplos:

- Arquivo `planta.json` inexistente.
- Arquivo vazio ou corrompido.
- Dados incompletos.
- Valores inválidos.
- Medições desatualizadas.

O supervisório deverá apresentar um diagnóstico informando a condição da aquisição.

Sempre que possível, deverá ser exibido o tempo decorrido desde a última medição válida recebida.

O comportamento seguro dos atuadores durante a perda de aquisição ainda deverá ser definido.

---

## 9. Interface supervisória

A interface será desenvolvida em Python utilizando Streamlit.

Deverá apresentar as seguintes informações:

### 9.1. Monitoramento

- Nível atual do reservatório.
- Temperatura atual do líquido.
- Faixa de operação configurada para cada variável.
- Estado atual da bomba.
- Estado atual do aquecedor.

### 9.2. Configuração

- Campo para configurar nível mínimo.
- Campo para configurar nível máximo.
- Campo para configurar temperatura mínima.
- Campo para configurar temperatura máxima.
- Validação dos valores antes do envio.
- Indicação do estado da solicitação de configuração.

### 9.3. Alarmes e diagnósticos

- Exibição de alarmes ativos.
- Identificação do tipo de alarme.
- Registro dos eventos.
- Diagnóstico de falhas na aquisição.
- Indicação da idade da última medição válida.

### 9.4. Gráficos

A interface deverá apresentar gráficos de tendência para:

- Nível do líquido ao longo do tempo.
- Temperatura ao longo do tempo.

Os gráficos deverão permitir acompanhar as variações das medições e visualizar as faixas de operação utilizadas pelo controlador.

### 9.5. Histórico e exportação

O sistema deverá manter um histórico dos dados do processo utilizando SQLite.

Os registros deverão incluir, quando disponíveis:

- Data e horário.
- Identificação da sessão da planta.
- Número do ciclo de aquisição.
- Nível medido.
- Temperatura medida.
- Faixas de operação utilizadas.
- Estado da bomba.
- Estado do aquecedor.
- Confirmação dos comandos.
- Alarmes e diagnósticos.

O supervisório deverá permitir exportar os registros para CSV.

A atualização da interface não poderá gerar registros duplicados. Para isso, deverá ser utilizada uma identificação que permita distinguir uma nova medição de uma atualização de tela referente à mesma medição.

---

## 10. Organização e padrões de projeto

O desenvolvimento deverá utilizar padrões de projeto que facilitem a manutenção e a expansão do sistema.

### 10.1. Strategy

Será utilizado para organizar as estratégias de controle.

A ideia é permitir que diferentes estratégias de controle possam ser implementadas e substituídas sem modificar toda a estrutura do controlador.

As estratégias concretas e sua divisão ainda deverão ser definidas.

### 10.2. Command

Será utilizado para representar as solicitações e ações de controle.

Permitirá organizar os comandos enviados aos atuadores e acompanhar sua confirmação pela planta.

### 10.3. Repository

Poderá ser utilizado para abstrair o acesso ao banco SQLite, separando as operações de armazenamento da lógica principal do supervisório.

---

## 11. Testes previstos

O sistema deverá ser submetido a testes individuais e integrados.

### 11.1. Testes da lógica de controle

- Ligar a bomba quando o nível atingir o mínimo.
- Desligar a bomba quando o nível atingir o máximo.
- Manter o estado da bomba dentro da faixa.
- Ligar o aquecedor quando a temperatura atingir o mínimo.
- Desligar o aquecedor quando a temperatura atingir o máximo.
- Manter o estado do aquecedor dentro da faixa.

### 11.2. Testes de configuração

- Aceitar configurações válidas.
- Rejeitar mínimo maior que máximo.
- Rejeitar valores não numéricos.
- Rejeitar campos ausentes.
- Rejeitar valores fora dos limites permitidos.
- Preservar a última configuração válida após uma rejeição.
- Aplicar a nova configuração somente na próxima medição válida.

### 11.3. Testes de segurança

- Acionar o alarme de nível criticamente baixo.
- Desligar o aquecedor em condição de nível inseguro.
- Permitir a recuperação do nível pela bomba quando permitido.
- Acionar o alarme de temperatura excessiva.
- Desligar o aquecedor em condição de temperatura excessiva.

### 11.4. Testes de comunicação

- Identificar arquivos JSON inexistentes.
- Identificar arquivos corrompidos.
- Identificar campos ausentes.
- Identificar medições inválidas.
- Identificar medições desatualizadas.
- Evitar a repetição de comandos ao reler o mesmo ciclo.
- Verificar o recebimento e a confirmação dos comandos.

### 11.5. Testes do supervisório

- Validar os campos de configuração.
- Exibir corretamente os valores recebidos.
- Exibir alarmes e diagnósticos.
- Registrar os dados no SQLite.
- Evitar registros duplicados durante atualizações de tela.
- Exportar corretamente os dados em CSV.

### 11.6. Teste integrado

Ao final, deverá ser realizado um teste envolvendo os três programas simultaneamente, verificando o fluxo completo de comunicação e controle.

---

## 12. Requisitos ainda pendentes de definição

Os seguintes pontos deverão ser definidos durante o desenvolvimento:

1. Limites operacionais mínimos e máximos do nível.
2. Limites operacionais mínimos e máximos da temperatura.
3. Valor do nível crítico baixo.
4. Valor da temperatura máxima de segurança.
5. Condições de recuperação e rearme dos alarmes.
6. Necessidade de histerese adicional nos alarmes.
7. Comportamento dos atuadores durante a perda ou desatualização das medições.
8. Comportamento do controlador caso o supervisório deixe de enviar configurações.
9. Identificação, versão, horário e validade das solicitações de configuração.
10. Tratamento de múltiplas configurações recebidas antes da próxima medição válida.
11. Estratégias concretas que serão implementadas utilizando o padrão Strategy.
12. Limites físicos e operacionais dos atuadores definidos pelo simulador.
13. Prioridade entre comandos normais e condições de segurança.
14. Recuperação do estado do controlador após reinicialização.
15. Estrutura definitiva das tabelas do SQLite.
16. Estrutura definitiva dos quatro arquivos JSON.

Uma eventual implementação de alarme de nível excessivamente alto, com bloqueio da bomba para evitar transbordamento, poderá ser avaliada posteriormente, mas não faz parte dos requisitos aprovados até o momento.

---

## 13. Fora do escopo nesta etapa

Nesta etapa, o foco será a definição do escopo funcional e da arquitetura geral do sistema.

Não fazem parte deste documento:

- Diagrama de classes UML.
- Implementação completa dos programas.
- Definição definitiva de todos os campos dos arquivos JSON.
- Definição final de todos os limites numéricos.
- Implementação de comunicação industrial física, como Modbus ou comunicação serial.

Esses elementos poderão ser desenvolvidos em etapas posteriores.

---

## 14. Resultado esperado

Ao final do projeto, espera-se obter um sistema composto por três aplicações independentes, capazes de monitorar e controlar automaticamente um reservatório térmico.

O operador deverá conseguir configurar as faixas de operação por meio de uma interface gráfica, acompanhar as variáveis do processo, visualizar os estados dos atuadores, receber alarmes e consultar o histórico de funcionamento.

A arquitetura deverá manter a separação entre simulação, controle e supervisão, garantindo que todas as decisões de acionamento sejam realizadas pelo controlador e que as configurações sejam validadas antes de sua aplicação.