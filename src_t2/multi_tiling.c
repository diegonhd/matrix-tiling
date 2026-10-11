#include "multi_tiling.h"
#include <string.h>
#include <math.h>

/**
 * @brief Relatório global de validação em SRAM.
 *
 * Utiliza inicialização com os valores nominais esperados de 'test_data.h'.
 */
volatile ValidationReport_t g_report = {
    .status = TEST_IDLE,
    .elements_processed = 0,
    .checksum_calculated = 0.0f,
    .checksum_expected = CHECKSUM_SUM_EXPECTED,
    .absolute_error = 0.0f
};

/**
 * @brief Retorna o menor entre dois números inteiros.
 */
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * @brief Executa a multiplicação A x B com Tile Streaming e validação incremental.
 *
 * DETALHES DE ARQUITETURA EMBARCADA:
 * - A e B são lidos diretamente da memória FLASH (const / .rodata), sem consumir RAM.
 * - 'bloco_C' é alocado como estático de 8x8 floats = 256 bytes na SRAM.
 * - Ao término da multiplicação de cada bloco, seus elementos são integrados ao BIST
 *   (acumulação de soma) e descartados/sobrescritos pelo próximo bloco.
 * - Isso viabiliza o cálculo de N=45 com apenas 256 bytes de memória ativa em vez
 *   dos 8.100 bytes que causariam estouro dos 8 KB de SRAM do microcontrolador.
 */
void multi_tiling_streaming_bist(void) {
    /* Buffer de bloco único ativo na SRAM: 8 x 8 floats = 256 bytes */
    static float bloco_C[BLOCK][BLOCK];

    g_report.status = TEST_RUNNING;
    g_report.elements_processed = 0;
    g_report.checksum_calculated = 0.0f;
    g_report.absolute_error = 0.0f;

    /*
     * 1º e 2º níveis de laço: Varrem os blocos no espaço bidimensional da matriz resultante C.
     */
    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);
        int bi = i_max - ii; /* Altura do bloco atual (até 8) */

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);
            int bj = j_max - jj; /* Largura do bloco atual (até 8) */

            /* Zera o buffer local para receber a acumulação do novo bloco */
            memset(bloco_C, 0, sizeof(bloco_C));

            /*
             * 3º nível de laço: Varridura da dimensão comum K em blocos BLOCK.
             */
            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                /*
                 * 4º, 5º e 6º níveis de laço: Multiplicação e acumulação interna do bloco.
                 */
                for (int i = 0; i < bi; i++) {
                    for (int j = 0; j < bj; j++) {
                        float soma = bloco_C[i][j];

                        for (int k = kk; k < k_max; k++) {
                            /* Acesso direto à Flash: A e B declarados em test_data.h */
                            soma += A[ii + i][k] * B[k][jj + j];
                        }

                        bloco_C[i][j] = soma;
                    }
                }
            }

            /*
             * TILE STREAMING & VALIDAÇÃO (BIST ONLINE):
             * O bloco (ii, jj) está completamente finalizado.
             * Integramos seus elementos ao checksum em vez de manter uma matriz C global em RAM.
             */
            for (int i = 0; i < bi; i++) {
                for (int j = 0; j < bj; j++) {
                    g_report.checksum_calculated += bloco_C[i][j];
                    g_report.elements_processed++;
                }
            }
        }
    }

    /*
     * Avaliação final de integridade numérica:
     * Compara a soma computada com o valor de referência oficial do Python.
     * Considera tolerância numérica de precisão float32 (atol = 1e-2f).
     */
    g_report.absolute_error = fabsf(g_report.checksum_calculated - g_report.checksum_expected);

    if ((g_report.elements_processed == (N * N)) && (g_report.absolute_error < 1e-2f)) {
        g_report.status = TEST_PASSED;
    } else {
        g_report.status = TEST_FAILED;
    }
}
