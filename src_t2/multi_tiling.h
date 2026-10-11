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
 * @brief Estrutura de instrumentação e relatório de validação (BIST).
 *
 * Mapeada em memória SRAM para inspeção direta em tempo real
 * através da janela 'Live Expressions' do STM32CubeIDE via ST-Link.
 */
typedef struct {
    volatile TestStatus_t status;              /**< Estado do teste (TEST_IDLE, RUNNING, PASSED, FAILED) */
    volatile uint32_t     elements_processed;  /**< Contador incremental: vai de 0 até N*N (2025) */
    volatile float        checksum_calculated; /**< Soma acumulada de todos os elementos C calculados */
    volatile float        checksum_expected;   /**< Valor de referência oficial derivado do Python */
    volatile float        absolute_error;      /**< Diferença absoluta (|calculado - esperado|) */
} ValidationReport_t;

/**
 * @brief Instância global pública do relatório de validação.
 */
extern volatile ValidationReport_t g_report;

/**
 * @brief Executa a multiplicação de matrizes com técnica de Tiling e Tile Streaming.
 *
 * Lê as matrizes A e B da memória Flash (.rodata), calcula a multiplicação bloco
 * a bloco utilizando apenas um buffer local de 256 bytes em SRAM, acumula
 * incrementalmente o checksum para o BIST e atualiza a struct g_report.
 */
void multi_tiling_streaming_bist(void);

#ifdef __cplusplus
}
#endif

#endif /* MULTI_TILING_H */
