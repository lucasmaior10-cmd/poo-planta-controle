#include "estrategias/estrategia_histerese.hpp"

namespace controlador::estrategias {

EstrategiaFaixaHisterese::EstrategiaFaixaHisterese()
    : ultimaBomba_(0.0), ultimoAquecedor_(0.0) {}

std::string EstrategiaFaixaHisterese::nome() const {
    return "histerese";
}

DecisaoAtuacao EstrategiaFaixaHisterese::calcular(double nivel,
                                                   double temperatura,
                                                   const ConfiguracaoControle& config) {
    // --- Bomba (controle de nível) ---
    if (nivel < config.nivelMin) {
        ultimaBomba_ = 100.0;         // Nível abaixo do mínimo → ligar bomba
    } else if (nivel > config.nivelMax) {
        ultimaBomba_ = 0.0;           // Nível acima do máximo → desligar bomba
    }
    // Dentro da faixa: preserva o último estado (histerese / banda morta)

    // --- Aquecedor (controle de temperatura) ---
    if (temperatura < config.tempMin) {
        ultimoAquecedor_ = 100.0;     // Temperatura abaixo do mínimo → ligar aquecedor
    } else if (temperatura > config.tempMax) {
        ultimoAquecedor_ = 0.0;       // Temperatura acima do máximo → desligar
    }
    // Dentro da faixa: preserva o último estado

    return DecisaoAtuacao{ultimaBomba_, ultimoAquecedor_};
}

void EstrategiaFaixaHisterese::resetar() {
    ultimaBomba_    = 0.0;
    ultimoAquecedor_ = 0.0;
}

} // namespace controlador::estrategias
