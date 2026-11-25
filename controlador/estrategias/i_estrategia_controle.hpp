#pragma once
#include <string>
#include "estrategias/configuracao_controle.hpp"

namespace controlador::estrategias {

// Interface (padrão Strategy) que define o contrato de qualquer
// algoritmo de controle do reservatório térmico.
//
// Novas estratégias (C, D, PID…) são adicionadas implementando
// esta interface sem modificar o ControladorProcesso.
class IEstrategiaControle {
public:
    virtual ~IEstrategiaControle() = default;

    // Retorna o nome identificador da estratégia (ex.: "histerese").
    virtual std::string nome() const = 0;

    // Calcula os percentuais de bomba e aquecedor para o ciclo atual.
    // O método pode manter estado interno entre chamadas (ex.: última
    // atuação aplicada para implementar a banda morta / histerese).
    virtual DecisaoAtuacao calcular(double nivel,
                                    double temperatura,
                                    const ConfiguracaoControle& config) = 0;

    // Reinicia qualquer estado interno (troca de sessão ou de estratégia).
    virtual void resetar() = 0;
};

} // namespace controlador::estrategias
