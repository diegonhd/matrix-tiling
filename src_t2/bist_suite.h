#ifndef BIST_SUITE_H
#define BIST_SUITE_H

#include <stdint.h>
#include "test_data.h"

/* Fallback de seguranca caso o test_data.h no CubeIDE ainda seja a versao antiga */
#ifndef CHECKSUM_ZEROS_EXPECTED
#define CHECKSUM_ZEROS_EXPECTED       0.0f
#endif

#ifndef CHECKSUM_IDENTIDADE_EXPECTED
#define CHECKSUM_IDENTIDADE_EXPECTED  -82.699997f
#endif

/* Tolerância absoluta analítica fundamentada no limite de Higham para a soma de 2.025 elementos */
#ifndef BIST_ATOL_TOLERANCE
#define BIST_ATOL_TOLERANCE           0.005f  /* 5.0e-3 (cobre o pior erro observado de 0.0029 com margem) */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Estados possíveis da execução do teste embarcado.
 */
typedef enum {
    TEST_IDLE    = 0,  /**< Inicializado, aguardando início */
    TEST_RUNNING = 1,  /**< Executando computação e streaming de blocos */
    TEST_PASSED  = 2,  /**< Concluído com sucesso (todos os cenários aprovados) */
    TEST_FAILED  = 3   /**< Concluído com falha (divergência numérica detectada) */
} TestStatus_t;

/**
 * @brief Tipos de cenários de teste disponíveis na suíte.
 */
typedef enum {
    SCENARIO_ZEROS      = 0, /**< Teste do Elemento Nulo: A * 0 = 0 */
    SCENARIO_IDENTITY   = 1, /**< Teste do Elemento Neutro: A * I = A */
    SCENARIO_STANDARD_1 = 2, /**< Teste Estatístico 1: A_1 * B_1 */
    SCENARIO_COUNT      = 3  /**< Quantidade total de cenários */
} TestScenario_t;

/**
 * @brief Estrutura de instrumentação e relatório de validação (BIST).
 *
 * Mapeada em memória SRAM para inspeção direta em tempo real
 * através da janela 'Live Expressions' do STM32CubeIDE via ST-Link.
 */
typedef struct {
    volatile TestStatus_t   status;              /**< Estado geral (TEST_IDLE, RUNNING, PASSED, FAILED) */
    volatile TestScenario_t current_scenario;    /**< Cenário atualmente em execução (0, 1 ou 2) */
    volatile uint32_t       tests_passed_count;  /**< Número de cenários aprovados com sucesso (0 a 3) */
    volatile uint32_t       elements_processed;  /**< Contador incremental do teste atual (0 a 2025) */
    volatile float          checksum_calculated; /**< Soma acumulada de todos os elementos C do teste atual */
    volatile float          checksum_expected;   /**< Valor esperado oficial do teste atual */
    volatile float          absolute_error;      /**< Diferença absoluta (|calculado - esperado|) */
    volatile float          tolerance;           /**< Tolerância analítica máxima de aceitação (atol = 0.005f) */
} ValidationReport_t;

/**
 * @brief Instância global pública do relatório de validação.
 */
extern volatile ValidationReport_t g_report;

/**
 * @brief Executa um cenário específico de teste.
 *
 * @param scenario Cenário a executar (SCENARIO_ZEROS, SCENARIO_IDENTITY, SCENARIO_STANDARD_1).
 * @return TestStatus_t TEST_PASSED ou TEST_FAILED.
 */
TestStatus_t bist_run_scenario(TestScenario_t scenario);

/**
 * @brief Executa a suíte completa de testes sequenciais (Zeros -> Identidade -> Teste 1).
 */
void bist_run_suite(void);

#ifdef __cplusplus
}
#endif

#endif /* BIST_SUITE_H */
