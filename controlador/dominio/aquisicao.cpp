#include "dominio/aquisicao.hpp"
#include <fstream>
#include <sstream>
#include <cmath>

namespace controlador::dominio {

Aquisicao::Aquisicao()
    : versao_(0),
      sessao_(""),
      ciclo_(0),
      timestamp_(""),
      nivel_(0.0),
      temperatura_(0.0),
      atuadores_(0.0, 0.0),
      ultimoComando_(0),
      leiturasJson_(nlohmann::json::array()),
      erroComandoJson_(nullptr),
      motivoSeguroJson_(nullptr) {}

Aquisicao::Aquisicao(int versao,
                     std::string sessao,
                     int ciclo,
                     std::string timestamp,
                     double nivel,
                     double temperatura,
                     Atuacao atuadores,
                     int ultimoComando,
                     nlohmann::json leiturasJson,
                     nlohmann::json erroComandoJson,
                     nlohmann::json motivoSeguroJson)
    : versao_(versao),
      sessao_(std::move(sessao)),
      ciclo_(ciclo),
      timestamp_(std::move(timestamp)),
      nivel_(nivel),
      temperatura_(temperatura),
      atuadores_(atuadores),
      ultimoComando_(ultimoComando),
      leiturasJson_(std::move(leiturasJson)),
      erroComandoJson_(std::move(erroComandoJson)),
      motivoSeguroJson_(std::move(motivoSeguroJson)) {}

bool Aquisicao::deJson(const nlohmann::json& j, Aquisicao& out, std::string& outErro) {
    if (!j.is_object()) {
        outErro = "JSON raiz deve ser um objeto";
        return false;
    }

    const std::vector<std::string> camposObrigatorios = {
        "versao", "sessao", "ciclo", "timestamp", "leituras", "atuadores", "ultimo_comando"
    };
    for (const auto& campo : camposObrigatorios) {
        if (!j.contains(campo)) {
            outErro = "Campo obrigatorio ausente: " + campo;
            return false;
        }
    }

    // versao: inteiro 1
    if (!j["versao"].is_number_integer() || j["versao"].is_boolean() || j["versao"].get<int>() != 1) {
        outErro = "Campo 'versao' deve ser o inteiro 1";
        return false;
    }
    int versao = j["versao"].get<int>();

    // sessao: string nao vazia
    if (!j["sessao"].is_string() || j["sessao"].get<std::string>().empty()) {
        outErro = "Campo 'sessao' deve ser uma string nao vazia";
        return false;
    }
    std::string sessao = j["sessao"].get<std::string>();

    // ciclo: inteiro positivo (> 0)
    if (!j["ciclo"].is_number_integer() || j["ciclo"].is_boolean() || j["ciclo"].get<int>() <= 0) {
        outErro = "Campo 'ciclo' deve ser um inteiro positivo maior que zero";
        return false;
    }
    int ciclo = j["ciclo"].get<int>();

    // timestamp: string nao vazia
    if (!j["timestamp"].is_string() || j["timestamp"].get<std::string>().empty()) {
        outErro = "Campo 'timestamp' deve ser uma string nao vazia";
        return false;
    }
    std::string timestamp = j["timestamp"].get<std::string>();

    // atuadores: objeto com bomba e aquecedor em [0, 100]
    if (!j["atuadores"].is_object() || !j["atuadores"].contains("bomba") || !j["atuadores"].contains("aquecedor")) {
        outErro = "Campo 'atuadores' deve ser um objeto contendo 'bomba' e 'aquecedor'";
        return false;
    }
    if (!j["atuadores"]["bomba"].is_number() || j["atuadores"]["bomba"].is_boolean() ||
        !j["atuadores"]["aquecedor"].is_number() || j["atuadores"]["aquecedor"].is_boolean()) {
        outErro = "Valores de 'bomba' e 'aquecedor' devem ser numericos";
        return false;
    }
    double bomba = j["atuadores"]["bomba"].get<double>();
    double aquecedor = j["atuadores"]["aquecedor"].get<double>();
    if (!Atuacao::validarPercentual(bomba) || !Atuacao::validarPercentual(aquecedor)) {
        outErro = "Valores de 'bomba' e 'aquecedor' devem estar entre 0.0 e 100.0";
        return false;
    }
    Atuacao atuadores(bomba, aquecedor);

    // ultimo_comando: inteiro >= 0
    if (!j["ultimo_comando"].is_number_integer() || j["ultimo_comando"].is_boolean() || j["ultimo_comando"].get<int>() < 0) {
        outErro = "Campo 'ultimo_comando' deve ser um inteiro nao negativo";
        return false;
    }
    int ultimoComando = j["ultimo_comando"].get<int>();

    // erro_comando opcional
    nlohmann::json erroComando = nullptr;
    if (j.contains("erro_comando")) {
        if (!j["erro_comando"].is_null() && !j["erro_comando"].is_string()) {
            outErro = "Campo 'erro_comando' deve ser string ou null";
            return false;
        }
        erroComando = j["erro_comando"];
    }

    // motivo_seguro opcional
    nlohmann::json motivoSeguro = nullptr;
    if (j.contains("motivo_seguro")) {
        if (!j["motivo_seguro"].is_null() && !j["motivo_seguro"].is_string()) {
            outErro = "Campo 'motivo_seguro' deve ser string ou null";
            return false;
        }
        motivoSeguro = j["motivo_seguro"];
    }

    // leituras: array nao vazio
    if (!j["leituras"].is_array() || j["leituras"].empty()) {
        outErro = "Campo 'leituras' deve ser um array nao vazio";
        return false;
    }

    bool encontrouNivel = false;
    bool encontrouTemp = false;
    double nivelVal = 0.0;
    double tempVal = 0.0;

    for (const auto& item : j["leituras"]) {
        if (!item.is_object()) {
            outErro = "Cada leitura deve ser um objeto";
            return false;
        }
        if (!item.contains("tag") || !item.contains("tipo") || !item.contains("valor") ||
            !item.contains("unidade") || !item.contains("status")) {
            outErro = "Item de leitura com campos obrigatorios ausentes";
            return false;
        }
        if (!item["tag"].is_string() || !item["tipo"].is_string() ||
            !item["unidade"].is_string() || !item["status"].is_string()) {
            outErro = "Tipos invalidos nos campos da leitura";
            return false;
        }

        std::string tipo = item["tipo"].get<std::string>();
        std::string unidade = item["unidade"].get<std::string>();
        std::string status = item["status"].get<std::string>();

        if (tipo == "nivel") {
            if (unidade != "%") {
                outErro = "Unidade de nivel incompativel: esperado '%'";
                return false;
            }
            if (status == "operando") {
                if (!item["valor"].is_number() || item["valor"].is_boolean()) {
                    outErro = "Valor de nivel deve ser numerico";
                    return false;
                }
                double v = item["valor"].get<double>();
                if (!std::isfinite(v) || v < 0.0 || v > 100.0) {
                    outErro = "Valor de nivel fora da faixa [0.0, 100.0]";
                    return false;
                }
                nivelVal = v;
                encontrouNivel = true;
            }
        } else if (tipo == "temperatura") {
            if (unidade != "C") {
                outErro = "Unidade de temperatura incompativel: esperado 'C'";
                return false;
            }
            if (status == "operando") {
                if (!item["valor"].is_number() || item["valor"].is_boolean()) {
                    outErro = "Valor de temperatura deve ser numerico";
                    return false;
                }
                double v = item["valor"].get<double>();
                if (!std::isfinite(v) || v < 0.0 || v > 100.0) {
                    outErro = "Valor de temperatura fora da faixa [0.0, 100.0]";
                    return false;
                }
                tempVal = v;
                encontrouTemp = true;
            }
        }
    }

    if (!encontrouNivel) {
        outErro = "Nenhuma leitura valida de nivel em operacao foi encontrada";
        return false;
    }
    if (!encontrouTemp) {
        outErro = "Nenhuma leitura valida de temperatura em operacao foi encontrada";
        return false;
    }

    out = Aquisicao(versao,
                    std::move(sessao),
                    ciclo,
                    std::move(timestamp),
                    nivelVal,
                    tempVal,
                    atuadores,
                    ultimoComando,
                    j["leituras"],
                    erroComando,
                    motivoSeguro);
    return true;
}

bool Aquisicao::parseString(const std::string& conteudo, Aquisicao& out, std::string& outErro) {
    if (conteudo.empty()) {
        outErro = "Arquivo ou conteudo JSON vazio";
        return false;
    }
    auto j = nlohmann::json::parse(conteudo, nullptr, false);
    if (j.is_discarded()) {
        outErro = "Erro de sintaxe JSON ao analisar aquisicao";
        return false;
    }
    return deJson(j, out, outErro);
}

bool Aquisicao::carregarDeArquivo(const std::string& caminhoArquivo, Aquisicao& out, std::string& outErro) {
    std::ifstream arq(caminhoArquivo);
    if (!arq.is_open()) {
        outErro = "Nao foi possivel abrir o arquivo: " + caminhoArquivo;
        return false;
    }
    std::stringstream buffer;
    buffer << arq.rdbuf();
    std::string conteudo = buffer.str();

    // Rejeitar arquivo em branco
    if (conteudo.find_first_not_of(" \t\n\r") == std::string::npos) {
        outErro = "Arquivo esta vazio: " + caminhoArquivo;
        return false;
    }

    return parseString(conteudo, out, outErro);
}

} // namespace controlador::dominio

