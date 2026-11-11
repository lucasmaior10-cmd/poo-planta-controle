## Requisitos da versão obrigatória
1. Executar planta, controlador C++ e supervisório em processos separados.

2. Adquirir pelo menos duas grandezas e representar pelo menos dois atuadores,
usando o tanque fornecido ou cenário equivalente documentado.
3. Implementar uma interface de política e pelo menos duas estratégias de
controle com comportamentos distintos e testáveis. A equipe escolhe e justifica
as estratégias; mudar apenas o nome de duas classes não caracteriza variação.
4. Implementar ao menos dois padrões com necessidade demonstrada. Strategy
atende à variação de política; o segundo pode ser Command, Repository,
Observer ou outro justificado. Não são exigidas hierarquias de sensores artificiais.
5. Ler planta.json , validar a aquisição e produzir comando_planta.json e
telemetria.json conforme o contrato. Aguardar e mostrar a confirmação da planta.
6. Tratar arquivo ausente, JSON inválido, unidade/tipo incompatível, repetição de
ciclo, reinício da planta e dados antigos. Leituras inválidas não viram medições novas.
7. Implementar pelo menos dois alarmes relevantes, com regra e fronteira explícitas,
além do diagnóstico de aquisição indisponível ou obsoleta.
8. Mostrar em Streamlit leituras, unidades, atuação observada, confirmação do
comando, alarmes e idade da aquisição; apresentar gráficos das duas grandezas.
9. Persistir histórico em SQLite, deduplicado por sessão/ciclo, e oferecer
exportação CSV. Atualizações da tela não podem duplicar registros.
10. Estender a suíte com testes dos objetos criados pela equipe, integrados ao
make test , e apresentar relatórios de cobertura C++ e Python pelo
make cobertura . Fornecer UML, README, registro de IA e histórico de contribuição.
Não são obrigatórios sockets, hardware, serviço de nuvem, autenticação, múltiplos
usuários nem comandos manuais pela tela. O controle automático pode funcionar
com configuração local. Caso a equipe acrescente comandos de operador, eles
devem passar pelo controlador; a tela não escreve na entrada da planta.

## Responsabilidades e padrões

| Componente |Deve fazer |Limite da responsabilidade|
|Planta| simular estado, perturbações e resposta aosatuadores| não escolher ações para atingir uma meta|
|Controlador| validar aquisição, aplicar política, solicitar atuação e emitir telemetria| não desenhar a tela nem gerar medições aleatórias|

|Supervisório | validar telemetria, apresentar e registrar o histórico| não recalcular a política de controle|

Encapsulamento deve proteger estados e invariantes. Composição deve tornar claras
as colaborações. Herança e polimorfismo devem aparecer na substituição de políticas
por uma interface comum. A mesma política precisa ser testável sem abrir Streamlit
nem depender de números aleatórios gerados durante o teste.

|Técnica/Padrão| Melhor uso |Esforço |Entregável |Limitação
|Strategy |variar a política de controle |médio | interface e duas políticas testadas |variação precisa ter motivo|

|Command |identificar solicitações de atuação | médio | comando associado à confirmação |emissão não prova aplicação|
|Repository |isolar persistência e consultas |médio| histórico testável sem tela| exige definir consultas e identidade |

| Observer | comunicar mudanças e alarmes | médio | notificações desacopladas | exige evitar duplicação |

Para começar, considere Strategy no controlador e Command na atuação. Para
organizar a persistência, considere Repository. A equipe deve explicar o problema
resolvido, as classes participantes, os testes e uma alternativa mais simples.

##  Contrato e operação dos arquivos

O documento técnico completo acompanha a base em docs/CONTRATO.md 

| Arquivo | Origem | Destino | Finalidade |
|---|---|---|---|
| `planta.json` | Simulador | Controlador | Disponibilizar os dados atuais da planta. |
| `solicitacao_operador.json` | Supervisório | Controlador | Enviar as faixas de operação configuradas pelo operador. |
| `comando_planta.json` | Controlador | Simulador | Enviar os comandos dos atuadores. |
| `telemetria.json` | Controlador | Supervisório | Disponibilizar medições, estados, alarmes e diagnósticos. |

A aquisição contém versao , sessao , ciclo , timestamp , leituras ,
atuadores , ultimo_comando , erro_comando e motivo_seguro .
Cada leitura informa tag , tipo , valor , unidade e status .
O comando informa versao , sessao , id e os dois percentuais de atuação.
A telemetria preserva a origem da aquisição e acrescenta a lista alarmes .

O par sessão/ciclo identifica a aquisição; timestamp registra seu instante em UTC.
Reiniciar a planta gera outra sessão. O controlador não emite novo comando ao
reler o mesmo ciclo. A planta confirma o id aceito no estado seguinte e zera os
atuadores após cinco ciclos sem um novo comando válido, por padrão.
Cada escritor publica um JSON completo por arquivo temporário e substituição do
destino. O snapshot anterior pode continuar legível após uma falha; o supervisor
deve sinalizar obsolescência. Defina no README o prazo de dados antigos e como
os processos recuperam a comunicação. Os snapshots podem perder ciclos se o
consumidor for mais lento: não anuncie histórico sem perdas.
Para outro domínio, mantenha o envelope e negocie mudanças de tags, unidades,
atuadores e testes. Não remova verificações apenas para ocultar incompatibilidade.


