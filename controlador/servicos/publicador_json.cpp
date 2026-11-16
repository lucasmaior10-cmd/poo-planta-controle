#include "servicos/publicador_json.hpp"
#include <fstream>
#include <filesystem>
#include <cstdio>

namespace controlador::servicos {

bool PublicadorJson::publicarAtomico(const std::string& caminhoDestino,
                                     const std::string& conteudo,
                                     std::string& outErro) {
    std::filesystem::path destPath(caminhoDestino);
    std::filesystem::path parentDir = destPath.parent_path();

    if (!parentDir.empty()) {
        std::error_code ecDir;
        std::filesystem::create_directories(parentDir, ecDir);
        if (ecDir) {
            outErro = "Falha ao criar diretorio pai: " + ecDir.message();
            return false;
        }
    }

    std::filesystem::path tempPath = destPath;
    tempPath += ".tmp";

    {
        std::ofstream arqTmp(tempPath, std::ios::out | std::ios::trunc);
        if (!arqTmp.is_open()) {
            outErro = "Nao foi possivel criar arquivo temporario: " + tempPath.string();
            return false;
        }

        arqTmp << conteudo;
        arqTmp.flush();

        if (!arqTmp.good()) {
            outErro = "Falha ao escrever no arquivo temporario: " + tempPath.string();
            arqTmp.close();
            std::filesystem::remove(tempPath);
            return false;
        }
        arqTmp.close();
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, destPath, ec);
    if (ec) {
        // Tentativa de remocao previa e renomeacao caso o filesystem exija
        std::filesystem::remove(destPath, ec);
        std::filesystem::rename(tempPath, destPath, ec);
        if (ec) {
            outErro = "Falha ao substituir arquivo de destino atomicamente: " + ec.message();
            std::filesystem::remove(tempPath);
            return false;
        }
    }

    return true;
}

} // namespace controlador::servicos

