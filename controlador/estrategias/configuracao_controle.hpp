#pragma once

namespace controlador::estrategias {

// Configuração das faixas e parâmetros de controle.
// Transmitida para a estratégia ativa a cada ciclo.
struct ConfiguracaoControle {
    // Faixas operacionais de nível (%)
    double nivelMin{40.0};
    double nivelMax{70.0};

    // Faixas operacionais de temperatura (°C)
    double tempMin{30.0};
    double tempMax{50.0};
};

// Resultado calculado por uma estratégia: o que a bomba e o aquecedor devem receber.
struct DecisaoAtuacao {
    double bomba{0.0};
    double aquecedor{0.0};
};

} // namespace controlador::estrategias
