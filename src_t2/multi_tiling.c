#include "multi_tiling.h"
#include <string.h>
#include <math.h>

/**
 * @brief Relatório global de validação em SRAM.
 *
 * Mapeado para leitura direta na janela 'Live Expressions' do STM32CubeIDE.
 */
volatile ValidationReport_t g_report = {
    .status = TEST_IDLE,
    .current_scenario = SCENARIO_ZEROS,
    .tests_passed_count = 0,
    .elements_processed = 0,
    .checksum_calculated = 0.0f,
    .checksum_expected = CHECKSUM_ZEROS_EXPECTED,
    .absolute_error = 0.0f
};

/**
 * @brief Retorna o menor entre dois números inteiros.
 */
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * @brief Retorna o valor de B[k][j] conforme o cenário sem alocar matrizes extras em Flash/RAM.
 */
static inline float get_B_element(TestScenario_t scenario, int k, int j) {
    switch (scenario) {
        case SCENARIO_ZEROS:
            return 0.0f;
        case SCENARIO_IDENTITY:
            return (k == j) ? 1.0f : 0.0f;
        case SCENARIO_STANDARD_1:
        default:
            return B[k][j];
    }
}

/**
 * @brief Retorna o checksum escalar de referência do Python para cada cenário.
 */
static inline float get_expected_checksum(TestScenario_t scenario) {
    switch (scenario) {
        case SCENARIO_ZEROS:
            return CHECKSUM_ZEROS_EXPECTED;
        case SCENARIO_IDENTITY:
            return CHECKSUM_IDENTIDADE_EXPECTED;
        case SCENARIO_STANDARD_1:
        default:
            return CHECKSUM_SUM_EXPECTED;
    }
}

/**
 * @brief Executa um cenário de teste pontual utilizando Tile Streaming.
 */
TestStatus_t multi_tiling_run_scenario(TestScenario_t scenario) {
    /* Buffer de bloco único em SRAM: 8x8 floats = 256 bytes */
    static float bloco_C[BLOCK][BLOCK];

    g_report.current_scenario = scenario;
    g_report.status = TEST_RUNNING;
    g_report.elements_processed = 0;
    g_report.checksum_calculated = 0.0f;
    g_report.checksum_expected = get_expected_checksum(scenario);
    g_report.absolute_error = 0.0f;

    /*
     * 1º e 2º níveis de laço: Varrem os blocos no espaço bidimensional da matriz C.
     */
    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);
        int bi = i_max - ii;

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);
            int bj = j_max - jj;

            /* Zera apenas o buffer local de 256 bytes */
            memset(bloco_C, 0, sizeof(bloco_C));

            /*
             * 3º nível de laço: Varridura da dimensão comum K em blocos BLOCK.
             */
            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                /*
                 * 4º, 5º e 6º níveis de laço: Multiplicação do bloco.
                 */
                for (int i = 0; i < bi; i++) {
                    for (int j = 0; j < bj; j++) {
                        float soma = bloco_C[i][j];

                        for (int k = kk; k < k_max; k++) {
                            float b_val = get_B_element(scenario, k, jj + j);
                            soma += A[ii + i][k] * b_val;
                        }

                        bloco_C[i][j] = soma;
                    }
                }
            }

            /*
             * Acumulação online do BIST bloco a bloco.
             */
            for (int i = 0; i < bi; i++) {
                for (int j = 0; j < bj; j++) {
                    g_report.checksum_calculated += bloco_C[i][j];
                    g_report.elements_processed++;
                }
            }
        }
    }

    /* Validação numérica com tolerância IEEE 754 */
    g_report.absolute_error = fabsf(g_report.checksum_calculated - g_report.checksum_expected);

    if ((g_report.elements_processed == (N * N)) && (g_report.absolute_error < 1e-2f)) {
        return TEST_PASSED;
    } else {
        return TEST_FAILED;
    }
}

/**
 * @brief Executa a suíte de múltiplos testes (Zeros -> Identidade -> Teste 1).
 */
void multi_tiling_run_suite(void) {
    g_report.tests_passed_count = 0;

    for (int sc = 0; sc < SCENARIO_COUNT; sc++) {
        TestStatus_t res = multi_tiling_run_scenario((TestScenario_t)sc);
        if (res == TEST_PASSED) {
            g_report.tests_passed_count++;
        } else {
            g_report.status = TEST_FAILED;
            return; /* Interrompe se algum cenário falhar */
        }
    }

    /* Todos os cenários da suíte passaram */
    g_report.status = TEST_PASSED;
}

/**
 * @brief Executa o teste padrão único (compatibilidade com Módulo 1).
 */
void multi_tiling_streaming_bist(void) {
    TestStatus_t res = multi_tiling_run_scenario(SCENARIO_STANDARD_1);
    g_report.status = res;
    if (res == TEST_PASSED) {
        g_report.tests_passed_count = 1;
    }
}
