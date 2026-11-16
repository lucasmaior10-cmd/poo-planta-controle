#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace controlador::dominio {

class Alarme {
private:
    std::string codigo_;
    std::string mensagem_;
    std::string severidade_;

public:
    Alarme(std::string codigo, std::string mensagem, std::string severidade);

    const std::string& codigo() const noexcept;
    const std::string& mensagem() const noexcept;
    const std::string& severidade() const noexcept;

    nlohmann::json paraJson() const;
};

} // namespace controlador::dominio

