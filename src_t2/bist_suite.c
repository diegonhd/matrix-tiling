#include "bist_suite.h"
#include "multi_tiling.h"
#include <math.h>
#include <string.h>

/**
 * @brief Relatório global de validação em SRAM.
 */
volatile ValidationReport_t g_report = {
    .status = TEST_IDLE,
    .current_scenario = SCENARIO_ZEROS,
    .tests_passed_count = 0,
    .elements_processed = 0,
    .checksum_calculated = 0.0f,
    .checksum_expected = CHECKSUM_ZEROS_EXPECTED,
    .absolute_error = 0.0f,
    .tolerance = BIST_ATOL_TOLERANCE
};

/**
 * @brief Getter de elementos da matriz B para o motor de Tiling.
 */
static float bist_b_getter(int k, int j, const void *ctx) {
    TestScenario_t scenario = (TestScenario_t)(uintptr_t)ctx;
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
 * @brief Callback consumidor que acumula o checksum de cada bloco gerado.
 */
static void bist_tile_consumer(int ii, int jj, int bi, int bj,
                               const float bloco_C[BLOCK][BLOCK],
                               void *user_data) {
    (void)ii;
    (void)jj;
    (void)user_data;

    for (int i = 0; i < bi; i++) {
        for (int j = 0; j < bj; j++) {
            g_report.checksum_calculated += bloco_C[i][j];
            g_report.elements_processed++;
        }
    }
}

static inline float bist_get_expected_checksum(TestScenario_t scenario) {
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

TestStatus_t bist_run_scenario(TestScenario_t scenario) {
    g_report.current_scenario = scenario;
    g_report.status = TEST_RUNNING;
    g_report.elements_processed = 0;
    g_report.checksum_calculated = 0.0f;
    g_report.checksum_expected = bist_get_expected_checksum(scenario);
    g_report.absolute_error = 0.0f;
    g_report.tolerance = BIST_ATOL_TOLERANCE;

    /* Invoca o motor de Tiling puro passando os callbacks do BIST */
    multi_tiling_compute_stream(A,
                                bist_b_getter,
                                (const void *)(uintptr_t)scenario,
                                bist_tile_consumer,
                                NULL);

    /* Validação da precisão float32: Erro absoluto contra a tolerância analítica */
    g_report.absolute_error = fabsf(g_report.checksum_calculated - g_report.checksum_expected);

    /* Critério analítico de aceitação: 2025 elementos e erro contido no limite de Higham */
    if ((g_report.elements_processed == (N * N)) &&
        (g_report.absolute_error <= g_report.tolerance)) {
        return TEST_PASSED;
    } else {
        return TEST_FAILED;
    }
}

void bist_run_suite(void) {
    g_report.tests_passed_count = 0;

    for (int sc = 0; sc < SCENARIO_COUNT; sc++) {
        TestStatus_t res = bist_run_scenario((TestScenario_t)sc);
        if (res == TEST_PASSED) {
            g_report.tests_passed_count++;
        } else {
            g_report.status = TEST_FAILED;
            return;
        }
    }

    g_report.status = TEST_PASSED;
}