## Evidências de teste
Cada objeto criado ou alterado pela equipe deve ter seus comportamentos
relevantes verificados por testes autorais. Para cada responsabilidade, registre
caso comum, fronteira e falha quando aplicáveis, com resultado esperado e estado
que deve ser preservado. Justifique os casos que não se aplicam. Testes que apenas
instanciam objetos ou executam métodos sem verificar resultados são insuficientes.
O starter compila e executa tests/cpp/autorais_*.cpp junto às implementações do
controlador e descobre tests/autorais_*.py com unittest . Os testes de objetos
C++ não ligam o main do aplicativo. Desde a etapa 01 são exigidos testes autorais
C++; desde a 03, também Python. A quantidade mínima confirma a integração à
automação; não substitui verificar os objetos reais desenvolvidos pela equipe.
Os exemplos didáticos fornecidos ensinam o mecanismo e não contam, sozinhos,
como evidência de cobertura dos objetos da entrega.
Além dos contratos fornecidos, escreva testes autorais para:
as duas políticas com entradas conhecidas e decisões diferentes;
limites da faixa de controle, valores de fronteira e preservação de estado;
recusa de leitura inválida e tratamento de dados antigos;
emissão, confirmação e expiração de comando, sem renovar ao reler um ciclo;
reinício de sessão e recuperação de arquivo inválido;
persistência e consulta em SQLite, sem duplicar ao atualizar a tela;
um caminho completo da planta até a telemetria e a tela.
Use diretórios temporários e sementes fixas quando a simulação for necessária.
Os testes de regra devem preferir valores escolhidos a sorteios. A matriz do
README relaciona objeto/método, requisito, caso, resultado esperado, teste e
lacuna conhecida. A automação fornecida cobre a base
e os contratos; não certifica os padrões, o controle nem toda a persistência.
Testes e workflow visíveis no fork são complementados pela revisão e defesa.
Demonstração obrigatória: execute make test na etapa final, mostre os nomes
dos seus testes na saída e explique o objeto e a decisão que cada caso verifica.
Execute também make cobertura com o Python do ambiente virtual indicado no
README e apresente os relatórios em build/relatorios/cpp/index.html e
build/relatorios/python/index.html . Mostre linhas e ramificações dos arquivos
autorais e explique uma lacuna relevante ou a evidência de que os caminhos
planejados foram exercitados. A CI executa a mesma suíte e disponibiliza os relatórios.
Cobertura mede execução de código, sem garantir correção do resultado. Não há
percentual mínimo arbitrário: a avaliação considera pertinência dos testes,
verificação de resultados, caminhos exercitados e justificativa das lacunas.

Não use o percentual da planta fornecida como evidência de cobertura do código
autoral nem exclua esse código para elevar artificialmente a métrica.

##  Uso de IA e domínio integral do código
IA é permitida para planejar, gerar, revisar, testar e documentar. Cada aluno
deve conseguir explicar 100% do código entregue: propósito, dados, fluxo,
condições, falhas, testes e decisões, incluindo código recebido e gerado por IA.
Delegar a geração não transfere a responsabilidade técnica.
Registre em AI_LOG.md : data, ferramenta, pedido, o que foi aceito ou rejeitado,
justificativa técnica e como foi validado. Referencie commits quando útil.
Se não usou IA, registre isso. Cada integrante deve conseguir alterar um pequeno
comportamento e prever seu efeito em teste durante a defesa.
Não se presume autoria pela aparência do código ou por parâmetros diferentes.
A avaliação usa explicação, histórico, testes, revisão e adaptação supervisionada.

## Documentação e demonstração
O README final contém: cenário, como obter dependências, como compilar e executar
os três processos, parâmetros, limites do modelo, diagrama de classes, contratos,
estratégias, padrões, testes, persistência, divisão de trabalho e limitações.
Forneça comandos reproduzíveis em ambiente limpo e registre as dependências C++.
Na demonstração, mostre o estado inicial, uma ação e sua confirmação, a mudança
das próximas leituras, duas políticas, um alarme, uma falha com recuperação e o
histórico persistido após reiniciar a tela. Demonstre seus testes de objetos
rodando pelo Make e interprete os relatórios de cobertura. A exportação CSV deve
conter os dados consultados. Prepare a execução previamente para que a explicação
caiba nos 10–12 minutos por dupla, além da entrevista individual.
Como estimativa de planejamento, considere 24 horas combinadas de trabalho da
dupla: 2h para arquitetura e planta, 4h para aquisição, 6h para controle, 5h para
supervisório/persistência, 4h para testes e 3h para documentação e defesa. É uma
referência de esforço, sem somar automaticamente essas horas à carga presencial.