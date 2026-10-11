#ifndef MULTI_TILING_H
#define MULTI_TILING_H

#include <stdint.h>
#include "test_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dimensão do bloco para Tiling.
 * BLOCK = 8 gera sub-blocos 8x8 de floats (64 elementos * 4 bytes = 256 bytes).
 * Cabe confortavelmente na SRAM com sobra massiva para o stack.
 */
#ifndef BLOCK
#define BLOCK 8
#endif

/**
 * @brief Estados possíveis da execução do teste embarcado.
 */
typedef enum {
    TEST_IDLE    = 0,  /**< Inicializado, aguardando início */
    TEST_RUNNING = 1,  /**< Executando computação e streaming de blocos */
    TEST_PASSED  = 2,  /**< Concluído com sucesso (checksum verificado dentro da tolerância) */
    TEST_FAILED  = 3   /**< Concluído com falha (divergência numérica ou elementos faltantes) */
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
} ValidationReport_t;

/**
 * @brief Instância global pública do relatório de validação.
 */
extern volatile ValidationReport_t g_report;

/**
 * @brief Executa um cenário específico com Tile Streaming.
 *
 * @param scenario Cenário a ser executado (SCENARIO_ZEROS, SCENARIO_IDENTITY, SCENARIO_STANDARD_1).
 * @return TestStatus_t TEST_PASSED se validado, TEST_FAILED caso contrário.
 */
TestStatus_t multi_tiling_run_scenario(TestScenario_t scenario);

/**
 * @brief Executa a suíte completa de múltiplos testes (Zeros -> Identidade -> Teste 1).
 */
void multi_tiling_run_suite(void);

/**
 * @brief Executa apenas o teste padrão (retrocompatibilidade com o Módulo 1).
 */
void multi_tiling_streaming_bist(void);

#ifdef __cplusplus
}
#endif

#endif /* MULTI_TILING_H */
