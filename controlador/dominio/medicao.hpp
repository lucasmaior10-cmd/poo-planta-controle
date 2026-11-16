#pragma once
#include <cmath>
#include <stdexcept>

namespace controlador::dominio {

class Medicao {
private:
    double valor_;

public:
    explicit Medicao(double valor = 0.0);

    void atualizar(double valor);
    double valor() const noexcept;
    bool ehValido() const noexcept;
    static bool validarValor(double valor) noexcept;
};

} // namespace controlador::dominio

