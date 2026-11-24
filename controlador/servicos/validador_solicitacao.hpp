#pragma once
#include <string>
#include "estrategias/configuracao_controle.hpp"
#include "servicos/gerenciador_seguranca.hpp"

namespace controlador::servicos {

// Configuração completa do controlador obtida da solicitação do operador.
struct ConfiguracaoOperador {
    int versao{1};
    int id{0};
    std::string timestamp{};
    std::string estrategia{"histerese"};

    estrategias::ConfiguracaoControle faixas{};
    ConfiguracaoAlarmes alarmes{};
};

// Lê e valida dados/solicitacao_operador.json.
//
// Regras de integração (conforme CONTRATO.md §5):
//   - Arquivo ausente ou inválido → retorna false e preserva a última config válida.
//   - Versão != 1 → rejeita.
//   - id <= idAnterior → rejeita (não é uma atualização nova).
//   - faixas: 0 <= min < max <= 100.0 obrigatório para nível e temperatura.
//   - estrategia: "histerese" ou "modulada".
class ValidadorSolicitacao {
public:
    // Tenta carregar e validar o arquivo. Retorna true se aceito.
    // 'idUltimaValida': último id já aceito (evita reprocessar a mesma solicitação).
    static bool carregar(const std::string& caminho,
                         int idUltimaValida,
                         ConfiguracaoOperador& out,
                         std::string& outErro);
};

} // namespace controlador::servicos
