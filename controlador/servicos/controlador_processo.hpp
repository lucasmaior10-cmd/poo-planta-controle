#pragma once
#include <string>
#include <memory>
#include "estrategias/i_estrategia_controle.hpp"
#include "servicos/gerenciador_seguranca.hpp"
#include "servicos/validador_solicitacao.hpp"
#include "dominio/aquisicao.hpp"

namespace controlador::servicos {

// Orquestrador central do controlador C++.
//
// A cada ciclo ele executa (conforme ARQUITETURA.md §6):
//   1. Verifica ciclo repetido (idempotência).
//   2. Reconcilia confirmação do último comando.
//   3. Atualiza configuração do operador (solicitacao_operador.json).
//   4. Avalia segurança / intertravamentos (GerenciadorSeguranca).
//   5. Chama a estratégia ativa se não houver intertravamento.
//   6. Publica comando_planta.json e telemetria.json atomicamente.
class ControladorProcesso {
public:
    explicit ControladorProcesso(const std::string& dirDados);

    // Executa um ciclo completo. Retorna 0 em sucesso, != 0 em erro fatal.
    int executarCiclo();

private:
    std::string dirDados_;

    // Estado persistente entre ciclos
    std::string sessaoAtual_{};
    int ultimoCicloProcessado_{0};
    int proximoIdComando_{1};
    int idUltimaSolicitacaoValida_{0};

    // Configuração ativa (padrão ou última válida do operador)
    ConfiguracaoOperador configAtiva_{};

    // Estratégia ativa (Strategy)
    std::unique_ptr<estrategias::IEstrategiaControle> estrategiaAtiva_;

    // Gerenciador de segurança e alarmes
    GerenciadorSeguranca gerenciadorSeguranca_{};

    // Troca para a estratégia indicada em config.estrategia se diferente da atual.
    void atualizarEstrategia(const std::string& nome);

    // Detecta nova sessão da planta e reinicia o estado interno.
    void tratarMudancaDeSessao(const dominio::Aquisicao& aq);
};

} // namespace controlador::servicos
