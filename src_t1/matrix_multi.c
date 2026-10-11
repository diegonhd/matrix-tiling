/**
 * ============================================================================
 * @file        matrix_multi.c
 * @brief       Motor de Computação: Multiplicação de Matrizes com Tiling (Bloqueio de Cache).
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este programa realiza a multiplicação de matrizes quadradas de dimensão N x N
 * utilizando a técnica de particionamento em blocos (Tiling / Blocking) com tamanho
 * de bloco BLOCK. O algoritmo emprega 6 laços aninhados (3 externos para iterar
 * sobre as coordenadas dos blocos e 3 internos para a computação dos elementos),
 * otimizando o reuso de dados e a localidade espacial/temporal nas memórias cache
 * L1/L2 do processador.
 *
 * CONTEXTO DE DESENVOLVIMENTO:
 * Trabalho acadêmico desenvolvido na disciplina de Sistemas Embarcados / Arquitetura
 * de Computadores, focado em otimização de cache, desacoplamento modular de software
 * e preparação para execução em sistemas com restrição de memória (e.g., STM32F030F4,
 * ARM Cortex-M0 com apenas 4 KB de RAM e 16 KB de Flash).
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Tipo de Dado: Ponto flutuante IEEE 754 de precisão simples (float, 32 bits / 4 bytes).
 * - Entrada: Arquivos binários raw (sem cabeçalho) contendo N x N floats dispostos
 *            sequencialmente em ordem 'row-major' (8.100 bytes para N=45):
 *            - Matrizes A: "input_matrix/A_<id>.bin"
 *            - Matrizes B: "input_matrix/B_<id>.bin"
 * - Saída:   Arquivos binários raw contendo a matriz produto C resultante (float32):
 *            - Matrizes C: "bin_c/C_<id>.bin"
 *
 * COMO USAR (HELP):
 * 1. Compilação (via GCC ou Clang):
 *    gcc -O3 matrix_multi.c -o matrix_multi
 *
 * 2. Execução em Lote (Padrão - processa os 30 testes estatísticos + 3 unitários):
 *    ./matrix_multi
 *
 * 3. Execução Pontual (processa um par específico A_<id>.bin e B_<id>.bin):
 *    ./matrix_multi 1            -> processa A_1.bin e B_1.bin gerando bin_c/C_1.bin
 *    ./matrix_multi identidade  -> processa A_identidade.bin e B_identidade.bin gerando bin_c/C_identidade.bin
 *
 * PLATAFORMA ALVO:
 * - Plataforma Host: Windows (MSVC/MinGW) e Linux/POSIX (GCC/Clang), x86_64.
 * - Plataforma de Destino Futura: Microcontrolador ARM Cortex-M0 (STM32F030F4).
 *
 * AUTOR / ESTUDANTE:
 * Diego Amorim Vidal (diegonhd) <diegoamorimvidal@gmail.com>
 *
 * DATA:
 * Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Lógica de I/O em binário, portabilidade entre pastas/SO,
 *                   tratamento de argumentos da CLI e pipeline de testes.
 * - Código Referência/Base: Algoritmo clássico de Matrix Multiplication with Tiling
 *                          (Lam, Rothberg, Wolf - ASPLOS / Hennessy & Patterson).
 * - Código Gerado: Não aplicável.
 *
 * PERMISSÕES DE USO E COPYRIGHT:
 * Copyright (c) 2026 Diego Amorim Vidal.
 * Uso educacional e acadêmico livre mediante citação da fonte.
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Compatibilidade de chamadas do sistema de arquivos entre Windows e POSIX */
#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#define MKDIR(dir) mkdir(dir, 0755)
#endif

/*
 * Dimensão estritamente fixa das matrizes quadradas (N x N = 45 x 45, 2025 elementos).
 * Cada matriz em float32 ocupa exatamente 8.100 bytes em disco e memória.
 */
#define N 45

/*
 * Dimensão do bloco de particionamento (BLOCK x BLOCK) para o Tiling.
 * BLOCK = 8 garante que submatrizes 8x8 (256 bytes) caibam confortavelmente na Cache L1.
 */
