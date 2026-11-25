#include "servicos/validador_solicitacao.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <nlohmann/json.hpp>

namespace controlador::servicos {

// Valida que um double é finito e está em [0, 100].
static bool percentualValido(double v) {
    return std::isfinite(v) && v >= 0.0 && v <= 100.0;
}

bool ValidadorSolicitacao::carregar(const std::string& caminho,
                                    int idUltimaValida,
                                    ConfiguracaoOperador& out,
                                    std::string& outErro) {
    // 1. Leitura do arquivo
    std::ifstream arq(caminho);
    if (!arq.is_open()) {
        outErro = "Arquivo solicitacao_operador.json ausente: " + caminho;
        return false;
    }
    std::stringstream buf;
    buf << arq.rdbuf();
    std::string conteudo = buf.str();

    if (conteudo.find_first_not_of(" \t\n\r") == std::string::npos) {
        outErro = "Arquivo solicitacao_operador.json vazio";
        return false;
    }

    // 2. Parse JSON
    auto j = nlohmann::json::parse(conteudo, nullptr, false);
    if (j.is_discarded()) {
        outErro = "Erro de sintaxe JSON em solicitacao_operador.json";
        return false;
    }

    if (!j.is_object()) {
        outErro = "solicitacao_operador.json deve ser objeto JSON";
        return false;
    }

    // 3. Campos obrigatórios
    for (const auto& campo : {"versao", "id", "timestamp", "estrategia", "faixas", "alarmes"}) {
        if (!j.contains(campo)) {
            outErro = std::string("Campo obrigatorio ausente: ") + campo;
            return false;
        }
    }

    // 4. versao == 1
    if (!j["versao"].is_number_integer() || j["versao"].is_boolean() || j["versao"].get<int>() != 1) {
        outErro = "Campo 'versao' deve ser o inteiro 1";
        return false;
    }

    // 5. id: inteiro positivo e maior que o último aceito
    if (!j["id"].is_number_integer() || j["id"].is_boolean() || j["id"].get<int>() <= 0) {
        outErro = "Campo 'id' deve ser inteiro positivo";
        return false;
    }
    int novoId = j["id"].get<int>();
    if (novoId <= idUltimaValida) {
        outErro = "id da solicitacao nao e mais recente que o ultimo aceito";
        return false;
    }

    // 6. timestamp: string não vazia
    if (!j["timestamp"].is_string() || j["timestamp"].get<std::string>().empty()) {
        outErro = "Campo 'timestamp' deve ser string nao vazia";
        return false;
    }

    // 7. estrategia
    if (!j["estrategia"].is_string()) {
        outErro = "Campo 'estrategia' deve ser string";
        return false;
    }
    std::string estrategia = j["estrategia"].get<std::string>();
    if (estrategia != "histerese" && estrategia != "modulada") {
        outErro = "Campo 'estrategia' invalido (use 'histerese' ou 'modulada')";
        return false;
    }

    // 8. faixas
    auto& faixas = j["faixas"];
    if (!faixas.is_object() || !faixas.contains("nivel") || !faixas.contains("temperatura")) {
        outErro = "Campo 'faixas' invalido";
        return false;
    }
    for (const std::string var : {"nivel", "temperatura"}) {
        auto& f = faixas[var];
        if (!f.is_object() || !f.contains("min") || !f.contains("max")) {
            outErro = "Faixa de '" + var + "' deve ter 'min' e 'max'";
            return false;
        }
        if (!f["min"].is_number() || f["min"].is_boolean() ||
            !f["max"].is_number() || f["max"].is_boolean()) {
            outErro = "Faixa de '" + var + "': min e max devem ser numericos";
            return false;
        }
        double mn = f["min"].get<double>();
        double mx = f["max"].get<double>();
        if (!percentualValido(mn) || !percentualValido(mx) || mn >= mx) {
            outErro = "Faixa de '" + var + "' invalida (0 <= min < max <= 100)";
            return false;
        }
    }

    // 9. alarmes (apenas valida presença de chaves e tipos; usa limites do hardware)
    auto& alarmes = j["alarmes"];
    if (!alarmes.is_object()) {
        outErro = "Campo 'alarmes' deve ser objeto";
        return false;
    }
    for (const std::string a : {"nivel_muito_baixo", "nivel_muito_alto",
                                 "temp_muito_baixa", "temp_muito_alta"}) {
        if (!alarmes.contains(a)) {
            outErro = "Alarme ausente em 'alarmes': " + a;
            return false;
        }
        auto& cfg = alarmes[a];
        if (!cfg.contains("habilitado") || !cfg.contains("limite")) {
            outErro = "Alarme '" + a + "' deve ter 'habilitado' e 'limite'";
            return false;
        }
        if (!cfg["habilitado"].is_boolean()) {
            outErro = "Alarme '" + a + "'.habilitado deve ser booleano";
            return false;
        }
        if (!cfg["limite"].is_number() || cfg["limite"].is_boolean()) {
            outErro = "Alarme '" + a + "'.limite deve ser numerico";
            return false;
        }
    }

    // --- Tudo válido: montar ConfiguracaoOperador ---
    ConfiguracaoOperador cfg{};
    cfg.versao    = 1;
    cfg.id        = novoId;
    cfg.timestamp = j["timestamp"].get<std::string>();
    cfg.estrategia = estrategia;

    cfg.faixas.nivelMin = faixas["nivel"]["min"].get<double>();
    cfg.faixas.nivelMax = faixas["nivel"]["max"].get<double>();
    cfg.faixas.tempMin  = faixas["temperatura"]["min"].get<double>();
    cfg.faixas.tempMax  = faixas["temperatura"]["max"].get<double>();

    // Limites de alarme — mantém valores padrão de rearme; apenas limites de disparo
    // e habilitação vêm do operador.
    cfg.alarmes.nivelMuitoBaixoHabilitado = alarmes["nivel_muito_baixo"]["habilitado"].get<bool>();
    cfg.alarmes.nivelMuitoBaixoLimite     = alarmes["nivel_muito_baixo"]["limite"].get<double>();

    cfg.alarmes.nivelMuitoAltoHabilitado  = alarmes["nivel_muito_alto"]["habilitado"].get<bool>();
    cfg.alarmes.nivelMuitoAltoLimite      = alarmes["nivel_muito_alto"]["limite"].get<double>();

    cfg.alarmes.tempMuitoBaixaHabilitada  = alarmes["temp_muito_baixa"]["habilitado"].get<bool>();
    cfg.alarmes.tempMuitoBaixaLimite      = alarmes["temp_muito_baixa"]["limite"].get<double>();

    cfg.alarmes.tempMuitoAltaHabilitada   = alarmes["temp_muito_alta"]["habilitado"].get<bool>();
    cfg.alarmes.tempMuitoAltaLimite       = alarmes["temp_muito_alta"]["limite"].get<double>();

    out = std::move(cfg);
    return true;
}

} // namespace controlador::servicos
