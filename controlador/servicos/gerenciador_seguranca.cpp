#include "servicos/gerenciador_seguranca.hpp"
#include "comandos/comando_seguranca.hpp"

namespace controlador::servicos {

using controlador::dominio::Alarme;
using controlador::comandos::ComandoSeguranca;

GerenciadorSeguranca::GerenciadorSeguranca()
    : ativoNivelBaixo_(false),
      ativoNivelAlto_(false),
      ativoTempBaixa_(false),
      ativoTempAlta_(false) {}

ResultadoSeguranca GerenciadorSeguranca::avaliar(double nivel,
                                                  double temperatura,
                                                  const std::string& sessao,
                                                  int proximoId,
                                                  const ConfiguracaoAlarmes& cfg) {
    ResultadoSeguranca resultado;
    bool interNivelBaixo = false;
    bool interNivelAlto  = false;
    bool interTempAlta   = false;

    // --- 1. Avaliação de alarmes com histerese de rearme ---

    // Nível muito baixo (ativa em <= limite, rearma em >= rearme)
    if (cfg.nivelMuitoBaixoHabilitado) {
        if (!ativoNivelBaixo_ && nivel <= cfg.nivelMuitoBaixoLimite) {
            ativoNivelBaixo_ = true;
        } else if (ativoNivelBaixo_ && nivel >= cfg.nivelMuitoBaixoRearme) {
            ativoNivelBaixo_ = false;
        }
        if (ativoNivelBaixo_) {
            resultado.alarmes.emplace_back(
                "NIVEL_MUITO_BAIXO",
                "Nivel critico abaixo do limite de seguranca",
                "critico");
            interNivelBaixo = true;
        }
    }

    // Nível muito alto (ativa em >= limite, rearma em <= rearme)
    if (cfg.nivelMuitoAltoHabilitado) {
        if (!ativoNivelAlto_ && nivel >= cfg.nivelMuitoAltoLimite) {
            ativoNivelAlto_ = true;
        } else if (ativoNivelAlto_ && nivel <= cfg.nivelMuitoAltoRearme) {
            ativoNivelAlto_ = false;
        }
        if (ativoNivelAlto_) {
            resultado.alarmes.emplace_back(
                "NIVEL_MUITO_ALTO",
                "Nivel critico acima do limite de seguranca",
                "critico");
            interNivelAlto = true;
        }
    }

    // Temperatura muito baixa (ativa em <= limite, rearma em >= rearme)
    if (cfg.tempMuitoBaixaHabilitada) {
        if (!ativoTempBaixa_ && temperatura <= cfg.tempMuitoBaixaLimite) {
            ativoTempBaixa_ = true;
        } else if (ativoTempBaixa_ && temperatura >= cfg.tempMuitoBaixaRearme) {
            ativoTempBaixa_ = false;
        }
        if (ativoTempBaixa_) {
            resultado.alarmes.emplace_back(
                "TEMP_MUITO_BAIXA",
                "Temperatura critica abaixo do limite de seguranca",
                "critico");
            // Temperatura baixa não causa intertravamento de atuadores nesta versão
        }
    }

    // Temperatura muito alta (ativa em >= limite, rearma em <= rearme)
    if (cfg.tempMuitoAltaHabilitada) {
        if (!ativoTempAlta_ && temperatura >= cfg.tempMuitoAltaLimite) {
            ativoTempAlta_ = true;
        } else if (ativoTempAlta_ && temperatura <= cfg.tempMuitoAltaRearme) {
            ativoTempAlta_ = false;
        }
        if (ativoTempAlta_) {
            resultado.alarmes.emplace_back(
                "TEMP_MUITO_ALTA",
                "Temperatura critica acima do limite de seguranca",
                "critico");
            interTempAlta = true;
        }
    }

    // --- 2. Hierarquia de intertravamentos ---
    // Prioridade: nivel_baixo > nivel_alto > temp_alta
    // (nível baixo é mais crítico: risco de dano à bomba)

    if (interNivelBaixo) {
        // Nível crítico baixo: liga bomba 100%, corta aquecedor (evita queima com pouco líquido)
        resultado.comandoSeguranca = std::make_unique<ComandoSeguranca>(
            proximoId, sessao, 100.0, 0.0);
    } else if (interNivelAlto) {
        // Nível crítico alto: desliga bomba, aquecedor livre (a menos que temp também alta)
        double aquecedorSeguro = interTempAlta ? 0.0 : 100.0;
        resultado.comandoSeguranca = std::make_unique<ComandoSeguranca>(
            proximoId, sessao, 0.0, aquecedorSeguro);
    } else if (interTempAlta) {
        // Apenas temperatura alta: corta aquecedor, bomba livre
        resultado.comandoSeguranca = std::make_unique<ComandoSeguranca>(
            proximoId, sessao, 100.0, 0.0);
    }
    // Se nenhum intertravamento: comandoSeguranca permanece nullptr

    return resultado;
}

void GerenciadorSeguranca::resetar() {
    ativoNivelBaixo_ = false;
    ativoNivelAlto_  = false;
    ativoTempBaixa_  = false;
    ativoTempAlta_   = false;
}

} // namespace controlador::servicos
