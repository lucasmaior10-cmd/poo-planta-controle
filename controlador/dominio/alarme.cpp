#include "dominio/alarme.hpp"

namespace controlador::dominio {

Alarme::Alarme(std::string codigo, std::string mensagem, std::string severidade)
    : codigo_(std::move(codigo)),
      mensagem_(std::move(mensagem)),
      severidade_(std::move(severidade)) {}

const std::string& Alarme::codigo() const noexcept {
    return codigo_;
}

const std::string& Alarme::mensagem() const noexcept {
    return mensagem_;
}

const std::string& Alarme::severidade() const noexcept {
    return severidade_;
}

nlohmann::json Alarme::paraJson() const {
    return nlohmann::json{
        {"codigo", codigo_},
        {"mensagem", mensagem_},
        {"severidade", severidade_}
    };
}

} // namespace controlador::dominio

