#include <iostream>
#include <stdexcept>
#include "servicos/gerenciador_seguranca.hpp"

using namespace controlador::servicos;

static void verificar(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

static bool temAlarme(const ResultadoSeguranca& r, const std::string& codigo) {
    for (const auto& a : r.alarmes) {
        if (a.codigo() == codigo) return true;
    }
    return false;
}

int main() {
    try {
        GerenciadorSeguranca gs;
        ConfiguracaoAlarmes cfg{};  // Padrão do projeto

        // ============================================================
        // Nível muito baixo (≤ 15%) → alarme + intertravamento
        // ============================================================
        auto r1 = gs.avaliar(15.0, 40.0, "s1", 1, cfg);
        verificar(temAlarme(r1, "NIVEL_MUITO_BAIXO"), "Nivel 15%: alarme NIVEL_MUITO_BAIXO ausente");
        verificar(r1.comandoSeguranca != nullptr, "Nivel 15%: intertravamento deve existir");
        verificar(r1.comandoSeguranca->valorBomba() == 100.0, "Intertravamento nivel baixo: bomba deve ser 100%");
        verificar(r1.comandoSeguranca->valorAquecedor() == 0.0, "Intertravamento nivel baixo: aquecedor deve ser 0%");

        // Histerese de rearme: nível 18% ainda deve manter alarme ativo (rearme em 20%)
        auto r2 = gs.avaliar(18.0, 40.0, "s1", 2, cfg);
        verificar(temAlarme(r2, "NIVEL_MUITO_BAIXO"), "Nivel 18%: alarme deve permanecer ativo (rearme=20%)");
        verificar(r2.comandoSeguranca != nullptr, "Nivel 18%: intertravamento deve permanecer");

        // Rearme: nível 20% → alarme desativa
        auto r3 = gs.avaliar(20.0, 40.0, "s1", 3, cfg);
        verificar(!temAlarme(r3, "NIVEL_MUITO_BAIXO"), "Nivel 20%: alarme NIVEL_MUITO_BAIXO deve rearmar");
        verificar(r3.comandoSeguranca == nullptr, "Nivel 20%: sem intertravamento apos rearme");

        // ============================================================
        // Nível muito alto (≥ 85%) → alarme + intertravamento
        // ============================================================
        gs.resetar();
        auto r4 = gs.avaliar(85.0, 40.0, "s1", 1, cfg);
        verificar(temAlarme(r4, "NIVEL_MUITO_ALTO"), "Nivel 85%: alarme NIVEL_MUITO_ALTO ausente");
        verificar(r4.comandoSeguranca != nullptr, "Nivel 85%: intertravamento deve existir");
        verificar(r4.comandoSeguranca->valorBomba() == 0.0, "Intertravamento nivel alto: bomba deve ser 0%");

        // Histerese de rearme: nível 82% ainda ativo (rearme em 80%)
        auto r5 = gs.avaliar(82.0, 40.0, "s1", 2, cfg);
        verificar(temAlarme(r5, "NIVEL_MUITO_ALTO"), "Nivel 82%: alarme deve permanecer ativo (rearme=80%)");

        // Rearme: nível 80% → alarme desativa
        auto r6 = gs.avaliar(80.0, 40.0, "s1", 3, cfg);
        verificar(!temAlarme(r6, "NIVEL_MUITO_ALTO"), "Nivel 80%: alarme NIVEL_MUITO_ALTO deve rearmar");
        verificar(r6.comandoSeguranca == nullptr, "Nivel 80%: sem intertravamento apos rearme");

        // ============================================================
        // Temperatura muito alta (≥ 75°C) → alarme + intertravamento
        // ============================================================
        gs.resetar();
        auto r7 = gs.avaliar(55.0, 75.0, "s1", 1, cfg);
        verificar(temAlarme(r7, "TEMP_MUITO_ALTA"), "Temp 75C: alarme TEMP_MUITO_ALTA ausente");
        verificar(r7.comandoSeguranca != nullptr, "Temp 75C: intertravamento deve existir");
        verificar(r7.comandoSeguranca->valorAquecedor() == 0.0, "Intertravamento temp alta: aquecedor deve ser 0%");

        // Histerese de rearme: 72°C ainda ativo (rearme em 70°C)
        auto r8 = gs.avaliar(55.0, 72.0, "s1", 2, cfg);
        verificar(temAlarme(r8, "TEMP_MUITO_ALTA"), "Temp 72C: alarme deve permanecer ativo (rearme=70C)");

        // Rearme: 70°C → alarme desativa
        auto r9 = gs.avaliar(55.0, 70.0, "s1", 3, cfg);
        verificar(!temAlarme(r9, "TEMP_MUITO_ALTA"), "Temp 70C: alarme TEMP_MUITO_ALTA deve rearmar");
        verificar(r9.comandoSeguranca == nullptr, "Temp 70C: sem intertravamento apos rearme");

        // ============================================================
        // Temperatura muito baixa (≤ 15°C) → só alarme, sem intertravamento
        // ============================================================
        gs.resetar();
        auto r10 = gs.avaliar(55.0, 15.0, "s1", 1, cfg);
        verificar(temAlarme(r10, "TEMP_MUITO_BAIXA"), "Temp 15C: alarme TEMP_MUITO_BAIXA ausente");
        // Temperatura baixa não deve causar intertravamento de atuadores
        verificar(r10.comandoSeguranca == nullptr, "Temp 15C: nao deve haver intertravamento para temp baixa");

        // Histerese: 16°C ainda ativo (rearme em 18°C)
        auto r11 = gs.avaliar(55.0, 16.0, "s1", 2, cfg);
        verificar(temAlarme(r11, "TEMP_MUITO_BAIXA"), "Temp 16C: alarme deve permanecer ativo");

        // Rearme: 18°C → alarme desativa
        auto r12 = gs.avaliar(55.0, 18.0, "s1", 3, cfg);
        verificar(!temAlarme(r12, "TEMP_MUITO_BAIXA"), "Temp 18C: alarme TEMP_MUITO_BAIXA deve rearmar");

        // ============================================================
        // Sem alarmes: nível e temperatura dentro das faixas seguras
        // ============================================================
        gs.resetar();
        auto r13 = gs.avaliar(55.0, 40.0, "s1", 1, cfg);
        verificar(r13.alarmes.empty(), "Sem alarmes: lista de alarmes deve estar vazia");
        verificar(r13.comandoSeguranca == nullptr, "Sem alarmes: nao deve haver intertravamento");

        // ============================================================
        // resetar() limpa o estado de histerese
        // ============================================================
        // Ativa alarme de nivel baixo
        gs.avaliar(10.0, 40.0, "s1", 1, cfg);
        gs.resetar();
        // Após reset, nível 18% (antes em estado ativo) não deve mais gerar alarme
        auto r14 = gs.avaliar(18.0, 40.0, "s1", 2, cfg);
        verificar(!temAlarme(r14, "NIVEL_MUITO_BAIXO"),
                  "Apos resetar, nivel 18% nao deve gerar alarme (estado limpo)");

        std::cout << "OK: GerenciadorSeguranca, alarmes e intertravamentos verificados\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALHOU: " << e.what() << "\n";
        return 1;
    }
}
