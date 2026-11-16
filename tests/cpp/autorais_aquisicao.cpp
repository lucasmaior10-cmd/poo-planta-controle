#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "dominio/aquisicao.hpp"
#include "dominio/telemetria.hpp"
#include "servicos/publicador_json.hpp"

using namespace controlador::dominio;
using namespace controlador::servicos;

static void verificar(bool condicao, const char* mensagem) {
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

int main() {
    try {
        // 1. Validacao com JSON valido canonico
        const std::string jsonValido = R"({
            "versao": 1,
            "sessao": "sessao-teste-01",
            "ciclo": 42,
            "timestamp": "2026-11-16T12:00:00Z",
            "leituras": [
                {"tag": "LT-01", "tipo": "nivel", "valor": 55.5, "unidade": "%", "status": "operando"},
                {"tag": "TT-01", "tipo": "temperatura", "valor": 35.0, "unidade": "C", "status": "operando"}
            ],
            "atuadores": {"bomba": 25.0, "aquecedor": 10.0},
            "ultimo_comando": 3,
            "erro_comando": null,
            "motivo_seguro": null
        })";

        Aquisicao aquisicao;
        std::string erro;
        bool ok = Aquisicao::parseString(jsonValido, aquisicao, erro);
        verificar(ok, "Aquisicao valida deve ser aceita");
        verificar(aquisicao.versao() == 1, "Versao deve ser 1");
        verificar(aquisicao.sessao() == "sessao-teste-01", "Sessao incorreta");
        verificar(aquisicao.ciclo() == 42, "Ciclo incorreto");
        verificar(aquisicao.timestamp() == "2026-11-16T12:00:00Z", "Timestamp incorreto");
        verificar(aquisicao.nivel() == 55.5, "Nivel incorreto");
        verificar(aquisicao.temperatura() == 35.0, "Temperatura incorreta");
        verificar(aquisicao.atuadores().bomba() == 25.0, "Bomba incorreta");
        verificar(aquisicao.atuadores().aquecedor() == 10.0, "Aquecedor incorreto");
        verificar(aquisicao.ultimoComando() == 3, "Ultimo comando incorreto");

        // 2. Rejeicao de entradas invalidas
        const std::vector<std::string> entradasInvalidas = {
            "",                             // Vazio
            "{",                            // Sintaxe quebrada
            "null",                         // Null
            "[]",                           // Array em vez de objeto
            "{}",                           // Objeto vazio
            R"({"versao": 2})",             // Versao diferente de 1
            R"({"versao": true})",          // Versao booleana
            R"({"versao": 1, "sessao": "", "ciclo": 1, "timestamp": "T", "leituras": [], "atuadores": {}, "ultimo_comando": 0})", // Sessao vazia
            R"({"versao": 1, "sessao": "s", "ciclo": 0, "timestamp": "T", "leituras": [], "atuadores": {}, "ultimo_comando": 0})", // Ciclo zero
            // Unidade de nivel errada
            R"({
                "versao": 1, "sessao": "s", "ciclo": 1, "timestamp": "T",
                "leituras": [
                    {"tag": "LT-01", "tipo": "nivel", "valor": 50, "unidade": "m", "status": "operando"},
                    {"tag": "TT-01", "tipo": "temperatura", "valor": 20, "unidade": "C", "status": "operando"}
                ],
                "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0
            })",
            // Nivel acima de 100%
            R"({
                "versao": 1, "sessao": "s", "ciclo": 1, "timestamp": "T",
                "leituras": [
                    {"tag": "LT-01", "tipo": "nivel", "valor": 105.0, "unidade": "%", "status": "operando"},
                    {"tag": "TT-01", "tipo": "temperatura", "valor": 20, "unidade": "C", "status": "operando"}
                ],
                "atuadores": {"bomba": 0, "aquecedor": 0}, "ultimo_comando": 0
            })"
        };

        for (const auto& invalido : entradasInvalidas) {
            Aquisicao aqInvalida;
            std::string erroInvalido;
            bool resultado = Aquisicao::parseString(invalido, aqInvalida, erroInvalido);
            verificar(!resultado, "Entrada invalida deveria ter sido rejeitada");
        }

        // 3. Teste da Telemetria
        Telemetria telemetria(aquisicao, {});
        nlohmann::json jsonTelemetria = telemetria.paraJson();
        verificar(jsonTelemetria["versao"] == 1, "Telemetria: versao incorreta");
        verificar(jsonTelemetria["sessao"] == "sessao-teste-01", "Telemetria: sessao incorreta");
        verificar(jsonTelemetria["ciclo"] == 42, "Telemetria: ciclo incorreto");
        verificar(jsonTelemetria["alarmes"].is_array(), "Telemetria: alarmes deve ser array");
        verificar(jsonTelemetria["alarmes"].empty(), "Telemetria: alarmes deve estar vazio na etapa 01");
        verificar(jsonTelemetria["atuadores"]["bomba"] == 25.0, "Telemetria: bomba preservada");

        // 4. Teste de Escrita Atomica
        std::filesystem::path dirTmp = std::filesystem::temp_directory_path() / "teste_publicador_poo";
        std::filesystem::create_directories(dirTmp);
        std::filesystem::path arquivoDestino = dirTmp / "telemetria_teste.json";

        std::string conteudo = telemetria.serializar();
        std::string erroPub;
        bool pubOk = PublicadorJson::publicarAtomico(arquivoDestino.string(), conteudo, erroPub);
        verificar(pubOk, "PublicadorJson: escrita atomica falhou");
        verificar(std::filesystem::exists(arquivoDestino), "Arquivo de destino nao existe apos publicacao");
        verificar(!std::filesystem::exists(dirTmp / "telemetria_teste.json.tmp"), "Arquivo temporario deve ser removido apos renomeacao");

        std::filesystem::remove_all(dirTmp);

        std::cout << "OK: Aquisicao, Telemetria e PublicadorJson autorais verificados com sucesso\n";
        return 0;
    } catch (const std::exception& erro) {
        std::cerr << "FALHOU: " << erro.what() << "\n";
        return 1;
    }
}

