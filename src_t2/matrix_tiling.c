#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define N 45          // Tamanho fixo da matriz (N=45: 45*45 = 2025 elementos em float32)
#define BLOCK 8        // Tamanho do bloco (parametrizável: testar 8, 16, etc.)



static inline int min_int(int a, int b){
    return (a < b) ? a : b;  // função auxiliar para retornar quem é maior entre a e b
}

void mult_tiling(float A[N][N], float B[N][N], float C[N][N]) {
    memset(C, 0, sizeof(float) * N * N); // inicia a matriz resultante C com zeros
 

    for(int ii = 0; ii < N; ii += BLOCK){ // faz a limitação horizontal do primeiro bloco
        int i_max = min_int(ii + BLOCK, N);

        for (int jj = 0; jj < N; jj += BLOCK){ // faz a limitação vertical
            int j_max = min_int(jj + BLOCK, N);

            for (int kk = 0; kk < N; kk += BLOCK){  // faz a iteração da multiplicação (itera a coluna de A e a linha de B)
                int k_max = min_int(kk + BLOCK, N); // k_max recebe a ultima posição da diagonal

                for (int i = ii; i < i_max; i++){ // retorna em que linha a iteração está
                    for (int j = jj; j < j_max; j++){ // retorna em que coluna a iteração está
                        float soma = C[i][j]; // coloca o valor inicial da posição da matriz C na variavel temporaria soma (valor inicial em C é 0-> memset)
                    
                    for (int k = kk; k < k_max; k++){
                        soma += A[i][k] * B[k][j]; // ssoma a multiplicação de cada posição no iterador k(que itera linha de A e coluna de B)
                    }
                    C[i][j] = soma;
                    }
                }
            }
        }
    }
}


