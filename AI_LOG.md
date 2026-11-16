# Registro de uso de IA da equipe

O código inicial fornecido pelo docente é identificado pelo primeiro commit.
Registre abaixo o uso de IA nas alterações da equipe; se não houve, registre isso.

| Data e ferramenta | Pedido | Aceito | Rejeitado | Justificativa técnica e validação |
|---|---|---|---|---|
| 2026-11-16 · Antigravity (Gemini 3.8 Flash) | Implementar requisitos da Etapa 01 no controlador C++: leitura/validação estrita de planta.json, escrita atômica de telemetria.json, classes de domínio e testes autorais | Classes de domínio (Medicao, Atuacao, Alarme, Aquisicao, Telemetria), PublicadorJson (rename atômico), CLI no main.cpp e testes C++ autorais (autorais_medicao e autorais_aquisicao) | N/A | Conformidade com CONTRATO.md, UML.md e ARQUITETURA.md. Validação via make test (15/15 testes aprovados) e conformidade estrita com g++ -Wall -Wextra -Werror -pedantic. |
