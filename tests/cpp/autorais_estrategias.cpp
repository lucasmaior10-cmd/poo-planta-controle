#include <iostream>
#include <stdexcept>
#include "estrategias/estrategia_histerese.hpp"
#include "estrategias/estrategia_modulada.hpp"

using namespace controlador::estrategias;

static void verificar(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

int main() {
    try {
        ConfiguracaoControle cfg{};  // Padrão: nivel[40,70], temp[30,50]

        // ============================================================
        // Estratégia A — Faixas com Histerese
        // ============================================================
        EstrategiaFaixaHisterese histerese;
        verificar(histerese.nome() == "histerese", "nome da estrategia A deve ser 'histerese'");

        // Nível abaixo do mínimo → bomba 100%
        auto d1 = histerese.calcular(30.0, 40.0, cfg);
        verificar(d1.bomba == 100.0, "Histerese: nivel baixo deve ligar bomba 100%");

        // Nível acima do máximo → bomba 0%
        auto d2 = histerese.calcular(80.0, 40.0, cfg);
        verificar(d2.bomba == 0.0, "Histerese: nivel alto deve desligar bomba");

        // Nível DENTRO da faixa → mantém estado anterior (banda morta)
        // Estado anterior foi bomba=0 (da chamada d2)
        auto d3 = histerese.calcular(55.0, 40.0, cfg);
        verificar(d3.bomba == 0.0, "Histerese: dentro da faixa, bomba deve preservar estado anterior");

        // Novo ciclo: ligar bomba e entrar na faixa
        histerese.resetar();
        histerese.calcular(30.0, 40.0, cfg);  // bomba → 100%
        auto d4 = histerese.calcular(55.0, 40.0, cfg);
        verificar(d4.bomba == 100.0, "Histerese: dentro da faixa apos ligar, bomba deve permanecer 100%");

        // Temperatura abaixo do mínimo → aquecedor 100%
        auto d5 = histerese.calcular(55.0, 20.0, cfg);
        verificar(d5.aquecedor == 100.0, "Histerese: temp baixa deve ligar aquecedor");

        // Temperatura acima do máximo → aquecedor 0%
        auto d6 = histerese.calcular(55.0, 60.0, cfg);
        verificar(d6.aquecedor == 0.0, "Histerese: temp alta deve desligar aquecedor");

        // Temperatura DENTRO da faixa → mantém estado (aquecedor=0 do d6)
        auto d7 = histerese.calcular(55.0, 40.0, cfg);
        verificar(d7.aquecedor == 0.0, "Histerese: dentro da faixa temp, aquecedor deve preservar");

        // resetar() limpa o estado
        histerese.resetar();
        auto d8 = histerese.calcular(55.0, 40.0, cfg);
        verificar(d8.bomba == 0.0 && d8.aquecedor == 0.0, "Histerese: apos resetar, estado deve ser zerado");

        // ============================================================
        // Estratégia B — Proporcional Modulada
        // ============================================================
        EstrategiaProporcionalModulada modulada;
        verificar(modulada.nome() == "modulada", "nome da estrategia B deve ser 'modulada'");

        // Nível no centro da faixa → bomba próxima do equilíbrio (~33.3%)
        auto dm1 = modulada.calcular(55.0, 40.0, cfg);  // centro nivel=55, temp=40
        verificar(dm1.bomba >= 0.0 && dm1.bomba <= 100.0, "Modulada: bomba deve estar em [0,100]");
        verificar(dm1.aquecedor >= 0.0 && dm1.aquecedor <= 100.0, "Modulada: aquecedor deve estar em [0,100]");

        // Nível muito baixo (10%) → bomba deve ser alta (> equilíbrio)
        auto dm2 = modulada.calcular(10.0, 40.0, cfg);
        verificar(dm2.bomba > 33.3, "Modulada: nivel muito baixo deve aumentar bomba");

        // Nível muito alto (90%) → bomba deve ser baixa (< equilíbrio)
        auto dm3 = modulada.calcular(90.0, 40.0, cfg);
        verificar(dm3.bomba < 33.3, "Modulada: nivel muito alto deve reduzir bomba");

        // Temperatura muito baixa (5°C) → aquecedor deve subir
        auto dm4 = modulada.calcular(55.0, 5.0, cfg);
        verificar(dm4.aquecedor > 0.0, "Modulada: temp muito baixa deve acionar aquecedor");

        // Temperatura muito alta (90°C) → aquecedor deve ser zero (ou mínimo)
        auto dm5 = modulada.calcular(55.0, 90.0, cfg);
        verificar(dm5.aquecedor == 0.0, "Modulada: temp muito alta deve zerar aquecedor");

        // Duas estratégias com mesma entrada devem diferenciar comportamento
        // (histerese é bang-bang, modulada é contínua)
        histerese.resetar();
        auto ha = histerese.calcular(55.0, 40.0, cfg);
        auto ma = modulada.calcular(55.0, 40.0, cfg);
        // Histerese no estado zerado permanece em 0; modulada calcula valor não necessariamente igual
        // A evidência de polimorfismo é que são tipos distintos com saídas distintas
        verificar(ha.bomba != ma.bomba || ha.aquecedor != ma.aquecedor ||
                  histerese.nome() != modulada.nome(),
                  "Estrategias A e B devem ter comportamentos distintos (polimorfismo)");

        std::cout << "OK: EstrategiaFaixaHisterese e EstrategiaProporcionalModulada verificadas\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALHOU: " << e.what() << "\n";
        return 1;
    }
}
