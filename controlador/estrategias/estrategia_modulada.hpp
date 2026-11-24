#pragma once
#include "estrategias/i_estrategia_controle.hpp"

namespace controlador::estrategias {

// Estratégia B — Proporcional Modulada (atuação contínua suave).
//
// A atuação é calculada proporcionalmente ao desvio da variável
// em relação ao centro da faixa operacional:
//
//   desvio_norm = (setpoint - valor) / (metade_faixa)   → clampado em [-1, 1]
//   bomba      = bombaEquilibrio    + desvio_nivel  * ganho_bomba
//   aquecedor  = aquecedorEquil    + desvio_temp   * ganho_aquecedor
//
// Ponto de equilíbrio:
//   - bomba:     ~33.3% (compensa consumo de 1 ponto/ciclo com bomba = 3 pontos/ciclo)
//   - aquecedor:  0%    (perda térmica passiva tende a 20°C sem aquecimento adicional)
class EstrategiaProporcionalModulada : public IEstrategiaControle {
public:
    EstrategiaProporcionalModulada();

    std::string nome() const override;
    DecisaoAtuacao calcular(double nivel,
                            double temperatura,
                            const ConfiguracaoControle& config) override;
    void resetar() override;

private:
    // Ganhos do controlador proporcional
    static constexpr double kGanhoBomba_       = 50.0;
    static constexpr double kGanhoAquecedor_   = 30.0;

    // Pontos de equilíbrio (percentuais)
    static constexpr double kBombaEquilibrio_       = 33.3;
    static constexpr double kAquecedorEquilibrio_   =  0.0;
};

} // namespace controlador::estrategias
