#include "comandos/comando_seguranca.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace controlador::comandos {

ComandoSeguranca::ComandoSeguranca(int id,
                                   std::string sessao,
                                   double bomba,
                                   double aquecedor)
    : id_(id), sessao_(std::move(sessao)), bomba_(bomba), aquecedor_(aquecedor) {
    if (id_ <= 0) {
        throw std::invalid_argument("ID do comando de seguranca deve ser inteiro positivo");
    }
}

int ComandoSeguranca::id() const { return id_; }
const std::string& ComandoSeguranca::sessao() const { return sessao_; }
double ComandoSeguranca::valorBomba() const { return bomba_; }
double ComandoSeguranca::valorAquecedor() const { return aquecedor_; }

bool ComandoSeguranca::foiConfirmado(int ultimoComandoConfirmado) const {
    return ultimoComandoConfirmado >= id_;
}

std::string ComandoSeguranca::gerarJson() const {
    nlohmann::json j;
    j["versao"]  = 1;
    j["sessao"]  = sessao_;
    j["id"]      = id_;
    j["atuadores"] = {
        {"bomba",      bomba_},
        {"aquecedor",  aquecedor_}
    };
    return j.dump(2);
}

} // namespace controlador::comandos
