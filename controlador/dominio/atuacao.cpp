#include "dominio/atuacao.hpp"

namespace controlador::dominio {

Atuacao::Atuacao(double bomba, double aquecedor) : bomba_(0.0), aquecedor_(0.0) {
    definirBomba(bomba);
    definirAquecedor(aquecedor);
}

double Atuacao::bomba() const noexcept {
    return bomba_;
}

double Atuacao::aquecedor() const noexcept {
    return aquecedor_;
}

void Atuacao::definirBomba(double bomba) {
    if (!validarPercentual(bomba)) {
        throw std::invalid_argument("percentual da bomba deve estar entre 0 e 100");
    }
    bomba_ = bomba;
}

void Atuacao::definirAquecedor(double aquecedor) {
    if (!validarPercentual(aquecedor)) {
        throw std::invalid_argument("percentual do aquecedor deve estar entre 0 e 100");
    }
    aquecedor_ = aquecedor;
}

bool Atuacao::ehValida() const noexcept {
    return validarPercentual(bomba_) && validarPercentual(aquecedor_);
}

bool Atuacao::validarPercentual(double valor) noexcept {
    return std::isfinite(valor) && valor >= 0.0 && valor <= 100.0;
}

nlohmann::json Atuacao::paraJson() const {
    return nlohmann::json{
        {"bomba", bomba_},
        {"aquecedor", aquecedor_}
    };
}

Atuacao Atuacao::deJson(const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("bomba") || !j.contains("aquecedor")) {
        throw std::invalid_argument("JSON de atuacao invalido");
    }
    if (!j["bomba"].is_number() || j["bomba"].is_boolean() ||
        !j["aquecedor"].is_number() || j["aquecedor"].is_boolean()) {
        throw std::invalid_argument("valores de atuadores devem ser numericos");
    }
    double b = j["bomba"].get<double>();
    double a = j["aquecedor"].get<double>();
    return Atuacao(b, a);
}

} // namespace controlador::dominio

