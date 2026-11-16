#pragma once
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "dominio/aquisicao.hpp"
#include "dominio/alarme.hpp"

namespace controlador::dominio {

class Telemetria {
private:
    Aquisicao aquisicao_;
    std::vector<Alarme> alarmes_;

public:
    explicit Telemetria(Aquisicao aquisicao, std::vector<Alarme> alarmes = {});

    const Aquisicao& aquisicao() const noexcept { return aquisicao_; }
    const std::vector<Alarme>& alarmes() const noexcept { return alarmes_; }

    void adicionarAlarme(Alarme alarme);

    nlohmann::json paraJson() const;
    std::string serializar(int indent = 2) const;
};

} // namespace controlador::dominio

