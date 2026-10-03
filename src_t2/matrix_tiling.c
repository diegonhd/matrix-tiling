#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifndef N
#define N 45          // Tamanho da matriz (bate o requisito de ~2000 elementos: 45*45=2025)
#endif
#define BLOCK 8        // Tamanho do bloco (parametrizável: testar 8, 16, etc.)


void mult_ingenua(float A[N][N], float B[N][N], float C[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float soma = 0.0f;
            for (int k = 0; k < N; k++) {
                soma += A[i][k] * B[k][j];
            }
            C[i][j] = soma;
        }
    }
}

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

void mult_tiling_buffers(const float A[N][N], const float B[N][N], float C[N][N]) {
    static float blocoA[BLOCK][BLOCK];
    static float blocoB[BLOCK][BLOCK];
    static float blocoC[BLOCK][BLOCK];

    memset(C, 0, sizeof(float) * N * N);
    for (int ii = 0; ii < N; ii += BLOCK){ // iteram sobre o tamanho do bloco para seguir adiante até a iteração N/BLOCK
        int bi = min_int(BLOCK, N - ii); // tamanho do bloco na atual borda vertical

        for (int jj = 0; jj < N; jj += BLOCK){ 
            int bj = min_int(BLOCK, N - jj); // tamanho do block na atual horizontal
        
        for (int i = 0; i < bi; i++){ // laço que zera as linhas do atual Bloco de C depois de toda as iterações verticais
            for (int j = 0; j < bj; j++){
            blocoC[i][j] = 0.0f;
            }
        }

        
        for (int kk = 0; kk < N; kk += BLOCK) { // laço da diagonal (itera até os ultimos iterados verticalmente e horizontalmente)
            int bk = min_int(BLOCK, N - kk); // retorna a borda

            for (int i = 0; i < bi; i++) // itera até a borda bi
                for (int k = 0; k < bk; k++) // itera até a borda bk
                    blocoA[i][k] = A[ii + i][kk + k]; // vai adicionando os dados do Bloco A
            // o laço de linha é o mais externo porque as matrizes são carregadas linha por linha 
            // então para que a leitura seja rápida, o laço mais interno deve iterar sobre as colunas

            for (int k = 0; k < bk; k++) 
                for (int j = 0; k < bj; j++)
                    blocoB[k][j] = B[kk + k][jj + j];
            // a mesma logica; os laços são "invertidos" pois o iterado j representa as colunas assim como bj representa a borda das colunas

            // aqui é o laço em que ocorre as multiplicações dos blocos
            for (int i = 0; i < bi; i++)
                for (int j = 0; j < bj; j++)
                    for (int k = 0; k < bk; k++)
                        blocoC[i][j] += blocoA[i][k] * blocoB[k][j]; 
            }

            for (int i = 0; i < bi; i++)
                for (int j = 0; j < bj; j++)
                    C[ii + i][jj + j] = blocoC[i][j]; // armazena tudo na matriz final C
        } 
    }
}

