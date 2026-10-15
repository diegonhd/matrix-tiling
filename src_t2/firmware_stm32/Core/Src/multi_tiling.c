/**
 ******************************************************************************
 * @file           : multi_tiling.c
 * @brief          : Implementação do motor de multiplicação particionada de matrizes
 *                   por blocos (Loop Tiling) em regime de streaming contínuo.
 *                   Otimizado para o microcontrolador ARM Cortex-M0 com apenas 8 KB SRAM.
 ******************************************************************************
 */

#include "multi_tiling.h"
#include <string.h>

/* =========================================================================
 * BUFFERS ESTÁTICOS DE MEMÓRIA SRAM
 * =========================================================================
 * Alocação estática na seção .bss da SRAM (zero impacto na pilha/stack).
 * Como a matriz A completa exigiria 8.100 bytes (estourando os 8 KB da placa),
 * mantemos apenas UMA FAIXA HORIZONTAL DE ATÉ 8 LINHAS (BLOCK):
 * 8 linhas x 45 colunas x 4 bytes/float = 1.440 bytes de SRAM.
 */
static float faixa_A[BLOCK][N]; // declara o bloco estatico
static int faixa_A_linhas = 0; 

/**
 * @brief Armazena na SRAM a faixa de até 8 linhas da matriz A enviada pelo Host.
 * 
 * Quando o Host envia o comando CMD_SEND_STRIP_A, ele transmite uma fatia
 * horizontal completa de A. Esta função grava essa fatia no buffer estático.
 * 
 * @param n_linhas Quantidade de linhas ativas (8 para faixas regulares, 5 na última borda de N=45).
 * @param dados Ponteiro para os dados em ponto-flutuante IEEE 754 recebidos da serial.
 */

void multi_tiling_set_faixa_A(int n_linhas, const float *dados) {
    /* Proteção contra estouro de limites do buffer estático */
    if (n_linhas > BLOCK) { // Protecao contra estouro da borda
        n_linhas = BLOCK; 
    }
    faixa_A_linhas = n_linhas;

    /* 
     * Copia direta de memória rápida (memcpy em blocos de 32 bits pelo Cortex-M0):
     * n_linhas * 45 elementos * 4 bytes/float = no máximo 1.440 bytes transferidos.
     */
    memcpy(faixa_A, dados, (size_t)n_linhas * N * sizeof(float));
}

/**
 * @brief Executa o produto matricial particionado entre a faixa de A e o bloco de B.
 * 
 * Matemática da Computação:
 * C[i][j] = sum_{k=0}^{N-1} (A[i][k] * B[k][j])
 * 
 * Estrutura de Tiling Espacial (Loop Tiling na dimensão K):
 * Em vez de percorrer a dimensão K de forma linear e contínua (o que causaria perda de localidade),
 * dividimos o percurso de K em sub-blocos de tamanho BLOCK = 8:
 * - Bloco kk = 0  -> k de 0 a 7
 * - Bloco kk = 8  -> k de 8 a 15
 * - Bloco kk = 16 -> k de 16 a 23
 * - Bloco kk = 24 -> k de 24 a 31
 * - Bloco kk = 32 -> k de 32 a 39
 * - Bloco kk = 40 -> k de 40 a 44 (resto de 5 elementos)
 * 
 * Formato dos dados de entrada `dados_B`:
 * A submatriz B é recebida com dimensão (45 x largura_b) em formato row-major contínuo:
 * O elemento B[k][j] está posicionado no índice `k * largura_b + j`.
 * 
 * @param n_linhas Altura do bloco C sendo computado (máximo 8).
 * @param largura_b Largura do bloco C sendo computado (máximo 8).
 * @param dados_B Vetor contínuo contendo a coluna particionada de B.
 * @param bloco_C_out Vetor de saída onde o resultado de 64 floats será gravado.
 */

void multi_tiling_computar_bloco(int n_linhas, int largura_b, const float *dados_B, float *bloco_C_out) {
    /* Saneamento defensivo de parâmetros de entrada */
    if (n_linhas > BLOCK) {
        n_linhas = BLOCK; 
    }
    if (largura_b > BLOCK) {
        largura_b = BLOCK;
    }

    /* Laço 1: Itera pelas linhas do bloco resultante (i = 0 até no máximo 7) */
    for (int i = 0; i < n_linhas; i++) {

        /* Laço 2: Itera pelas colunas do bloco resultante (j = 0 até no máximo 7) */
        for (int j = 0; j < largura_b; j++) {
            
            /* Acumulador local em registrador de 32 bits da CPU (s0/r0) */
            float soma = 0.0f;

            /* 
             * Laço 3: Particionamento Tiling na dimensão interna K (passo = BLOCK = 8).
             * Divide a soma de 45 termos em pedaços de 8 elementos para manter
             * os operandos no conjunto de trabalho dos registradores da ALU do Cortex-M0.
             */
            for (int kk = 0; kk < N; kk += BLOCK) {
                
                /* Calcula o limite superior do sub-bloco tratando o resto na borda (45 % 8 = 5) */
                int k_max = (kk + BLOCK < N) ? (kk + BLOCK) : N;

                /* Laço 4: Acumulação do produto interno dentro do sub-bloco de K */
                for (int k = kk; k < k_max; k++) {
                    /*
                     * faixa_A[i][k]: linha i da faixa ativa na SRAM (acesso contínuo).
                     * dados_B[k * largura_b + j]: linha k, coluna j da submatriz B recebida.
                     */
                    soma += faixa_A[i][k] * dados_B[k * largura_b + j];
                }
            }

            /* 
             * Armazena o valor acumulado final no buffer de saída do bloco:
             * Formato 2D linearizado: elemento (i, j) = índice [i * largura_b + j].
             * Ocupa apenas 8 x 8 x 4 = 256 bytes na memória SRAM!
             */
            bloco_C_out[i * largura_b + j] = soma;
        }
    }
}