#define BLOCK 8

/**
 * @brief       Retorna o menor valor entre dois números inteiros.
 *
 * @details     Função auxiliar 'inline' utilizada para calcular o limite superior
 *              das iterações nos blocos periféricos (tratamento de bordas quando
 *              N não é múltiplo exato de BLOCK).
 *
 * @param[in]   a Primeiro número inteiro para comparação.
 * @param[in]   b Segundo número inteiro para comparação.
 *
 * @return      O menor valor entre 'a' e 'b'.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
 */
static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * @brief       Realiza a multiplicação de matrizes C = A x B utilizando a técnica de Tiling.
 *
 * @details     O algoritmo particiona as matrizes em blocos de tamanho BLOCK x BLOCK.
 *              Utiliza 6 laços aninhados:
 *              - Laços externos (ii, jj, kk): percorrem as coordenadas dos sub-blocos.
 *              - Laços internos (i, j, k): realizam o produto escalar dentro do bloco ativo.
 *              O uso de 'min_int' assegura a corretude mesmo caso a dimensão N não
 *              seja múltiplo exato do tamanho do bloco (BLOCK).
 *
 * @param[in]   A Matriz multiplicando de entrada de dimensão N x N (float).
 * @param[in]   B Matriz multiplicador de entrada de dimensão N x N (float).
 * @param[out]  C Matriz produto de saída de dimensão N x N (float), zerada no início da função.
 *
 * @return      void.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Algoritmo clássico de Tiling/Blocking adaptado para float.
 */
void mult_tiling(float A[N][N], float B[N][N], float C[N][N]) {
    /* Inicializa toda a matriz de saída C com zeros */
    memset(C, 0, sizeof(float) * N * N);

    /* Laço externo ii: itera sobre as linhas de blocos */
    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N); /* Limite superior do bloco em i */

        /* Laço externo jj: itera sobre as colunas de blocos */
        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N); /* Limite superior do bloco em j */

            /* Laço externo kk: itera sobre a dimensão compartilhada entre blocos */
            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N); /* Limite superior do bloco em k */

                /* Laços internos: multiplicação escalar no bloco ativo */
                for (int i = ii; i < i_max; i++) {
                    for (int j = jj; j < j_max; j++) {
                        float soma = C[i][j]; /* Acumulador local em registrador */
                        for (int k = kk; k < k_max; k++) {
                            soma += A[i][k] * B[k][j];
                        }
                        C[i][j] = soma; /* Atualiza o elemento acumulado da matriz C */
                    }
                }
            }
        }
    }
}

/**
 * @brief       Constrói o caminho relativo para um arquivo considerando a pasta de execução.
 *
 * @details     Verifica se a pasta "src_t1" existe no diretório atual de trabalho.
 *              Se existir (execução a partir da raiz do repositório), prefixa "src_t1/".
 *              Caso contrário (execução já dentro de "src_t1"), usa o caminho direto.
 *
 * @param[out]  dest         Buffer de destino onde o caminho formatado será armazenado.
 * @param[in]   max_len      Tamanho máximo (em bytes) suportado pelo buffer 'dest'.
 * @param[in]   subpasta     Nome da subpasta do recurso (ex: "input_matrix", "bin_c").
 * @param[in]   nome_arquivo Nome do arquivo a ser acessado (ex: "A_1.bin").
 *
 * @return      void.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
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
 * @brief       Assegura a existência do diretório de saída no disco.
 *
 * @details     Tenta criar o diretório tanto com prefixo "src_t1/" (caso o programa
 *              esteja rodando da raiz) quanto de forma direta, garantindo que o diretório
 *              de destino exista antes da escrita de arquivos binários.
 *
 * @param[in]   subpasta Nome da subpasta a ser verificada/criada (ex: "bin_c").
 *
 * @return      void.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
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
 * @brief       Carrega uma matriz quadrada N x N em formato binário bruto a partir do disco.
 *
 * @details     Abre o arquivo binário em modo leitura ("rb") e efetua a leitura sequencial
 *              de exatamente N * N elementos do tipo float (precisão simples de 4 bytes).
 *
 * @param[in]   subpasta     Nome da subpasta onde o arquivo está localizado (ex: "input_matrix").
 * @param[in]   nome_arquivo Nome do arquivo binário a ser lido (ex: "A_1.bin").
 * @param[out]  M            Matriz N x N (float) de destino onde os dados serão alocados.
 *
 * @return      1 em caso de leitura completa e com sucesso (N*N floats lidos),
 *              0 em caso de erro na abertura ou dados corrompidos/incompletos.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
 */
