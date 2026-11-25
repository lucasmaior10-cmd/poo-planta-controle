#pragma once
#include "estrategias/i_estrategia_controle.hpp"

namespace controlador::estrategias {

// Estratégia A — Faixas com Histerese (Bang-Bang).
//
// Regras de nível:
//   - Se nivel < nivelMin  → bomba = 100%
//   - Se nivel > nivelMax  → bomba = 0%
//   - Caso contrário (dentro da faixa) → mantém o estado anterior da bomba
//
// Regras de temperatura:
//   - Se temp < tempMin    → aquecedor = 100%
//   - Se temp > tempMax    → aquecedor = 0%
//   - Caso contrário       → mantém o estado anterior do aquecedor
class EstrategiaFaixaHisterese : public IEstrategiaControle {
public:
    EstrategiaFaixaHisterese();

    std::string nome() const override;
    DecisaoAtuacao calcular(double nivel,
                            double temperatura,
                            const ConfiguracaoControle& config) override;
    void resetar() override;

private:
    double ultimaBomba_;
    double ultimoAquecedor_;
};

} // namespace controlador::estrategias
