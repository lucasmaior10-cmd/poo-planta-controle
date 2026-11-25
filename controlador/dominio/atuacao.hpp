#pragma once
#include <cmath>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace controlador::dominio {

class Atuacao {
private:
    double bomba_;
    double aquecedor_;

public:
    explicit Atuacao(double bomba = 0.0, double aquecedor = 0.0);

    double bomba() const noexcept;
    double aquecedor() const noexcept;
    void definirBomba(double bomba);
    void definirAquecedor(double aquecedor);
    bool ehValida() const noexcept;

    static bool validarPercentual(double valor) noexcept;

    nlohmann::json paraJson() const;
    static Atuacao deJson(const nlohmann::json& j);
};

} // namespace controlador::dominio

