#include "comandos/comando_atuacao.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace controlador::comandos {

ComandoAtuacao::ComandoAtuacao(int id,
                               std::string sessao,
                               double bomba,
                               double aquecedor)
    : id_(id), sessao_(std::move(sessao)), bomba_(bomba), aquecedor_(aquecedor) {
    if (id_ <= 0) {
        throw std::invalid_argument("ID do comando deve ser inteiro positivo");
    }
}

int ComandoAtuacao::id() const { return id_; }
const std::string& ComandoAtuacao::sessao() const { return sessao_; }
double ComandoAtuacao::valorBomba() const { return bomba_; }
double ComandoAtuacao::valorAquecedor() const { return aquecedor_; }

bool ComandoAtuacao::foiConfirmado(int ultimoComandoConfirmado) const {
    return ultimoComandoConfirmado >= id_;
}

std::string ComandoAtuacao::gerarJson() const {
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
