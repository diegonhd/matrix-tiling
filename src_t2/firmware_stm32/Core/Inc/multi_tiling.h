/**
 ******************************************************************************
 * @file           : multi_tiling.h
 * @brief          : Definições e protótipos do motor de multiplicação particionada
 *                   de matrizes por blocos (Tiling / Loop Tiling) em streaming.
 *                   Projetado especificamente para operar sob o limite severo de
 *                   8 KB de SRAM do microcontrolador STM32F030R8.
 ******************************************************************************
 */

#ifndef MULTI_TILING_H
#define MULTI_TILING_H

#include <stdint.h>

/**
 * @brief Dimensão das matrizes quadradas do teste (45x45 elementos).
 * Cada matriz completa de floats de 32 bits (4 bytes) exigiria:
 * 45 * 45 * 4 = 8.100 bytes (8,1 KB).
 * Três matrizes completas (A, B e C) somam 24,3 KB, estourando a SRAM (8 KB).
 */
#define N      45

/**
 * @brief Tamanho do bloco de partição espacial (Block Size / Tile Size).
 * B = 8 divide a matriz de dimensão 45 em:
 * ceil(45 / 8) = 6 blocos ao longo de cada eixo:
 * - 5 blocos regulares de tamanho 8x8.
 * - 1 bloco de borda de tamanho 5x8 ou 5x5.
 * 
 * Orçamento de SRAM no STM32 com B = 8:
 * - Faixa de A ativa: 8 linhas x 45 colunas x 4 B = 1.440 bytes
 * - Sub-bloco de B:   45 linhas x 8 colunas x 4 B = 1.440 bytes
 * - Bloco C de saída: 8 linhas x 8 colunas x 4 B = 256 bytes
 * TOTAL DE RAM UTILIZADA = ~3.136 bytes (< 40% da SRAM de 8 KB).
 * 
 */
#define BLOCK  8

/**
 * @brief Armazena na memória SRAM estática uma faixa horizontal ativa da matriz A.
 * 
 * Esta função é invocada quando o Host PC despacha o comando CMD_SEND_STRIP_A.
 * O microcontrolador não guarda a matriz A inteira, mas apenas uma fatia de 8 linhas.
 * Todas as multiplicações com os blocos de B subsequentes reutilizarão essa mesma faixa,
 * maximizando a localidade temporal de cache/memória sem custo adicional de RAM.
 * 
 * @param n_linhas Quantidade de linhas válidas na fatia (8 para blocos centrais, 5 na borda).
 * @param dados Ponteiro para o vetor contínuo de floats (n_linhas * 45 floats = até 1.440 bytes).
 */
void multi_tiling_set_faixa_A(int n_linhas, const float *dados);

/**
 * @brief Multiplica a faixa de A armazenada pelo sub-bloco de colunas de B recebido.
 * 
 * Implementa o algoritmo particionado nos 3 eixos (I, J, K) utilizando Tiling na dimensão K.
 * O resultado é calculado e acumulado diretamente no buffer local de saída `bloco_C_out`.
 * 
 * Vantagem crítica de memória:
 * O buffer `bloco_C_out` ocupa apenas 8 * 8 * 4 = 256 bytes na SRAM.
 * Assim que esta função retorna, o bloco é imediatamente enviado de volta pela UART
 * e esse mesmo espaço de memória é reciclado para a próxima iteração.
 * 
 * @param n_linhas Altura do bloco C a calcular (máximo BLOCK = 8).
 * @param largura_b Largura do bloco C a calcular (máximo BLOCK = 8).
 * @param dados_B Vetor contínuo contendo a coluna particionada de B (45 linhas x largura_b colunas).
 * @param bloco_C_out Ponteiro para o buffer de destino onde os 64 floats calculados serão gravados.
 */
void multi_tiling_computar_bloco(int n_linhas, int largura_b, const float *dados_B, float *bloco_C_out);

#endif /* MULTI_TILING_H */
