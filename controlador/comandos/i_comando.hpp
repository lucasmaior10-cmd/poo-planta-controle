#pragma once
#include <string>

namespace controlador::comandos {

// Interface (padrão Command) que representa uma ordem de atuação
// encapsulada como objeto rastreável.
//
// Separa a decisão (Strategy) do transporte JSON e da reconciliação
// de confirmação ("emissão não prova aplicação").
class IComando {
public:
    virtual ~IComando() = default;

    // ID incremental positivo único desta ordem dentro da sessão.
    virtual int id() const = 0;

    // Sessão da planta para a qual o comando foi emitido.
    virtual const std::string& sessao() const = 0;

    // Percentuais calculados para os atuadores.
    virtual double valorBomba() const = 0;
    virtual double valorAquecedor() const = 0;

    // Retorna true quando a planta já confirmou este ID em ultimo_comando.
    virtual bool foiConfirmado(int ultimoComandoConfirmado) const = 0;

    // Serializa o comando no formato exigido pelo CONTRATO.md para
    // gravar em dados/comando_planta.json.
    virtual std::string gerarJson() const = 0;
};

} // namespace controlador::comandos
