#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define N 45          // Tamanho da matriz (bate o requisito de ~2000 elementos: 45*45=2025)
#define BLOCK 8        // Tamanho do bloco (parametrizável: testar 8, 16, etc.)

void gerar_matriz_aleatoria(float M[N][N], unsigned int seed) {
    srand(seed);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            M[i][j] = (float)(rand() % 100) / 10.0f; // valores entre 0.0 e 9.9
}


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

int carregar_matriz(const char *arquivo, float M[N][N]) {
    FILE *f = fopen(arquivo, "r");
    if (!f) {
        fprintf(stderr, "Erro ao abrir %s\n", arquivo);
        return -1;
    }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (fscanf(f, "%f", &M[i][j]) != 1) {
                fprintf(stderr, "Erro de leitura em %s [%d][%d]\n", arquivo, i, j);
                fclose(f);
                return -1;
            }
    fclose(f);
    return 0;
}

int comparar_matrizes(float X[N][N], float Y[N][N], float tolerancia) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float diff = X[i][j] - Y[i][j];
            if (diff < 0) diff = -diff;
            if (diff > tolerancia) {
                printf("Divergencia em [%d][%d]: %.6f vs %.6f (diff=%.6f)\n",
                       i, j, X[i][j], Y[i][j], diff);
                return 0;
            }
        }
    }
    return 1;
}

int main(void){
    static float A[N][N]; // static evita estourar a pilha com N grande
    static float B[N][N];
    static float C_ingenua[N][N];
    static float C_tiling[N][N];

    clock_t t0, t1;
    gerar_matriz_aleatoria(A, 42);   // seed fixa: reprodutível
    gerar_matriz_aleatoria(B, 123);
    
    // para carregar a matriz em txt

    // if (carregar_matriz("A.txt", A) != 0) return 1;
    // if (carregar_matriz("B.txt", B) != 0) return 1;

    t0 = clock();
    mult_ingenua(A, B, C_ingenua);
    t1 = clock();
    printf("Ingenua:        %.6f s\n", (double)(t1 - t0) / CLOCKS_PER_SEC);

    t0 = clock();
    mult_tiling(A, B, C_tiling);
    t1 = clock();
    printf("Tiling:         %.6f s\n", (double)(t1 - t0) / CLOCKS_PER_SEC);

    printf("\nValidacao tiling vs ingenua:        %s\n",
           comparar_matrizes(C_ingenua, C_tiling, 1e-4f) ? "OK" : "FALHOU");
    return 0;

}