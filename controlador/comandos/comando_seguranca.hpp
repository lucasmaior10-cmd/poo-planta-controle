#pragma once
#include "comandos/i_comando.hpp"

namespace controlador::comandos {

// Comando prioritário de intertravamento de segurança.
//
// Emitido pelo GerenciadorSeguranca quando uma variável atinge
// um limite crítico. Sempre força bomba e aquecedor a valores
// seguros pré-definidos (ex.: cortar aquecedor, zerar bomba).
// Tem a mesma interface de IComando para ser tratado de forma
// polimórfica pelo ControladorProcesso.
class ComandoSeguranca : public IComando {
public:
    ComandoSeguranca(int id,
                     std::string sessao,
                     double bomba,
                     double aquecedor);

    int id() const override;
    const std::string& sessao() const override;
    double valorBomba() const override;
    double valorAquecedor() const override;
    bool foiConfirmado(int ultimoComandoConfirmado) const override;
    std::string gerarJson() const override;

private:
    int id_;
    std::string sessao_;
    double bomba_;
    double aquecedor_;
};

} // namespace controlador::comandos
