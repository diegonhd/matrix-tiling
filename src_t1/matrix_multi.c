/*
 * ==============================================================================
 * PROJETO: Multiplicação de Matrizes com Otimização de Cache (Tiling / Blocking)
 * ARQUIVO: matrix_multi.c (Tarefa 1 - src_t1)
 * OBJETIVO: Implementar a multiplicação C = A * B utilizando o algoritmo de Tiling
 *           em precisão dupla (double) e exportar os dados em formato binário (.bin).
 * ==============================================================================
 * 
 * CONCEITO CHAVE PARA EXPLICAR AO PROFESSOR:
 * 1. O que é Tiling (Blocking)?
 *    A multiplicação ingênua itera toda a linha de A e coluna de B, gerando muitos
 *    "cache misses" porque matrizes grandes não cabem inteiras na memória Cache L1/L2.
 *    O Tiling particiona a matriz em sub-blocos de tamanho BxB (aqui, B=8). Cada bloco
 *    é carregado na cache e reutilizado exaustivamente antes de buscar novos dados da RAM.
 * 
 * 2. Por que precisão dupla (double)?
 *    Garante 53 bits de mantissa (~15 a 17 dígitos decimais de precisão), reduzindo o
 *    erro de arredondamento acumulado de ~10^-4 (em float) para ~10^-13 (em double).
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dimensão da matriz quadrada (45x45 = 2025 elementos, atende ao requisito de ~2000)
#define N 45

// Tamanho do bloco de Tiling (parâmetro de sintonia da Cache L1)
#define BLOCK 8

/**
 * Preenche a matriz com valores pseudo-aleatórios uniformes entre -10.0 e +9.9.
 * Simula leituras bipolares de sensores embarcados (ex: acelerômetros e IMUs).
 * Recebe uma seed fixa para permitir reprodução determinística dos testes.
 */
void gerar_matriz_aleatoria(double M[N][N], unsigned int seed) {
    srand(seed);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            M[i][j] = ((double)(rand() % 200) - 100.0) / 10.0;
        }
    }
}

/**
 * Função auxiliar que retorna o menor entre dois inteiros.
 * Essencial para tratar as bordas da matriz caso N não seja múltiplo exato de BLOCK.
 */
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * Multiplicação com particionamento em blocos (Tiling / Blocking).
 * Estrutura com 6 laços aninhados:
 *  - 3 laços externos (ii, jj, kk): navegam bloco a bloco pela matriz.
 *  - 3 laços internos (i, j, k): realizam a multiplicação dos elementos dentro do bloco atual.
 */
void mult_tiling(double A[N][N], double B[N][N], double C[N][N]) {
    // Inicializa a matriz resultante com zeros
    memset(C, 0, sizeof(double) * N * N);

    // Laços externos: delimitam as coordenadas dos blocos
    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);

            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                // Laços internos: multiplicam e acumulam dentro da memória rápida (Cache)
                for (int i = ii; i < i_max; i++) {
                    for (int j = jj; j < j_max; j++) {
                        double soma = C[i][j];
                        for (int k = kk; k < k_max; k++) {
                            soma += A[i][k] * B[k][j];
                        }
                        C[i][j] = soma;
                    }
                }
            }
        }
    }
}

/**
 * Salva a matriz em formato binário bruto (.bin) em precisão dupla (8 bytes por valor).
 * Suporta execução tanto a partir de dentro de 'src_t1/' quanto da raiz do repositório.
 */
void salvar_matriz(const char *nome, double M[N][N]) {
    char caminho[128];

    // 1. Tenta salvar na pasta local "bin_c/" (se executado de dentro de src_t1)
    snprintf(caminho, sizeof(caminho), "bin_c/%s", nome);
    FILE *arquivo = fopen(caminho, "wb");

    // 2. Se falhar, tenta "src_t1/bin_c/" (se executado da raiz do projeto)
    if (!arquivo) {
        snprintf(caminho, sizeof(caminho), "src_t1/bin_c/%s", nome);
        arquivo = fopen(caminho, "wb");
    }

    if (!arquivo) {
        printf("[ERRO] Nao foi possivel criar o arquivo binario: %s\n", caminho);
        exit(1);
    }

    // Grava N*N elementos de 8 bytes cada (45*45*8 = 16.200 bytes por matriz)
    fwrite(M, sizeof(double), N * N, arquivo);
    fclose(arquivo);
    printf("  -> Arquivo salvo: %s (16.200 bytes)\n", caminho);
}

/**
 * Função principal:
 * Permite passar seeds opcionais via linha de comando para suportar testes automatizados:
 *   ./matrix_multi [seedA] [seedB]
 * Se nenhuma seed for passada, adota por padrão seedA=42 e seedB=123.
 */
int main(int argc, char *argv[]) {
    unsigned int seedA = 42;
    unsigned int seedB = 123;

    if (argc >= 3) {
        seedA = (unsigned int)atoi(argv[1]);
        seedB = (unsigned int)atoi(argv[2]);
    }

    // 'static' aloca na área de dados globais/estáticos, evitando estouro de pilha (stack overflow)
    static double A[N][N];
    static double B[N][N];
    static double C_tiling[N][N];

    printf("=====================================================\n");
    printf(" MULTIPLICACAO COM TILING EM C (N=%d, BLOCK=%d, DOUBLE)\n", N, BLOCK);
    printf(" Seeds utilizadas: A=%u, B=%u\n", seedA, seedB);
    printf("=====================================================\n");

    // 1. Geração determinística das matrizes de entrada
    gerar_matriz_aleatoria(A, seedA);
    gerar_matriz_aleatoria(B, seedB);

    // 2. Computação com Tiling
    mult_tiling(A, B, C_tiling);

    // 3. Serialização binária em disco
    printf("Gravando matrizes em bin_c/...\n");
    salvar_matriz("A.bin", A);
    salvar_matriz("B.bin", B);
    salvar_matriz("C.bin", C_tiling);

    printf("Computacao em C finalizada com sucesso!\n\n");
    return 0;
}