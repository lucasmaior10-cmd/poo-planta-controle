#pragma once
#include <string>

namespace controlador::servicos {

class PublicadorJson {
public:
    static bool publicarAtomico(const std::string& caminhoDestino,
                                const std::string& conteudo,
                                std::string& outErro);
};

} // namespace controlador::servicos

