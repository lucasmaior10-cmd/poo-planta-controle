#include "estrategias/estrategia_modulada.hpp"
#include <algorithm>

namespace controlador::estrategias {

EstrategiaProporcionalModulada::EstrategiaProporcionalModulada() {}

std::string EstrategiaProporcionalModulada::nome() const {
    return "modulada";
}

DecisaoAtuacao EstrategiaProporcionalModulada::calcular(double nivel,
                                                         double temperatura,
                                                         const ConfiguracaoControle& config) {
    // --- Nível ---
    double centroNivel = (config.nivelMin + config.nivelMax) / 2.0;
    double meioNivel   = (config.nivelMax - config.nivelMin) / 2.0;

    double erroNivel = centroNivel - nivel;    // positivo = precisa mais bomba
    double desvioNivel = (meioNivel > 0.0) ? (erroNivel / meioNivel) : 0.0;
    desvioNivel = std::clamp(desvioNivel, -1.0, 1.0);

    double bomba = kBombaEquilibrio_ + desvioNivel * kGanhoBomba_;
    bomba = std::clamp(bomba, 0.0, 100.0);

    // --- Temperatura ---
    double centroTemp = (config.tempMin + config.tempMax) / 2.0;
    double meioTemp   = (config.tempMax - config.tempMin) / 2.0;

    double erroTemp = centroTemp - temperatura;   // positivo = precisa mais calor
    double desvioTemp = (meioTemp > 0.0) ? (erroTemp / meioTemp) : 0.0;
    desvioTemp = std::clamp(desvioTemp, -1.0, 1.0);

    double aquecedor = kAquecedorEquilibrio_ + desvioTemp * kGanhoAquecedor_;
    aquecedor = std::clamp(aquecedor, 0.0, 100.0);

    return DecisaoAtuacao{bomba, aquecedor};
}

void EstrategiaProporcionalModulada::resetar() {
    // Estratégia proporcional não mantém estado persistente — sem ação necessária.
}

} // namespace controlador::estrategias
