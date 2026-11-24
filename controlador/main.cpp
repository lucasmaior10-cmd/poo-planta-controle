#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include "servicos/controlador_processo.hpp"

int main(int argc, char* argv[]) {
    std::string dirDados = "dados";
    bool umaVez = false;
    int intervaloMs = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ajuda") {
            std::cout << "Uso: controlador [--dados DIRETORIO] [--uma-vez] [--ajuda]\n";
            return 0;
        } else if (arg == "--dados" && i + 1 < argc) {
            dirDados = argv[++i];
        } else if (arg == "--uma-vez") {
            umaVez = true;
        } else if (arg == "--intervalo" && i + 1 < argc) {
            intervaloMs = std::stoi(argv[++i]);
        } else {
            std::cerr << "Argumento desconhecido: " << arg << "\n";
            return 1;
        }
    }

    controlador::servicos::ControladorProcesso ctrl(dirDados);

    if (umaVez) {
        return ctrl.executarCiclo();
    }

    // Modo contínuo: executa ciclos até sinal de interrupção
    while (true) {
        int rc = ctrl.executarCiclo();
        if (rc != 0) {
            // Erros fatais encerram; ciclos repetidos (rc==0) continuam normalmente
            std::cerr << "[Controlador] Ciclo encerrado com codigo " << rc << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(intervaloMs));
    }

    return 0;
}
