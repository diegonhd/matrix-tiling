/**
 * ============================================================================
 * @file        multi_tiling.c
 * @brief       Implementação do motor de multiplicação particionada de matrizes
 *              por blocos (Loop Tiling) em regime de streaming contínuo.
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este módulo realiza a computação do produto escalar matricial por sub-blocos
 * no microcontrolador ARM Cortex-M0 STM32F030R8T6. Ao invés de computar uma matriz
 * inteira 45 x 45 (o que exigiria 24,3 KB de RAM e excederia os 8 KB do chip),
 * o algoritmo mantém na SRAM apenas uma faixa horizontal de até 8 linhas de A (1.440 B)
 * e acumula o sub-bloco resultante C em um buffer de saída de apenas 256 bytes (64 floats).
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Tipo de Dado: Ponto flutuante IEEE 754 de precisão simples (float, 32 bits / 4 bytes).
 * - Entrada: Faixa de A (até 8 x 45 floats) e bloco de B (45 x até 8 floats).
 * - Saída: Sub-bloco C (até 8 x 8 floats = 256 bytes).
 *
 * COMO USAR (HELP):
 * 1. multi_tiling_set_faixa_A(bi, buffer_A) carrega a faixa de A no buffer estático;
 * 2. multi_tiling_computar_bloco(bi, bj, buffer_B, bloco_C) calcula o sub-bloco C.
 *
 * PLATAFORMA ALVO:
 * - Placa STM32 NUCLEO-F030R8 (MCU ARM Cortex-M0 @ 48 MHz).
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Lógica de alocação estática da faixa de A (1.440 B) em .bss,
 *                   particionamento em K (passo BLOCK=8), acúmulo em registrador da ALU
 *                   e geração contínua de blocos C de 256 bytes sob streaming serial.
 * - Código Legado:  Conceito clássico de Loop Tiling / Blocking herdado da Tarefa 1.
 * - Código Gerado:  Não aplicável.
 *
 * PERMISSÕES DE USO E COPYRIGHT:
 * Copyright (c) 2026 Diego Amorim Vidal & André Queiroz Medeiros.
 * Uso educacional e acadêmico livre mediante citação da fonte.
 * ============================================================================
 */
/*--------------------------------------------------------------
--  #1.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Implementação do motor de multiplicação com Loop Tiling em streaming.
--          Otimizado para o microcontrolador ARM Cortex-M0 com apenas 8 KB SRAM.
-------------------------------------------------------------
--  #2.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Buffer estático de faixa de A na seção .bss (1.440 B) com reuso
--          temporal entre múltiplos blocos de B, evitando estouro de memória.
-------------------------------------------------------------
**/

#include "multi_tiling.h"
#include <string.h>

// By Diego & André: #2. Buffer estático de faixa de A
// Como a matriz A completa exigiria 8.100 bytes (estourando os 8 KB da placa),
// mantemos na SRAM apenas UMA FAIXA HORIZONTAL DE ATÉ 8 LINHAS (BLOCK):
// 8 linhas x 45 colunas x 4 bytes/float = 1.440 bytes de SRAM.
static float faixa_A[BLOCK][N];
static int faixa_A_linhas = 0;
// End by Diego & André

/**
 * @brief Armazena na SRAM a faixa de até 8 linhas da matriz A enviada pelo Host.
 *        O conteúdo anterior da variável estática global faixa_A é sobrescrito pela nova fatia.
 * 
 * @param[in] n_linhas Quantidade de linhas ativas (8 para faixas regulares, 5 na borda final).
 * @param[in] dados    Ponteiro para os dados em float32 contíguos recebidos da serial.
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica a variável global estática 'faixa_A' (buffer interno de até 1.440 B sobrescrito).
 *       - Modifica a variável global estática 'faixa_A_linhas' (atualizada com n_linhas após saneamento).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
void multi_tiling_set_faixa_A(int n_linhas, const float *dados) {
    // By Diego & André: #1. Proteção de borda contra overflow de buffer
    if (n_linhas > BLOCK) {
        n_linhas = BLOCK;
    }
    faixa_A_linhas = n_linhas;
    // End by Diego & André

    /* Copia os floats para o buffer estático: n_linhas * 45 * 4 bytes (até 1.440 B) */
    memcpy(faixa_A, dados, (size_t)n_linhas * N * sizeof(float));
}

/**
 * @brief Executa o produto matricial particionado entre a faixa de A e o bloco de B.
 *        O resultado C[i][j] = sum(A[i][k] * B[k][j]) é calculado particionando a
 *        dimensão K em passos de tamanho BLOCK = 8 para reuso nos registradores da ALU.
 *        O buffer bloco_C_out (256 bytes) é sobrescrito com os novos valores.
 * 
 * @param[in]  n_linhas    Altura do bloco C sendo computado (máximo 8).
 * @param[in]  largura_b   Largura do bloco C sendo computado (máximo 8).
 * @param[in]  dados_B     Vetor contínuo contendo a coluna particionada de B.
 * @param[out] bloco_C_out Buffer de saída (256 bytes / 64 floats) sobrescrito com os resultados.
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Lê as variáveis globais estáticas 'faixa_A' e 'faixa_A_linhas'.
 *       - Não altera variáveis globais estáticas internas; altera os dados na memória apontada por 'bloco_C_out'.
 *       Origem: Código próprio adaptado do algoritmo clássico de Loop Tiling da T1 para streaming.
 */
void multi_tiling_computar_bloco(int n_linhas, int largura_b, const float *dados_B, float *bloco_C_out) {
    // By Diego & André: #1. Saneamento defensivo de limites dos sub-blocos
    if (n_linhas > BLOCK) {
        n_linhas = BLOCK;
    }
    if (largura_b > BLOCK) {
        largura_b = BLOCK;
    }
    // End by Diego & André

    /* Laço 1: Itera pelas linhas do bloco resultante (i = 0 até no máximo 7) */
    for (int i = 0; i < n_linhas; i++) {

        /* Laço 2: Itera pelas colunas do bloco resultante (j = 0 até no máximo 7) */
        for (int j = 0; j < largura_b; j++) {

            // By Diego & André: #1. Acumulador local em registrador da CPU (reduz acessos à SRAM)
            float soma = 0.0f;

            /*
             * Laço 3: Particionamento Tiling na dimensão interna K (passo = BLOCK = 8).
             * Divide a soma de 45 termos em pedaços de 8 elementos para manter
             * os operandos no conjunto de trabalho dos registradores da ALU do Cortex-M0.
             */
            for (int kk = 0; kk < N; kk += BLOCK) {

                /* Limite superior tratando o resto de borda (45 % 8 = 5) */
                int k_max = (kk + BLOCK < N) ? (kk + BLOCK) : N;

                /* Laço 4: Acumulação do produto interno dentro do sub-bloco de K */
                for (int k = kk; k < k_max; k++) {
                    soma += faixa_A[i][k] * dados_B[k * largura_b + j];
                }
            }

            /*
             * Grava no buffer de saída: elemento (i, j) = índice [i * largura_b + j].
             * Ocupa apenas 8 x 8 x 4 = 256 bytes na memória SRAM!
             */
            bloco_C_out[i * largura_b + j] = soma;
            // End by Diego & André
        }
    }
}
