/**
 * ============================================================================
 * @file        multi_tiling.h
 * @brief       Definições e protótipos do motor de multiplicação de matrizes
 *              com particionamento em blocos (Loop Tiling) em regime de streaming.
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este módulo provê a interface do motor de álgebra linear particionado projetado
 * especificamente para operar sob o limite de 8 KB de SRAM do microcontrolador
 * STM32F030R8T6. Ao invés de alocar 24,3 KB para as 3 matrizes completas 45x45,
 * mantém apenas uma faixa de até 8 linhas de A (1.440 B) e acumula sub-blocos
 * resultantes de C em apenas 256 bytes (64 floats).
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Tipo de Dado: Ponto flutuante IEEE 754 de precisão simples (float, 32 bits / 4 bytes).
 * - Entrada: Faixa horizontal de A (até 8 x 45 floats) e bloco vertical de B (45 x até 8 floats).
 * - Saída: Sub-bloco resultante C de dimensão até 8 x 8 floats (até 256 bytes).
 *
 * COMO USAR (HELP):
 * 1. Ao receber a faixa horizontal de A: multi_tiling_set_faixa_A(bi, buffer_A);
 * 2. Ao receber cada bloco vertical de B: multi_tiling_computar_bloco(bi, bj, buffer_B, bloco_C);
 *
 * PLATAFORMA ALVO:
 * - Microcontrolador: STMicroelectronics STM32F030R8T6 (ARM Cortex-M0 @ 48 MHz).
 * - Placa: NUCLEO-F030R8 (8 KB de SRAM, 64 KB de Flash).
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Declaração da interface de streaming por blocos e constantes de dimensionamento de memória.
 * - Código Legado:  Parâmetros dimensionais da matriz (N=45) e do bloco (BLOCK=8) herdados da Tarefa 1.
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
--  Motivo: Especificação do motor de multiplicação particionada por blocos
--          (Loop Tiling em streaming) para caber nos 8 KB de SRAM do STM32F030R8.
-------------------------------------------------------------
--  #2.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Definição de constantes nomeadas para dimensões e tamanhos de buffers
--          (N=45, BLOCK=8, TAM_BLOCO_C_BYTES=256, TAM_BLOCO_C_ELEMS=64).
-------------------------------------------------------------
**/

#ifndef MULTI_TILING_H
#define MULTI_TILING_H

#include <stdint.h>
#include <stddef.h>

// By Diego & André: #2. Constantes nomeadas de dimensão e memória
// Substituição de valores literais para atender ao feedback de T1
#define N                     45  /* Dimensão fixa das matrizes quadradas (45 x 45) */
#define BLOCK                 8   /* Dimensão do sub-bloco de particionamento (8 x 8) */

#define TAM_BLOCO_C_ELEMS     (BLOCK * BLOCK)         /* 64 floats por sub-bloco resultante */
#define TAM_BLOCO_C_BYTES     (TAM_BLOCO_C_ELEMS * 4) /* 256 bytes de SRAM */
#define TAM_FAIXA_A_MAX_BYTES (BLOCK * N * 4)         /* 1.440 bytes de SRAM (8 x 45 floats) */
#define TAM_BLOCO_B_MAX_BYTES (N * BLOCK * 4)         /* 1.440 bytes de SRAM (45 x 8 floats) */
// End by Diego & André

/**
 * @brief Armazena na memória SRAM estática a faixa horizontal ativa de A (até 8 linhas).
 *        Sobrescreve a faixa anterior preexistente no buffer estático interno faixa_A.
 * 
 * @param[in] n_linhas Quantidade de linhas válidas da faixa (8 para blocos regulares, 5 na borda final).
 * @param[in] dados    Ponteiro para os floats contíguos recebidos da serial/DMA (até 1.440 bytes).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica a variável global estática 'faixa_A' (buffer interno de 1.440 B sobrescrito com os novos floats).
 *       - Modifica a variável global estática 'faixa_A_linhas' (atualizada com a quantidade efetiva de linhas).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
void multi_tiling_set_faixa_A(int n_linhas, const float *dados);

/**
 * @brief Multiplica a faixa horizontal ativa de A pelo sub-bloco de colunas de B recebido.
 *        Computa o produto matricial particionado (Loop Tiling em K com passo BLOCK=8),
 *        acumulando os resultados em registrador da CPU e gravando no buffer de saída bloco_C_out.
 * 
 * @param[in]  n_linhas    Altura do bloco C a calcular (máximo BLOCK = 8).
 * @param[in]  largura_b   Largura do bloco C a calcular (máximo BLOCK = 8).
 * @param[in]  dados_B     Vetor contínuo contendo a coluna particionada de B (45 x largura_b floats).
 * @param[out] bloco_C_out Buffer de destino (256 bytes / 64 floats) onde o resultado é gravado (sobrescrito).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Lê as variáveis globais estáticas 'faixa_A' e 'faixa_A_linhas'.
 *       - Não altera variáveis globais de estado interno do módulo; grava diretamente no buffer apontado por 'bloco_C_out'.
 *       Origem: Código próprio adaptado do algoritmo clássico de Loop Tiling da T1 para streaming.
 */
void multi_tiling_computar_bloco(int n_linhas, int largura_b, const float *dados_B, float *bloco_C_out);

#endif /* MULTI_TILING_H */
