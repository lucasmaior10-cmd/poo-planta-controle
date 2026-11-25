#include "dominio/medicao.hpp"

namespace controlador::dominio {

Medicao::Medicao(double valor) : valor_(0.0) {
    atualizar(valor);
}

void Medicao::atualizar(double valor) {
    if (!validarValor(valor)) {
        throw std::invalid_argument("medicao deve estar entre 0 e 100");
    }
    valor_ = valor;
}

double Medicao::valor() const noexcept {
    return valor_;
}

bool Medicao::ehValido() const noexcept {
    return validarValor(valor_);
}

bool Medicao::validarValor(double valor) noexcept {
    return std::isfinite(valor) && valor >= 0.0 && valor <= 100.0;
}

} // namespace controlador::dominio

