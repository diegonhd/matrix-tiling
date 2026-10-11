#include "multi_tiling.h"
#include <string.h>

/**
 * @brief Retorna o menor entre dois inteiros.
 */
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

void multi_tiling_compute_stream(const float A[N][N],
                                 b_getter_fn_t b_getter,
                                 const void *b_ctx,
                                 tile_consumer_fn_t consumer,
                                 void *consumer_ctx) {
    /* Buffer de bloco único em SRAM: 8 x 8 floats = 256 bytes */
    static float bloco_C[BLOCK][BLOCK];

    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);
        int bi = i_max - ii;

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);
            int bj = j_max - jj;

            /* Zera apenas o buffer local de 256 bytes */
            memset(bloco_C, 0, sizeof(bloco_C));

            /* Varridura da dimensão comum K em blocos */
            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                /* Multiplicação e acumulação interna do bloco */
                for (int i = 0; i < bi; i++) {
                    for (int j = 0; j < bj; j++) {
                        float soma = bloco_C[i][j];

                        for (int k = kk; k < k_max; k++) {
                            float b_val = b_getter(k, jj + j, b_ctx);
                            soma += A[ii + i][k] * b_val;
                        }

                        bloco_C[i][j] = soma;
                    }
                }
            }

            /* Despacha o bloco finalizado para o consumidor/validador */
            if (consumer != NULL) {
                consumer(ii, jj, bi, bj, bloco_C, consumer_ctx);
            }
        }
    }
}