int carregar_matriz_bin(const char *subpasta, const char *nome_arquivo, float M[N][N]) {
    char caminho[256];
    obter_caminho_arquivo(caminho, sizeof(caminho), subpasta, nome_arquivo);

    FILE *arquivo = fopen(caminho, "rb");
    if (!arquivo) {
        /* Tentativa de fallback usando o caminho relativo direto */
        snprintf(caminho, sizeof(caminho), "%s/%s", subpasta, nome_arquivo);
        arquivo = fopen(caminho, "rb");
    }

    if (!arquivo) return 0;

    size_t lidos = fread(M, sizeof(float), N * N, arquivo);
    fclose(arquivo);
    return (lidos == (size_t)(N * N));
}

/**
 * @brief       Salva uma matriz quadrada N x N em formato binário bruto no disco.
 *
 * @details     Abre ou cria o arquivo binário em modo escrita ("wb") e grava os
 *              N * N elementos float de forma contígua em formato raw (sem cabeçalho).
 *
 * @param[in]   subpasta     Nome da subpasta de destino (ex: "bin_c").
 * @param[in]   nome_arquivo Nome do arquivo a ser gravado (ex: "C_1.bin").
 * @param[in]   M            Matriz N x N (float) contendo os dados a serem salvos.
 *
 * @return      1 em caso de gravação completa e com sucesso (N*N floats escritos),
 *              0 em caso de erro ao criar ou gravar no arquivo.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
 */
int salvar_matriz_bin(const char *subpasta, const char *nome_arquivo, float M[N][N]) {
    char caminho[256];
    obter_caminho_arquivo(caminho, sizeof(caminho), subpasta, nome_arquivo);

    FILE *arquivo = fopen(caminho, "wb");
    if (!arquivo) {
        /* Tentativa de fallback usando o caminho relativo direto */
        snprintf(caminho, sizeof(caminho), "%s/%s", subpasta, nome_arquivo);
        arquivo = fopen(caminho, "wb");
    }

    if (!arquivo) return 0;

    size_t escritos = fwrite(M, sizeof(float), N * N, arquivo);
    fclose(arquivo);
    return (escritos == (size_t)(N * N));
}

/**
 * @brief       Coordena o ciclo completo de carregamento, multiplicação e persistência de um par de matrizes.
 *
 * @details     Realiza a carga das matrizes A e B de 'input_matrix/', invoca 'mult_tiling'
 *              para computar o produto C e salva a matriz resultante em 'bin_c/'.
 *
 * @param[in]   nome_a Nome do arquivo da matriz multiplicando A (ex: "A_1.bin").
 * @param[in]   nome_b Nome do arquivo da matriz multiplicador B (ex: "B_1.bin").
 * @param[in]   nome_c Nome do arquivo de saída da matriz C (ex: "C_1.bin").
 * @param[out]  A      Buffer estático/alocado para receber a matriz A.
 * @param[out]  B      Buffer estático/alocado para receber a matriz B.
 * @param[out]  C      Buffer estático/alocado para receber e computar a matriz C.
 *
 * @return      1 se todas as etapas (carregamento de A, B, multiplicação e salvamento de C)
 *              foram concluídas com êxito, 0 caso ocorra falha em qualquer etapa.
 *
 * @note        Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
 */
int processar_par(const char *nome_a, const char *nome_b, const char *nome_c,
                   float A[N][N], float B[N][N], float C[N][N]) {
    if (!carregar_matriz_bin("input_matrix", nome_a, A)) return 0;
    if (!carregar_matriz_bin("input_matrix", nome_b, B)) return 0;
    mult_tiling(A, B, C);
    return salvar_matriz_bin("bin_c", nome_c, C);
}

