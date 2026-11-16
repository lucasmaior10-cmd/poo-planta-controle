#include <iostream>
#include <string>
#include <filesystem>
#include "dominio/aquisicao.hpp"
#include "dominio/telemetria.hpp"
#include "servicos/publicador_json.hpp"

using namespace controlador::dominio;
using namespace controlador::servicos;

int main(int argc, char* argv[]) {
    std::string dirDados = "dados";
    bool umaVez = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ajuda") {
            std::cout << "Uso: controlador [--dados DIRETORIO] [--uma-vez] [--ajuda]\n";
            return 0;
        } else if (arg == "--dados" && i + 1 < argc) {
            dirDados = argv[++i];
        } else if (arg == "--uma-vez") {
            umaVez = true;
        } else {
            std::cerr << "Argumento desconhecido: " << arg << "\n";
            return 1;
        }
    }

    std::filesystem::path caminhoDados(dirDados);
    std::filesystem::path caminhoPlanta = caminhoDados / "planta.json";
    std::filesystem::path caminhoTelemetria = caminhoDados / "telemetria.json";

    // 1. Verificar se o arquivo planta.json existe
    if (!std::filesystem::exists(caminhoPlanta)) {
        std::cerr << "Erro: Arquivo planta.json nao encontrado em: " << caminhoPlanta.string() << "\n";
        return 1;
    }

    // 2. Leitura e validacao estrita do planta.json
    Aquisicao aquisicao;
    std::string erroAquisicao;
    if (!Aquisicao::carregarDeArquivo(caminhoPlanta.string(), aquisicao, erroAquisicao)) {
        std::cerr << "Erro: Falha na aquisicao da planta: " << erroAquisicao << "\n";
        return 2;
    }

    // 3. Geracao e publicacao atomica da telemetria.json
    Telemetria telemetria(aquisicao, {});
    std::string jsonTelemetria = telemetria.serializar(2);

    std::string erroPublicacao;
    if (!PublicadorJson::publicarAtomico(caminhoTelemetria.string(), jsonTelemetria, erroPublicacao)) {
        std::cerr << "Erro ao gravar telemetria: " << erroPublicacao << "\n";
        return 3;
    }

    if (umaVez) {
        return 0;
    }

    return 0;
}
