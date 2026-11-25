#pragma once
#include "comandos/i_comando.hpp"

namespace controlador::comandos {

// Comando normal de controle gerado pela estratégia ativa.
//
// Possui ID incremental positivo e verifica a confirmação via
// planta.json["ultimo_comando"] (princípio "emissão ≠ aplicação").
class ComandoAtuacao : public IComando {
public:
    ComandoAtuacao(int id,
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
