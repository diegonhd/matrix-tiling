#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef N
#define N 45
#endif
#define BLOCK 8

/**
 * Preenche a matriz com valores uniformes entre -10.0 e +9.9 (dados bipolares).
 */
void gerar_matriz_aleatoria(double M[N][N], unsigned int seed) {
    srand(seed);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            M[i][j] = ((double)(rand() % 200) - 100.0) / 10.0;
        }
    }
}

static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * Multiplicação com particionamento em blocos (Tiling / Blocking).
 */
void mult_tiling(double A[N][N], double B[N][N], double C[N][N]) {
    memset(C, 0, sizeof(double) * N * N);

    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);

            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

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
 * Exporta a matriz para arquivo binário bruto (.bin).
 */
void salvar_matriz(const char *nome, double M[N][N]) {
    char caminho[128];
    snprintf(caminho, sizeof(caminho), "bin_c/%s", nome);
    FILE *arquivo = fopen(caminho, "wb");

    if (!arquivo) {
        snprintf(caminho, sizeof(caminho), "src_t1/bin_c/%s", nome);
        arquivo = fopen(caminho, "wb");
    }

    if (!arquivo) {
        fprintf(stderr, "[ERRO] Nao foi possivel criar o arquivo: %s\n", caminho);
        exit(1);
    }

    fwrite(M, sizeof(double), N * N, arquivo);
    fclose(arquivo);
}

int main(int argc, char *argv[]) {
    unsigned int seedA = 42;
    unsigned int seedB = 123;

    if (argc >= 3) {
        seedA = (unsigned int)atoi(argv[1]);
        seedB = (unsigned int)atoi(argv[2]);
    }

    static double A[N][N];
    static double B[N][N];
    static double C[N][N];

    // 1. Gera entradas com seeds determinísticas
    gerar_matriz_aleatoria(A, seedA);
    gerar_matriz_aleatoria(B, seedB);

    // 2. Processa multiplicação por Tiling
    mult_tiling(A, B, C);

    // 3. Salva os binários de saída
    salvar_matriz("A.bin", A);
    salvar_matriz("B.bin", B);
    salvar_matriz("C.bin", C);

    if (argc < 4) {
        printf("C (Tiling): Matrizes calculadas e salvas em 'bin_c/' (Seeds: %u, %u)\n", seedA, seedB);
    }

    return 0;
}