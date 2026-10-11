#ifndef MULTI_TILING_H
#define MULTI_TILING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dimensão fixada da matriz (N=45) */
#ifndef N
#define N 45
#endif

/* Dimensão do bloco de particionamento (BLOCK=8: sub-blocos 8x8 consomem 256B) */
#ifndef BLOCK
#define BLOCK 8
#endif

/**
 * @brief Assinatura da função de acesso a elementos da matriz B (permite matrizes reais ou virtuais).
 */
typedef float (*b_getter_fn_t)(int k, int j, const void *ctx);

/**
 * @brief Assinatura do callback de consumo de cada bloco C calculado.
 *
 * Chamado assim que um sub-bloco de coordenadas (ii, jj) e dimensões bi x bj
 * é completamente finalizado.
 *
 * @param ii Coordenada vertical inicial do bloco em C.
 * @param jj Coordenada horizontal inicial do bloco em C.
 * @param bi Altura efetiva do bloco atual (até BLOCK).
 * @param bj Largura efetiva do bloco atual (até BLOCK).
 * @param bloco_C Ponteiro para a matriz local de bloco float[BLOCK][BLOCK].
 * @param user_data Ponteiro opcional de contexto do usuário/validador.
 */
typedef void (*tile_consumer_fn_t)(int ii, int jj, int bi, int bj,
                                   const float bloco_C[BLOCK][BLOCK],
                                   void *user_data);

/**
 * @brief Executa a multiplicação A x B com particionamento em blocos e Tile Streaming.
 *
 * Algoritmo matemático puro e independente de plataforma. Não realiza alocação dinâmica
 * e consome apenas 256 bytes de buffer de bloco em SRAM.
 *
 * @param A Matriz de entrada A (armazenada em Flash ou RAM).
 * @param b_getter Função para obtenção de cada elemento de B[k][j].
 * @param b_ctx Contexto repassado à função b_getter.
 * @param consumer Callback invocado ao término de cada bloco para consumo/validação.
 * @param consumer_ctx Contexto repassado ao callback consumer.
 */
void multi_tiling_compute_stream(const float A[N][N],
                                 b_getter_fn_t b_getter,
                                 const void *b_ctx,
                                 tile_consumer_fn_t consumer,
                                 void *consumer_ctx);

#ifdef __cplusplus
}
#endif

#endif /* MULTI_TILING_H */
