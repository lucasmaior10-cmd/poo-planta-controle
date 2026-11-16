#include <iostream>
#include <stdexcept>
#include <limits>
#include "dominio/medicao.hpp"
#include "dominio/atuacao.hpp"
#include "dominio/alarme.hpp"

using namespace controlador::dominio;

static void verificar(bool condicao, const char* mensagem) {
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

int main() {
    try {
        // --- Testes da classe Medicao ---
        Medicao medicao{50.0};
        verificar(medicao.valor() == 50.0, "Medicao: valor inicial incorreto");
        verificar(medicao.ehValido(), "Medicao: 50.0 deve ser valido");

        medicao.atualizar(0.0);
        verificar(medicao.valor() == 0.0, "Medicao: limite inferior 0.0 deve ser aceito");

        medicao.atualizar(100.0);
        verificar(medicao.valor() == 100.0, "Medicao: limite superior 100.0 deve ser aceito");

        // Rejeitar negativo
        bool rejeitouNegativo = false;
        try {
            medicao.atualizar(-1.0);
        } catch (const std::invalid_argument&) {
            rejeitouNegativo = true;
        }
        verificar(rejeitouNegativo, "Medicao: valor negativo deve ser rejeitado");
        verificar(medicao.valor() == 100.0, "Medicao: estado anterior deve ser preservado apos falha");

        // Rejeitar acima de 100
        bool rejeitouSuperior = false;
        try {
            medicao.atualizar(100.1);
        } catch (const std::invalid_argument&) {
            rejeitouSuperior = true;
        }
        verificar(rejeitouSuperior, "Medicao: valor > 100 deve ser rejeitado");

        // Rejeitar NaN
        bool rejeitouNaN = false;
        try {
            medicao.atualizar(std::numeric_limits<double>::quiet_NaN());
        } catch (const std::invalid_argument&) {
            rejeitouNaN = true;
        }
        verificar(rejeitouNaN, "Medicao: NaN deve ser rejeitado");

        // --- Testes da classe Atuacao ---
        Atuacao atuacao{30.0, 70.0};
        verificar(atuacao.bomba() == 30.0, "Atuacao: bomba inicial incorreta");
        verificar(atuacao.aquecedor() == 70.0, "Atuacao: aquecedor inicial incorreto");
        verificar(atuacao.ehValida(), "Atuacao: deve ser valida");

        atuacao.definirBomba(0.0);
        atuacao.definirAquecedor(100.0);
        verificar(atuacao.bomba() == 0.0 && atuacao.aquecedor() == 100.0, "Atuacao: limites devem ser aceitos");

        bool rejeitouBombaInvalida = false;
        try {
            atuacao.definirBomba(150.0);
        } catch (const std::invalid_argument&) {
            rejeitouBombaInvalida = true;
        }
        verificar(rejeitouBombaInvalida, "Atuacao: bomba > 100 deve ser rejeitada");

        auto jsonAtuadores = atuacao.paraJson();
        verificar(jsonAtuadores["bomba"] == 0.0 && jsonAtuadores["aquecedor"] == 100.0, "Atuacao: JSON invalido");
        Atuacao deserializada = Atuacao::deJson(jsonAtuadores);
        verificar(deserializada.bomba() == 0.0 && deserializada.aquecedor() == 100.0, "Atuacao: deJson falhou");

        // --- Testes da classe Alarme ---
        Alarme alarme("NIVEL_CRITICO", "Nivel critico baixo", "critico");
        verificar(alarme.codigo() == "NIVEL_CRITICO", "Alarme: codigo incorreto");
        verificar(alarme.mensagem() == "Nivel critico baixo", "Alarme: mensagem incorreta");
        verificar(alarme.severidade() == "critico", "Alarme: severidade incorreta");
        auto jsonAlarme = alarme.paraJson();
        verificar(jsonAlarme["codigo"] == "NIVEL_CRITICO", "Alarme: JSON com codigo errado");

        std::cout << "OK: Medicao, Atuacao e Alarme autorais verificados com sucesso\n";
        return 0;
    } catch (const std::exception& erro) {
        std::cerr << "FALHOU: " << erro.what() << "\n";
        return 1;
    }
}

