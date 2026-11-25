#include "servicos/controlador_processo.hpp"
#include "estrategias/estrategia_histerese.hpp"
#include "estrategias/estrategia_modulada.hpp"
#include "comandos/comando_atuacao.hpp"
#include "dominio/telemetria.hpp"
#include "servicos/publicador_json.hpp"
#include <filesystem>
#include <iostream>

namespace controlador::servicos {

using namespace controlador::dominio;
using namespace controlador::estrategias;
using namespace controlador::comandos;

ControladorProcesso::ControladorProcesso(const std::string& dirDados)
    : dirDados_(dirDados),
      estrategiaAtiva_(std::make_unique<EstrategiaFaixaHisterese>()) {}

void ControladorProcesso::atualizarEstrategia(const std::string& nome) {
    if (estrategiaAtiva_ && estrategiaAtiva_->nome() == nome) return;
    if (nome == "modulada") {
        estrategiaAtiva_ = std::make_unique<EstrategiaProporcionalModulada>();
    } else {
        estrategiaAtiva_ = std::make_unique<EstrategiaFaixaHisterese>();
    }
}

void ControladorProcesso::tratarMudancaDeSessao(const Aquisicao& aq) {
    if (aq.sessao() != sessaoAtual_) {
        sessaoAtual_            = aq.sessao();
        ultimoCicloProcessado_  = 0;
        proximoIdComando_       = aq.ultimoComando() + 1;
        if (proximoIdComando_ < 1) proximoIdComando_ = 1;
        gerenciadorSeguranca_.resetar();
        if (estrategiaAtiva_) estrategiaAtiva_->resetar();
        std::clog << "[Controlador] Nova sessao detectada: " << sessaoAtual_
                  << " | proximo id: " << proximoIdComando_ << "\n";
    }
}

int ControladorProcesso::executarCiclo() {
    namespace fs = std::filesystem;
    fs::path caminhoPlanta      = fs::path(dirDados_) / "planta.json";
    fs::path caminhoTelemetria  = fs::path(dirDados_) / "telemetria.json";
    fs::path caminhoComando     = fs::path(dirDados_) / "comando_planta.json";
    fs::path caminhoSolicitacao = fs::path(dirDados_) / "solicitacao_operador.json";

    // --- 1. Leitura e validação de planta.json ---
    if (!fs::exists(caminhoPlanta)) {
        std::cerr << "[Controlador] Erro: planta.json nao encontrado\n";
        return 1;
    }
    Aquisicao aq;
    std::string erroAq;
    if (!Aquisicao::carregarDeArquivo(caminhoPlanta.string(), aq, erroAq)) {
        std::cerr << "[Controlador] Erro na aquisicao: " << erroAq << "\n";
        return 2;
    }

    // --- 2. Detecção de mudança de sessão ---
    tratarMudancaDeSessao(aq);

    // --- 3. Idempotência: ciclo repetido ---
    if (aq.ciclo() == ultimoCicloProcessado_) {
        std::clog << "[Controlador] Ciclo " << aq.ciclo() << " ja processado, ignorando\n";
        return 0;
    }
    ultimoCicloProcessado_ = aq.ciclo();

    // --- 4. Atualização das configurações do operador ---
    {
        ConfiguracaoOperador novaCfg;
        std::string erroSol;
        bool solicitacaoOk = ValidadorSolicitacao::carregar(
            caminhoSolicitacao.string(),
            idUltimaSolicitacaoValida_,
            novaCfg,
            erroSol);
        if (solicitacaoOk) {
            configAtiva_ = novaCfg;
            idUltimaSolicitacaoValida_ = novaCfg.id;
            atualizarEstrategia(configAtiva_.estrategia);
            std::clog << "[Controlador] Solicitacao do operador aplicada (id="
                      << novaCfg.id << ")\n";
        } else if (!erroSol.empty() &&
                   erroSol.find("ausente") == std::string::npos &&
                   erroSol.find("nao e mais recente") == std::string::npos) {
            // Arquivo presente mas inválido: apenas loga, mantém config anterior
            std::clog << "[Controlador] Solicitacao invalida (mantendo config anterior): "
                      << erroSol << "\n";
        }
    }

    // --- 5. Avaliação de segurança e intertravamentos ---
    auto resultadoSeg = gerenciadorSeguranca_.avaliar(
        aq.nivel(),
        aq.temperatura(),
        aq.sessao(),
        proximoIdComando_,
        configAtiva_.alarmes);

    std::string jsonComando;
    bool emitirComando = false;

    if (resultadoSeg.comandoSeguranca) {
        // Intertravamento ativo: usar ComandoSeguranca
        jsonComando  = resultadoSeg.comandoSeguranca->gerarJson();
        emitirComando = true;
        proximoIdComando_ = resultadoSeg.comandoSeguranca->id() + 1;
        std::clog << "[Controlador] INTERTRAVAMENTO ativo (id="
                  << resultadoSeg.comandoSeguranca->id()
                  << "): bomba=" << resultadoSeg.comandoSeguranca->valorBomba()
                  << "% aquecedor=" << resultadoSeg.comandoSeguranca->valorAquecedor()
                  << "%\n";
    } else {
        // --- 6. Estratégia de controle normal ---
        DecisaoAtuacao decisao = estrategiaAtiva_->calcular(
            aq.nivel(), aq.temperatura(), configAtiva_.faixas);

        ComandoAtuacao cmd(proximoIdComando_, aq.sessao(),
                           decisao.bomba, decisao.aquecedor);
        jsonComando  = cmd.gerarJson();
        emitirComando = true;
        proximoIdComando_++;

        std::clog << "[Controlador] Estrategia '" << estrategiaAtiva_->nome()
                  << "' (id=" << cmd.id()
                  << "): bomba=" << decisao.bomba
                  << "% aquecedor=" << decisao.aquecedor << "%\n";
    }

    // --- 7. Publicação atômica de comando_planta.json ---
    if (emitirComando) {
        std::string erroPub;
        if (!PublicadorJson::publicarAtomico(caminhoComando.string(), jsonComando, erroPub)) {
            std::cerr << "[Controlador] Erro ao publicar comando: " << erroPub << "\n";
            return 3;
        }
    }

    // --- 8. Publicação atômica de telemetria.json ---
    Telemetria telemetria(aq, std::move(resultadoSeg.alarmes));
    std::string jsonTel = telemetria.serializar(2);
    std::string erroTel;
    if (!PublicadorJson::publicarAtomico(caminhoTelemetria.string(), jsonTel, erroTel)) {
        std::cerr << "[Controlador] Erro ao publicar telemetria: " << erroTel << "\n";
        return 4;
    }

    return 0;
}

} // namespace controlador::servicos
