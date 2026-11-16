#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "dominio/atuacao.hpp"

namespace controlador::dominio {

class Aquisicao {
private:
    int versao_;
    std::string sessao_;
    int ciclo_;
    std::string timestamp_;
    double nivel_;
    double temperatura_;
    Atuacao atuadores_;
    int ultimoComando_;
    nlohmann::json leiturasJson_;
    nlohmann::json erroComandoJson_;
    nlohmann::json motivoSeguroJson_;

public:
    Aquisicao();

    Aquisicao(int versao,
              std::string sessao,
              int ciclo,
              std::string timestamp,
              double nivel,
              double temperatura,
              Atuacao atuadores,
              int ultimoComando,
              nlohmann::json leiturasJson,
              nlohmann::json erroComandoJson,
              nlohmann::json motivoSeguroJson);

    int versao() const noexcept { return versao_; }
    const std::string& sessao() const noexcept { return sessao_; }
    int ciclo() const noexcept { return ciclo_; }
    const std::string& timestamp() const noexcept { return timestamp_; }
    double nivel() const noexcept { return nivel_; }
    double temperatura() const noexcept { return temperatura_; }
    const Atuacao& atuadores() const noexcept { return atuadores_; }
    int ultimoComando() const noexcept { return ultimoComando_; }

    const nlohmann::json& leiturasJson() const noexcept { return leiturasJson_; }
    const nlohmann::json& erroComandoJson() const noexcept { return erroComandoJson_; }
    const nlohmann::json& motivoSeguroJson() const noexcept { return motivoSeguroJson_; }

    static bool deJson(const nlohmann::json& j, Aquisicao& out, std::string& outErro);
    static bool parseString(const std::string& conteudo, Aquisicao& out, std::string& outErro);
    static bool carregarDeArquivo(const std::string& caminhoArquivo, Aquisicao& out, std::string& outErro);
};

} // namespace controlador::dominio

