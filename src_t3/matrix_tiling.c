#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define N 45          // Tamanho da matriz (bate o requisito de ~2000 elementos: 45*45=2025)
#define BLOCK 8        // Tamanho do bloco (parametrizável: testar 8, 16, etc.)
#define NUM_TESTES 10  //Numero de multiplicações desejadas

int min_int(int a, int b) {
    return (a < b) ? a : b;
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

/* Gera números aleatórios entre -10 e 10 */
float aleatorio() {
    return ((float)rand() / RAND_MAX) * 20.0f - 10.0f;
}


/* Salva uma matriz em arquivo TXT */
void salvar_matriz(const char *nome, float M[N][N]) {

    FILE *arquivo = fopen(nome, "w");

    if (arquivo == NULL) {
        printf("Erro ao criar arquivo %s\n", nome);
        exit(1);
    }

    for (int i = 0; i < N; i++) {

        for (int j = 0; j < N; j++) {
            fprintf(arquivo, "%.8f ", M[i][j]);
        }

        fprintf(arquivo, "\n");
    }

    fclose(arquivo);
}


int main() {

    srand(12345);  // mesma sequência aleatória em cada execução

    float A[N][N];
    float B[N][N];
    float C[N][N];

    for (int teste = 1; teste <= NUM_TESTES; teste++) {

        /* Gera A e B */
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {

                A[i][j] = aleatorio();
                B[i][j] = aleatorio();

            }
        }

        /* Multiplicação usando seu algoritmo */
        mult_tiling(A, B, C);

        /* Salva os dados */
        char nomeA[50];
        char nomeB[50];
        char nomeC[50];

        sprintf(nomeA, "teste_%d_A.txt", teste);
        sprintf(nomeB, "teste_%d_B.txt", teste);
        sprintf(nomeC, "teste_%d_C.txt", teste);

        salvar_matriz(nomeA, A);
        salvar_matriz(nomeB, B);
        salvar_matriz(nomeC, C);

        printf("Teste %d concluido.\n", teste);
    }

    printf("\n10 testes gerados com sucesso!\n");

    return 0;
}