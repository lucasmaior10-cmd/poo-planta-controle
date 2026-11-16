#include "dominio/telemetria.hpp"

namespace controlador::dominio {

Telemetria::Telemetria(Aquisicao aquisicao, std::vector<Alarme> alarmes)
    : aquisicao_(std::move(aquisicao)), alarmes_(std::move(alarmes)) {}

void Telemetria::adicionarAlarme(Alarme alarme) {
    alarmes_.push_back(std::move(alarme));
}

nlohmann::json Telemetria::paraJson() const {
    nlohmann::json j;
    j["versao"] = aquisicao_.versao();
    j["sessao"] = aquisicao_.sessao();
    j["ciclo"] = aquisicao_.ciclo();
    j["timestamp"] = aquisicao_.timestamp();
    j["leituras"] = aquisicao_.leiturasJson();
    j["atuadores"] = aquisicao_.atuadores().paraJson();
    j["ultimo_comando"] = aquisicao_.ultimoComando();
    j["erro_comando"] = aquisicao_.erroComandoJson();
    j["motivo_seguro"] = aquisicao_.motivoSeguroJson();

    nlohmann::json arrAlarmes = nlohmann::json::array();
    for (const auto& a : alarmes_) {
        arrAlarmes.push_back(a.paraJson());
    }
    j["alarmes"] = arrAlarmes;

    return j;
}

std::string Telemetria::serializar(int indent) const {
    return paraJson().dump(indent);
}

} // namespace controlador::dominio

