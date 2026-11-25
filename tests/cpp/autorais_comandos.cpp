#include <iostream>
#include <stdexcept>
#include "comandos/comando_atuacao.hpp"
#include "comandos/comando_seguranca.hpp"
#include <nlohmann/json.hpp>

using namespace controlador::comandos;

static void verificar(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

int main() {
    try {
        // ============================================================
        // ComandoAtuacao
        // ============================================================
        ComandoAtuacao cmd(1, "sessao-01", 75.0, 20.0);
        verificar(cmd.id() == 1,                "ComandoAtuacao: id incorreto");
        verificar(cmd.sessao() == "sessao-01",  "ComandoAtuacao: sessao incorreta");
        verificar(cmd.valorBomba() == 75.0,     "ComandoAtuacao: bomba incorreta");
        verificar(cmd.valorAquecedor() == 20.0, "ComandoAtuacao: aquecedor incorreto");

        // foiConfirmado: verdadeiro apenas quando ultimo_comando >= id
        verificar(!cmd.foiConfirmado(0), "ComandoAtuacao: nao deve estar confirmado (ultimo=0)");
        verificar( cmd.foiConfirmado(1), "ComandoAtuacao: deve estar confirmado (ultimo=1)");
        verificar( cmd.foiConfirmado(5), "ComandoAtuacao: deve estar confirmado (ultimo=5)");

        // JSON gerado deve estar conforme o CONTRATO
        std::string json = cmd.gerarJson();
        auto j = nlohmann::json::parse(json);
        verificar(j["versao"] == 1,              "JSON: versao deve ser 1");
        verificar(j["sessao"] == "sessao-01",    "JSON: sessao incorreta");
        verificar(j["id"] == 1,                  "JSON: id incorreto");
        verificar(j["atuadores"]["bomba"] == 75.0,     "JSON: bomba incorreta");
        verificar(j["atuadores"]["aquecedor"] == 20.0, "JSON: aquecedor incorreta");

        // ID deve ser positivo (ID 0 deve ser rejeitado)
        bool rejeitouIdInvalido = false;
        try { ComandoAtuacao bad(0, "s", 0.0, 0.0); }
        catch (const std::invalid_argument&) { rejeitouIdInvalido = true; }
        verificar(rejeitouIdInvalido, "ComandoAtuacao: ID 0 deve ser rejeitado");

        // ID incremental: cada instancia com id diferente
        ComandoAtuacao cmd2(2, "sessao-01", 0.0, 0.0);
        verificar(cmd2.id() == 2, "ComandoAtuacao: segundo id deve ser 2");
        verificar(!cmd2.foiConfirmado(1), "ComandoAtuacao: id=2 nao deve estar confirmado com ultimo=1");
        verificar( cmd2.foiConfirmado(2), "ComandoAtuacao: id=2 deve estar confirmado com ultimo=2");

        // Idempotência: mesmo id não reaplica (representado pelo foiConfirmado)
        // → se ultimo_comando já tinha confirmado id=2, cmd2 não emite novo comando
        verificar(cmd2.foiConfirmado(3), "ComandoAtuacao: id=2 confirmado com ultimo=3 (idempotente)");

        // ============================================================
        // ComandoSeguranca
        // ============================================================
        ComandoSeguranca seg(10, "sessao-seg", 100.0, 0.0);
        verificar(seg.id() == 10,                 "ComandoSeguranca: id incorreto");
        verificar(seg.sessao() == "sessao-seg",   "ComandoSeguranca: sessao incorreta");
        verificar(seg.valorBomba() == 100.0,      "ComandoSeguranca: bomba deve ser 100%");
        verificar(seg.valorAquecedor() == 0.0,    "ComandoSeguranca: aquecedor deve ser 0%");
        verificar(!seg.foiConfirmado(9),          "ComandoSeguranca: nao confirmado (ultimo=9)");
        verificar( seg.foiConfirmado(10),         "ComandoSeguranca: confirmado (ultimo=10)");

        // JSON do ComandoSeguranca também segue o contrato
        auto js = nlohmann::json::parse(seg.gerarJson());
        verificar(js["versao"] == 1,            "ComandoSeguranca JSON: versao errada");
        verificar(js["id"] == 10,               "ComandoSeguranca JSON: id errado");
        verificar(js["atuadores"]["bomba"] == 100.0,    "ComandoSeguranca JSON: bomba errada");
        verificar(js["atuadores"]["aquecedor"] == 0.0,  "ComandoSeguranca JSON: aquecedor errada");

        // ID inválido
        bool rejeitouSegInvalido = false;
        try { ComandoSeguranca bad(-1, "s", 0.0, 0.0); }
        catch (const std::invalid_argument&) { rejeitouSegInvalido = true; }
        verificar(rejeitouSegInvalido, "ComandoSeguranca: ID negativo deve ser rejeitado");

        std::cout << "OK: ComandoAtuacao e ComandoSeguranca verificados com sucesso\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALHOU: " << e.what() << "\n";
        return 1;
    }
}
