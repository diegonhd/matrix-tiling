#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#define MKDIR(dir) mkdir(dir, 0755)
#endif

/* Dimensão estritamente fixa das matrizes quadradas (N x N = 45 x 45, 2025 elementos) */
#define N 45

/**
 * Preenche a matriz quadrada N x N com valores uniformes entre -10.0f e +9.9f
 * (dados bipolares simulando sensores como acelerômetros/giroscópios).
 */
void gerar_matriz_aleatoria(float M[N][N], unsigned int seed) {
    srand(seed);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            M[i][j] = ((float)(rand() % 200) - 100.0f) / 10.0f;
        }
    }
}

/**
 * Preenche a matriz como Matriz Identidade I (1.0f na diagonal, 0.0f nas demais).
 */
void gerar_matriz_identidade(float M[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            M[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

/**
 * Preenche a matriz com zeros (Matriz Nula Z).
 */
void gerar_matriz_zeros(float M[N][N]) {
    memset(M, 0, sizeof(float) * N * N);
}

/**
 * Preenche o par de matrizes A e B para forçar ao limite máximo o alinhamento
 * de expoentes e a absorção numérica da mantissa em ponto flutuante IEEE 754 (float32).
 *
 * Estratégia Matemática:
 * - Em cada linha i, um único elemento k = (i % N) recebe magnitude dominante 2^20 = 1048576.0f (E = 20).
 * - Os outros 44 elementos recebem pequenas frações em torno de 2^-4 = 0.0625f (E = -4 a -2).
 * - A diferença de expoente atinge delta_E = 20 - (-4) = 24 bits!
 * - Como a mantissa do float32 tem apenas 24 bits, um deslocamento de 24 bits empurra
 *   os bits dos termos menores para fora do registrador, expondo a não-associatividade.
 */
void gerar_matrizes_alinhamento_forcado(float A[N][N], float B[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            if (k == (i % N)) {
                A[i][k] = 1048576.0f; // 2^20 (magnitude dominante)
            } else {
                A[i][k] = 0.0625f * (float)(1 + (i + k) % 5); // 2^-4 * fator pequeno
            }
        }
    }

    for (int k = 0; k < N; k++) {
        for (int j = 0; j < N; j++) {
            B[k][j] = 1.0f + 0.1f * (float)((k + j) % 3);
        }
    }
}

/**
 * Determina o caminho correto do diretório de destino considerando
 * a execução a partir da raiz do repositório ou de dentro de src_t1/.
 */
void obter_caminho_arquivo(char *dest, size_t max_len, const char *subpasta, const char *nome_arquivo) {
    struct stat st;
    if (stat("src_t1", &st) == 0 && (st.st_mode & S_IFDIR)) {
        snprintf(dest, max_len, "src_t1/%s/%s", subpasta, nome_arquivo);
    } else {
        snprintf(dest, max_len, "%s/%s", subpasta, nome_arquivo);
    }
}

/**
 * Garante a criação do diretório de destino tanto na raiz quanto em src_t1/.
 */
void garantir_diretorio(const char *subpasta) {
    char caminho[256];
    struct stat st;
    if (stat("src_t1", &st) == 0 && (st.st_mode & S_IFDIR)) {
        snprintf(caminho, sizeof(caminho), "src_t1/%s", subpasta);
        MKDIR(caminho);
    }
    MKDIR(subpasta);
}

/**
 * Salva a matriz em formato binário bruto IEEE 754 float32 (single precision).
 */
int salvar_matriz_bin(const char *caminho, float M[N][N]) {
    FILE *arquivo = fopen(caminho, "wb");
    if (!arquivo) {
        const char *barra = strrchr(caminho, '/');
        if (barra) {
            arquivo = fopen(barra + 1, "wb");
        }
    }

    if (!arquivo) {
        fprintf(stderr, "[ERRO] Nao foi possivel criar o arquivo binario: %s\n", caminho);
        return 0;
    }

    size_t escritos = fwrite(M, sizeof(float), N * N, arquivo);
    fclose(arquivo);

    if (escritos != (size_t)(N * N)) {
        fprintf(stderr, "[ERRO] Gravacao incompleta em %s (%lu/%d elementos)\n", caminho, (unsigned long)escritos, N * N);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int total_testes = 30;

    if (argc >= 2) {
        total_testes = atoi(argv[1]);
        if (total_testes <= 0) {
            fprintf(stderr, "[AVISO] Numero de testes invalido (%s). Usando padrao: 30 testes.\n", argv[1]);
            total_testes = 30;
        }
    }

    garantir_diretorio("input_matrix");

    static float A[N][N];
    static float B[N][N];
    char caminho_a[256];
    char caminho_b[256];
    char nome_a[64];
    char nome_b[64];
    int sucessos = 0;

    // 1. Gera os 30 testes estatísticos aleatórios bipolares
    for (int t = 1; t <= total_testes; t++) {
        unsigned int seed_a = 1000 + t;
        unsigned int seed_b = 2000 + t;

        gerar_matriz_aleatoria(A, seed_a);
        gerar_matriz_aleatoria(B, seed_b);

        snprintf(nome_a, sizeof(nome_a), "A_%d.bin", t);
        snprintf(nome_b, sizeof(nome_b), "B_%d.bin", t);

        obter_caminho_arquivo(caminho_a, sizeof(caminho_a), "input_matrix", nome_a);
        obter_caminho_arquivo(caminho_b, sizeof(caminho_b), "input_matrix", nome_b);

        int ok_a = salvar_matriz_bin(caminho_a, A);
        int ok_b = salvar_matriz_bin(caminho_b, B);

        if (ok_a && ok_b) {
            sucessos++;
        }
    }

    // 2. Gera Matrizes para Teste Unitário 1: Matriz Identidade (A_identidade.bin, B_identidade.bin)
    gerar_matriz_aleatoria(A, 1001); // Utiliza matriz A real do teste 1
    gerar_matriz_identidade(B);
    obter_caminho_arquivo(caminho_a, sizeof(caminho_a), "input_matrix", "A_identidade.bin");
    obter_caminho_arquivo(caminho_b, sizeof(caminho_b), "input_matrix", "B_identidade.bin");
    salvar_matriz_bin(caminho_a, A);
    salvar_matriz_bin(caminho_b, B);

    // 3. Gera Matrizes para Teste Unitário 2: Matriz Nula / Zeros (A_zeros.bin, B_zeros.bin)
    gerar_matriz_aleatoria(A, 1001);
    gerar_matriz_zeros(B);
    obter_caminho_arquivo(caminho_a, sizeof(caminho_a), "input_matrix", "A_zeros.bin");
    obter_caminho_arquivo(caminho_b, sizeof(caminho_b), "input_matrix", "B_zeros.bin");
    salvar_matriz_bin(caminho_a, A);
    salvar_matriz_bin(caminho_b, B);

    // 4. Gera Matrizes para Teste Unitário 3: Alinhamento Forçado de Expoentes (A_mforcada.bin, B_mforcada.bin)
    gerar_matrizes_alinhamento_forcado(A, B);
    obter_caminho_arquivo(caminho_a, sizeof(caminho_a), "input_matrix", "A_mforcada.bin");
    obter_caminho_arquivo(caminho_b, sizeof(caminho_b), "input_matrix", "B_mforcada.bin");
    salvar_matriz_bin(caminho_a, A);
    salvar_matriz_bin(caminho_b, B);

    printf("matrix_gen: %d testes estatisticos e 3 testes unitarios gravados em 'input_matrix/'.\n", sucessos);

    return (sucessos == total_testes) ? 0 : 1;
}