/**
 * @brief       Ponto de entrada principal do programa.
 *
 * @details     Gerencia o modo de execução:
 *              1. Modo Pontual (se argc >= 2):
 *                 Multiplica apenas o par especificado via argumento de linha de comando.
 *                 Exemplo: './matrix_multi 5' ou './matrix_multi identidade'.
 *              2. Modo em Lote (padrão sem argumentos):
 *                 Processa os 30 casos de teste estatísticos (de 1 a 30) e os 3 testes
 *                 unitários especiais ("identidade", "zeros", "mforcada").
 *
 * @param[in]   argc Quantidade de argumentos passados na linha de comando.
 * @param[in]   argv Vetor de strings com os argumentos da linha de comando.
 *
 * @return      0 se todos os testes previstos forem processados com sucesso;
 *              1 se houver qualquer falha de processamento ou leitura/escrita.
 *
 * @note        Variáveis locais estáticas:
 *              - static float A[N][N]: Matriz temporária de entrada A (~8.100 bytes).
 *              - static float B[N][N]: Matriz temporária de entrada B (~8.100 bytes).
 *              - static float C[N][N]: Matriz de saída do produto C (~8.100 bytes).
 *              (O uso do modificador 'static' aloca os buffers na seção BSS/Data,
 *              evitando estouro da pilha de execução - Stack Overflow).
 *              Variáveis globais afetadas: Nenhuma.
 *              Origem: Código próprio.
 */
int main(int argc, char *argv[]) {
    /* Buffers estáticos para evitar alocação excessiva na stack do SO/microcontrolador */
    static float A[N][N];
    static float B[N][N];
    static float C[N][N];

    /* Garante a criação do diretório de destino dos arquivos de saída */
    garantir_diretorio("bin_c");

    /* Modo de execução pontual quando argumentos são fornecidos */
    if (argc >= 2) {
        int t = atoi(argv[1]);
        char na[64], nb[64], nc[64];
        if (t > 0) {
            snprintf(na, sizeof(na), "A_%d.bin", t);
            snprintf(nb, sizeof(nb), "B_%d.bin", t);
            snprintf(nc, sizeof(nc), "C_%d.bin", t);
        } else {
            snprintf(na, sizeof(na), "A_%s.bin", argv[1]);
            snprintf(nb, sizeof(nb), "B_%s.bin", argv[1]);
            snprintf(nc, sizeof(nc), "C_%s.bin", argv[1]);
        }
        if (processar_par(na, nb, nc, A, B, C)) {
            printf("C (Tiling): %s gerado em 'bin_c/'.\n", nc);
            return 0;
        } else {
            fprintf(stderr, "Erro ao processar %s em 'bin_c/'.\n", nc);
            return 1;
        }
    }

    /* Processamento em lote padrão: 30 baterias de testes estatísticos */
    int total_testes = 30;
    int sucessos = 0;
    char na[64], nb[64], nc[64];

    for (int t = 1; t <= total_testes; t++) {
        snprintf(na, sizeof(na), "A_%d.bin", t);
        snprintf(nb, sizeof(nb), "B_%d.bin", t);
        snprintf(nc, sizeof(nc), "C_%d.bin", t);
        if (processar_par(na, nb, nc, A, B, C)) {
            sucessos++;
        }
    }

    /* Processa os 3 testes unitários caso existam em input_matrix/ */
    processar_par("A_identidade.bin", "B_identidade.bin", "C_identidade.bin", A, B, C);
    processar_par("A_zeros.bin", "B_zeros.bin", "C_zeros.bin", A, B, C);
    processar_par("A_mforcada.bin", "B_mforcada.bin", "C_mforcada.bin", A, B, C);

    printf("C (Tiling): %d matrizes calculadas e salvas em 'bin_c/'.\n", sucessos);
    return (sucessos == total_testes) ? 0 : 1;
}