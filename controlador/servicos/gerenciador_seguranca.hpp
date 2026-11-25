#pragma once
#include <string>
#include <vector>
#include <memory>
#include "dominio/alarme.hpp"
#include "comandos/i_comando.hpp"

namespace controlador::servicos {

// Configuração dos limites críticos de alarme.
// Recebida do ValidadorSolicitacao ou usada com valores padrão.
struct ConfiguracaoAlarmes {
    bool nivelMuitoBaixoHabilitado{true};
    double nivelMuitoBaixoLimite{15.0};
    double nivelMuitoBaixoRearme{20.0};

    bool nivelMuitoAltoHabilitado{true};
    double nivelMuitoAltoLimite{85.0};
    double nivelMuitoAltoRearme{80.0};

    bool tempMuitoBaixaHabilitada{true};
    double tempMuitoBaixaLimite{15.0};
    double tempMuitoBaixaRearme{18.0};

    bool tempMuitoAltaHabilitada{true};
    double tempMuitoAltaLimite{75.0};
    double tempMuitoAltaRearme{70.0};
};

// Resultado da avaliação de segurança de um ciclo.
struct ResultadoSeguranca {
    // Alarmes ativos neste ciclo para incluir na telemetria.
    std::vector<controlador::dominio::Alarme> alarmes;

    // Se há intertravamento: comando de segurança a ser despachado.
    // nullptr = sem intertravamento, seguir para estratégia normal.
    std::unique_ptr<controlador::comandos::IComando> comandoSeguranca;
};

// Avalia as variáveis do processo contra os limites críticos.
//
// Mantém estado de histerese entre ciclos (alarme ativo / rearme)
// para evitar oscilações rápidas na borda dos limites.
//
// Hierarquia de intertravamentos:
//   - nivel <= 15%  → bomba = 100%, aquecedor = 0%
//   - nivel >= 85%  → bomba = 0%
//   - temp  >= 75°C → aquecedor = 0%  (pode coexistir com intertravamento de nível)
class GerenciadorSeguranca {
public:
    GerenciadorSeguranca();

    // Avalia nivel e temperatura. Retorna alarmes ativos e, se houver
    // intertravamento, o ComandoSeguranca correspondente.
    ResultadoSeguranca avaliar(double nivel,
                               double temperatura,
                               const std::string& sessao,
                               int proximoId,
                               const ConfiguracaoAlarmes& cfg = ConfiguracaoAlarmes{});

    // Reinicia o estado de histerese (nova sessão ou reset manual).
    void resetar();

private:
    // Estados de histerese de cada alarme (true = alarme ativo)
    bool ativoNivelBaixo_{false};
    bool ativoNivelAlto_{false};
    bool ativoTempBaixa_{false};
    bool ativoTempAlta_{false};
};

} // namespace controlador::servicos
