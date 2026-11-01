/**
 * ============================================================================
 * @file        matrix_multi.c
 * @brief       Motor de Computação: Multiplicação de Matrizes com Tiling (Bloqueio de Cache).
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este programa executa o produto matricial C = A x B para matrizes quadradas de
 * dimensão fixa N x N (45 x 45 elementos) utilizando a técnica de particionamento em
 * blocos (Tiling / Blocking) com tamanho de bloco B = 8. A implementação emprega 6
 * laços aninhados (3 externos delimitando as coordenadas dos sub-blocos e 3 internos
 * calculando o produto escalar), maximizando o reúso de dados na hierarquia de memória
 * cache (L1/L2) do processador.
 *
 * AUTORES / ESTUDANTES:
 * - Diego Amorim Vidal
 * - André Queiroz Medeiros
 *
 * DATA DE REVISÃO / ENTREGA:
 * 31 de Outubro de 2026
 *
 * PLATAFORMA ALVO:
 * - Plataforma Host de Execução: Host x86_64 (Windows via MinGW/MSVC ou Linux via GCC/Clang).
 * - Arquitetura de Referência: Preparação para sistemas com restrição de memória SRAM/Flash.
 *
 * PERMISSÕES DE USO / COPYRIGHT:
 * Uso exclusivamente acadêmico e didático para avaliação da disciplina.
 *
 * COMO USAR (HELP):
 * 1. Compilação recomendada (com otimizações de vetorização e pipeline):
 *    gcc -O3 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi
 *
 * 2. Execução em Lote (Padrão - processa os 30 testes estatísticos + 3 unitários):
 *    ./matrix_multi
 *
 * 3. Execução Pontual (processa um caso específico via identificador):
 *    ./matrix_multi 1           -> processa A_1.bin e B_1.bin gerando bin_c/C_1.bin
 *    ./matrix_multi identidade -> processa A_identidade.bin e B_identidade.bin
 *
 * ENTRADA E SAÍDA:
 * - Formato: Arquivos binários brutos (raw IEEE 754 float32, 4 bytes por elemento, row-major).
 * - Dimensões: 45 x 45 floats = 2025 elementos (exatamente 8.100 bytes por arquivo).
 * - Entradas: Matrizes A ("input_matrix/A_<id>.bin") e B ("input_matrix/B_<id>.bin").
 * - Saídas:   Matrizes C produto ("bin_c/C_<id>.bin").
 *
 * CLASSIFICAÇÃO DE AUTORIA:
 * - Código Próprio: Gerenciador de arquivos em disco, validação de integridade I/O,
 *                   tratamento de argumentos CLI e orquestração do pipeline de testes.
 * - Código Referência / Adaptado: Estrutura clássica de particionamento em 6 laços aninhados (Tiling):
 *   * Lam, M. S., Rothberg, E. E., & Wolf, M. E. (1991). The cache performance and
 *     optimizations of blocked algorithms. In Proceedings of ASPLOS-IV, pp. 63-74.
 *     DOI: 10.1145/106972.106981.
 *   * Higham, N. J. (2002). Accuracy and Stability of Numerical Algorithms. 2nd ed. SIAM.
 * - Código Gerado: Não aplicável.
 * ============================================================================
 */

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

/*
 * ============================================================================
 * CONSTANTES NOMINATIVAS DO SISTEMA
 * ============================================================================
 */

/** @brief Dimensão fixa das matrizes quadradas (N x N = 45 x 45 elementos). */
#define DIMENSAO_MATRIZ_N              45

/** @brief Total de elementos contidos em cada matriz (45 * 45 = 2025 floats). */
#define TOTAL_ELEMENTOS_MATRIZ         (DIMENSAO_MATRIZ_N * DIMENSAO_MATRIZ_N)

/** @brief Tamanho do sub-bloco de particionamento (BLOCK x BLOCK = 8 x 8 floats = 256 bytes). */
#define TAMANHO_BLOCO_TILING           8

/** @brief Quantidade padrão de baterias de testes estatísticos aleatórios. */
#define QUANTIDADE_TESTES_ESTATISTICOS 30

/** @brief Quantidade de testes unitários específicos (Identidade, Zeros, Forçado). */
#define QUANTIDADE_TESTES_UNITARIOS    3

/** @brief Capacidade máxima em bytes para os buffers de caminho de arquivos. */
#define TAMANHO_BUFFER_CAMINHO         256

/** @brief Capacidade máxima em bytes para nomes de arquivos individuais. */
#define TAMANHO_BUFFER_NOME_ARQUIVO    64

/*
 * ============================================================================
 * PROTÓTIPOS E IMPLEMENTAÇÃO DAS FUNÇÕES
 * ============================================================================
 */

/**
 * @brief       Retorna o menor valor entre dois números inteiros.
 *
 * @details     Função auxiliar inline empregada no particionamento do Tiling para
 *              delimitar a borda superior dos sub-blocos quando N não é divisível
 *              exatamente por TAMANHO_BLOCO_TILING (ex.: 45 % 8 = 5).
 *
 * @param[in]   valor_a Primeiro valor inteiro para comparação.
 * @param[in]   valor_b Segundo valor inteiro para comparação.
 *
 * @return      int O menor valor escalar entre 'valor_a' e 'valor_b'.
 *
 * @note        Restrições de Buffer: Não se aplica (operação estritamente escalar).
 *              Sobrescrita: Nenhuma.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
static inline int min_int(int valor_a, int valor_b) {
    return (valor_a < valor_b) ? valor_a : valor_b;
}

/**
 * @brief       Executa o produto matricial particionado C = A x B via Tiling em 6 laços.
 *
 * @details     Implementa o bloqueio de cache bidimensional. Divide o domínio computacional
 *              em ladrilhos de dimensão TAMANHO_BLOCO_TILING x TAMANHO_BLOCO_TILING.
 *              - Laços externos (bloco_i, bloco_j, bloco_k): percorrem as coordenadas dos ladrilhos.
 *              - Laços internos (i, j, k): computam o produto escalar acumulando os valores
 *                em um registrador escalar local, reduzindo tráfego desnecessário de escrita na memória.
 *              ATENÇÃO: A matriz de saída 'matriz_c' é inicializada com zeros no início
 *              e seu conteúdo anterior é completamente descartado e sobrescrito.
 *
 * @param[in]   matriz_a Matriz de entrada A [N][N] (dimensão 45 x 45 floats, row-major).
 * @param[in]   matriz_b Matriz de entrada B [N][N] (dimensão 45 x 45 floats, row-major).
 * @param[out]  matriz_c Matriz de saída C [N][N] (dimensão 45 x 45 floats). Buffer pré-alocado.
 *                       ATENÇÃO: Buffer integralmente sobrescrito pela computação.
 *
 * @return      void (função sem valor de retorno; matriz calculada é gravada no buffer 'matriz_c').
 *
 * @note        Restrições de Buffer: Todos os três operandos devem ter exatamente [45][45] floats.
 *              Sobrescrita: A matriz 'matriz_c' é totalmente sobrescrita (zerada via memset e preenchida).
 *              Variáveis Globais Afetadas: Nenhuma (função reentrante e autocontida).
 *              Classificação: Código Clássico Adaptado (Lam et al., 1991; Higham, 2002).
 */
void mult_tiling(const float matriz_a[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                 const float matriz_b[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                 float matriz_c[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    /* Zera integralmente o buffer da matriz de destino C */
    memset(matriz_c, 0, sizeof(float) * TOTAL_ELEMENTOS_MATRIZ);

    /* Laço 1 (Externo): Itera sobre as faixas de linhas dos blocos */
    for (int bloco_i = 0; bloco_i < DIMENSAO_MATRIZ_N; bloco_i += TAMANHO_BLOCO_TILING) {
        int limite_i = min_int(bloco_i + TAMANHO_BLOCO_TILING, DIMENSAO_MATRIZ_N);

        /* Laço 2 (Externo): Itera sobre as faixas de colunas dos blocos */
        for (int bloco_j = 0; bloco_j < DIMENSAO_MATRIZ_N; bloco_j += TAMANHO_BLOCO_TILING) {
            int limite_j = min_int(bloco_j + TAMANHO_BLOCO_TILING, DIMENSAO_MATRIZ_N);

            /* Laço 3 (Externo): Itera sobre a dimensão interna compartilhada k entre blocos */
            for (int bloco_k = 0; bloco_k < DIMENSAO_MATRIZ_N; bloco_k += TAMANHO_BLOCO_TILING) {
                int limite_k = min_int(bloco_k + TAMANHO_BLOCO_TILING, DIMENSAO_MATRIZ_N);

                /* Laços 4, 5 e 6 (Internos): Acumulação do produto interno no bloco ativo */
                for (int i = bloco_i; i < limite_i; i++) {
                    for (int j = bloco_j; j < limite_j; j++) {
                        float acumulador_local = matriz_c[i][j];
                        for (int k = bloco_k; k < limite_k; k++) {
                            acumulador_local += matriz_a[i][k] * matriz_b[k][j];
                        }
                        matriz_c[i][j] = acumulador_local;
                    }
                }
            }
        }
    }
}

/**
 * @brief       Constrói o caminho relativo para localização do arquivo conforme o diretório de execução.
 *
 * @details     Identifica se a raiz de trabalho contém a pasta "src_t1". Em caso positivo,
 *              prefixa o caminho com "src_t1/"; caso contrário, formata o caminho de forma direta.
 *
 * @param[out]  caminho_destino  Buffer para receber a string formatada do caminho.
 * @param[in]   tamanho_maximo   Capacidade máxima em bytes do buffer 'caminho_destino'.
 * @param[in]   nome_subpasta    Nome do subdiretório alvo (ex.: "input_matrix", "bin_c").
 * @param[in]   nome_arquivo     Nome do arquivo binário (ex.: "A_1.bin").
 *
 * @return      void (função sem valor de retorno; resultado gravado em 'caminho_destino').
 *
 * @note        Restrições de Buffer: 'caminho_destino' deve comportar pelo menos 'tamanho_maximo' bytes.
 *              Sobrescrita: O conteúdo de 'caminho_destino' é totalmente sobrescrito.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
void obter_caminho_arquivo(char *caminho_destino, size_t tamanho_maximo,
                           const char *nome_subpasta, const char *nome_arquivo) {
    struct stat st;
    if (stat("src_t1", &st) == 0 && (st.st_mode & S_IFDIR)) {
        snprintf(caminho_destino, tamanho_maximo, "src_t1/%s/%s", nome_subpasta, nome_arquivo);
    } else {
        snprintf(caminho_destino, tamanho_maximo, "%s/%s", nome_subpasta, nome_arquivo);
    }
}

/**
 * @brief       Assegura a existência do subdiretório de saída no sistema de arquivos.
 *
 * @details     Verifica se o diretório especificado existe em disco e, caso contrário, cria-o
 *              com permissões padrão de escrita e leitura, tratando a execução na raiz ou em src_t1.
 *
 * @param[in]   nome_subpasta Nome do diretório a ser verificado/criado (ex.: "bin_c").
 *
 * @return      void (função sem valor de retorno; manipula diretórios diretamente no SO).
 *
 * @note        Restrições de Buffer: 'nome_subpasta' deve ser string nula terminada válida.
 *              Sobrescrita: Cria diretórios no sistema de arquivos (não sobrescreve arquivos).
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
void garantir_diretorio(const char *nome_subpasta) {
    char caminho_buffer[TAMANHO_BUFFER_CAMINHO];
    struct stat st;
    if (stat("src_t1", &st) == 0 && (st.st_mode & S_IFDIR)) {
        snprintf(caminho_buffer, sizeof(caminho_buffer), "src_t1/%s", nome_subpasta);
        MKDIR(caminho_buffer);
    }
    MKDIR(nome_subpasta);
}

/**
 * @brief       Carrega uma matriz quadrada N x N em formato binário bruto a partir do disco.
 *
 * @details     Abre o arquivo binário em modo leitura ("rb") e efetua a leitura sequencial
 *              de exatamente N * N elementos float (8.100 bytes para N=45).
 *
 * @param[in]   nome_subpasta    Nome da subpasta onde o arquivo está localizado (ex.: "input_matrix").
 * @param[in]   nome_arquivo     Nome do arquivo binário a ser lido (ex.: "A_1.bin").
 * @param[out]  matriz_destino   Matriz [N][N] de destino onde os 2025 floats serão carregados.
 *                               ATENÇÃO: Conteúdo anterior da matriz é completamente sobrescrito.
 *
 * @return      int 1 em caso de leitura completa e com sucesso (exatamente 2025 floats lidos);
 *                  0 em caso de erro na abertura do arquivo ou leitura incompleta.
 *
 * @note        Restrições de Buffer: A matriz de destino deve possuir capacidade para [45][45] floats.
 *              Sobrescrita: O buffer 'matriz_destino' é totalmente sobrescrito pelos dados lidos.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int carregar_matriz_bin(const char *nome_subpasta, const char *nome_arquivo,
                        float matriz_destino[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    char caminho_buffer[TAMANHO_BUFFER_CAMINHO];
    obter_caminho_arquivo(caminho_buffer, sizeof(caminho_buffer), nome_subpasta, nome_arquivo);

    FILE *arquivo = fopen(caminho_buffer, "rb");
    if (!arquivo) {
        /* Fallback utilizando caminho relativo direto */
        snprintf(caminho_buffer, sizeof(caminho_buffer), "%s/%s", nome_subpasta, nome_arquivo);
        arquivo = fopen(caminho_buffer, "rb");
    }

    if (!arquivo) return 0;

    size_t elementos_lidos = fread(matriz_destino, sizeof(float), TOTAL_ELEMENTOS_MATRIZ, arquivo);
    fclose(arquivo);
    return (elementos_lidos == (size_t)TOTAL_ELEMENTOS_MATRIZ);
}

/**
 * @brief       Salva uma matriz quadrada N x N em formato binário bruto no disco.
 *
 * @details     Abre ou cria o arquivo binário em modo escrita destrutiva ("wb") e grava os
 *              N * N elementos float de forma contígua em formato raw (sem cabeçalhos).
 *              ATENÇÃO: Qualquer arquivo pré-existente no mesmo caminho será integralmente sobrescrito.
 *
 * @param[in]   nome_subpasta Nome da subpasta de destino (ex.: "bin_c").
 * @param[in]   nome_arquivo  Nome do arquivo de saída a ser gravado (ex.: "C_1.bin").
 * @param[in]   matriz_origem Matriz [N][N] contendo os 2025 floats a serem persistidos em disco.
 *
 * @return      int 1 se a escrita de exatamente 2025 floats foi concluída com sucesso;
 *                  0 em caso de erro na abertura do arquivo ou falha de escrita.
 *
 * @note        Restrições de Buffer: A matriz deve conter [45][45] floats contíguos válidos.
 *              Sobrescrita: O arquivo em disco é COMPLETAMENTE SOBRESCRITO via fopen("wb").
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int salvar_matriz_bin(const char *nome_subpasta, const char *nome_arquivo,
                      const float matriz_origem[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    char caminho_buffer[TAMANHO_BUFFER_CAMINHO];
    obter_caminho_arquivo(caminho_buffer, sizeof(caminho_buffer), nome_subpasta, nome_arquivo);

    FILE *arquivo = fopen(caminho_buffer, "wb");
    if (!arquivo) {
        /* Fallback utilizando caminho relativo direto */
        snprintf(caminho_buffer, sizeof(caminho_buffer), "%s/%s", nome_subpasta, nome_arquivo);
        arquivo = fopen(caminho_buffer, "wb");
    }

    if (!arquivo) return 0;

    size_t elementos_escritos = fwrite(matriz_origem, sizeof(float), TOTAL_ELEMENTOS_MATRIZ, arquivo);
    fclose(arquivo);
    return (elementos_escritos == (size_t)TOTAL_ELEMENTOS_MATRIZ);
}

/**
 * @brief       Coordena o ciclo completo de carga, cálculo de Tiling e persistência de um par de matrizes.
 *
 * @details     Executa sequencialmente:
 *              1. Carregamento de 'matriz_a' a partir de "input_matrix/nome_arquivo_a".
 *              2. Carregamento de 'matriz_b' a partir de "input_matrix/nome_arquivo_b".
 *              3. Computação do produto matricial via 'mult_tiling'.
 *              4. Gravação da matriz produto 'matriz_c' em "bin_c/nome_arquivo_c".
 *              ATENÇÃO: Sobrescreve tanto os buffers de memória quanto o arquivo de destino em disco.
 *
 * @param[in]   nome_arquivo_a Nome do arquivo da matriz operando A (ex.: "A_1.bin").
 * @param[in]   nome_arquivo_b Nome do arquivo da matriz operando B (ex.: "B_1.bin").
 * @param[in]   nome_arquivo_c Nome do arquivo de saída para a matriz C (ex.: "C_1.bin").
 * @param[out]  matriz_a       Buffer estático para alocação temporária da matriz A.
 * @param[out]  matriz_b       Buffer estático para alocação temporária da matriz B.
 * @param[out]  matriz_c       Buffer estático para computação e escrita da matriz C.
 *
 * @return      int 1 se todas as 4 etapas foram concluídas com sucesso;
 *                  0 se ocorreu falha em qualquer etapa de I/O ou processamento.
 *
 * @note        Restrições de Buffer: Todos os três buffers devem suportar [45][45] floats.
 *              Sobrescrita: Sobrescreve 'matriz_a', 'matriz_b', 'matriz_c' e o arquivo 'nome_arquivo_c'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int processar_par(const char *nome_arquivo_a, const char *nome_arquivo_b, const char *nome_arquivo_c,
                  float matriz_a[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                  float matriz_b[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                  float matriz_c[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    if (!carregar_matriz_bin("input_matrix", nome_arquivo_a, matriz_a)) return 0;
    if (!carregar_matriz_bin("input_matrix", nome_arquivo_b, matriz_b)) return 0;
    mult_tiling(matriz_a, matriz_b, matriz_c);
    return salvar_matriz_bin("bin_c", nome_arquivo_c, matriz_c);
}

/**
 * @brief       Ponto de entrada principal do programa de multiplicação com Tiling.
 *
 * @details     Gerencia dois modos de operação:
 *              1. Modo Pontual (se argc >= 2):
 *                 Multiplica apenas o par especificado pelo argumento de linha de comando.
 *                 Exemplo: './matrix_multi 5' ou './matrix_multi identidade'.
 *              2. Modo em Lote (padrão sem argumentos):
 *                 Processa a bateria de 30 casos estatísticos e os 3 testes unitários limítrofes
 *                 ("identidade", "zeros", "mforcada").
 *
 * @param[in]   argc Quantidade de argumentos passados na linha de comando.
 * @param[in]   argv Vetor de argumentos (argv[1] opcional: identificador numérico ou nome do teste).
 *
 * @return      int 0 se todos os testes previstos forem processados com sucesso;
 *                  1 se houver qualquer falha de I/O ou processamento.
 *
 * @note        Restrições de Buffer: Os buffers de matriz são alocados com modificador 'static'
 *              no segmento BSS, prevenindo estouro de pilha (Stack Overflow).
 *              Sobrescrita: Sobrescreve integralmente os arquivos de saída correspondentes em 'bin_c/'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int main(int argc, char *argv[]) {
    /* Buffers estáticos no BSS para evitar esgotamento da stack */
    static float matriz_a[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N];
    static float matriz_b[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N];
    static float matriz_c[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N];

    garantir_diretorio("bin_c");

    char nome_arquivo_a[TAMANHO_BUFFER_NOME_ARQUIVO];
    char nome_arquivo_b[TAMANHO_BUFFER_NOME_ARQUIVO];
    char nome_arquivo_c[TAMANHO_BUFFER_NOME_ARQUIVO];

    /* 1. Modo de execução pontual por argumento de linha de comando */
    if (argc >= 2) {
        int indice_teste = atoi(argv[1]);
        if (indice_teste > 0) {
            snprintf(nome_arquivo_a, sizeof(nome_arquivo_a), "A_%d.bin", indice_teste);
            snprintf(nome_arquivo_b, sizeof(nome_arquivo_b), "B_%d.bin", indice_teste);
            snprintf(nome_arquivo_c, sizeof(nome_arquivo_c), "C_%d.bin", indice_teste);
        } else {
            snprintf(nome_arquivo_a, sizeof(nome_arquivo_a), "A_%s.bin", argv[1]);
            snprintf(nome_arquivo_b, sizeof(nome_arquivo_b), "B_%s.bin", argv[1]);
            snprintf(nome_arquivo_c, sizeof(nome_arquivo_c), "C_%s.bin", argv[1]);
        }

        if (processar_par(nome_arquivo_a, nome_arquivo_b, nome_arquivo_c,
                          matriz_a, matriz_b, matriz_c)) {
            printf("C (Tiling): %s gerado com sucesso em 'bin_c/'.\n", nome_arquivo_c);
            return 0;
        } else {
            fprintf(stderr, "[ERRO] Falha ao processar par %s em 'bin_c/'.\n", nome_arquivo_c);
            return 1;
        }
    }

    /* 2. Modo de processamento padrão em lote: 30 baterias estatísticas */
    int total_testes = QUANTIDADE_TESTES_ESTATISTICOS;
    int sucessos_processamento = 0;

    for (int t = 1; t <= total_testes; t++) {
        snprintf(nome_arquivo_a, sizeof(nome_arquivo_a), "A_%d.bin", t);
        snprintf(nome_arquivo_b, sizeof(nome_arquivo_b), "B_%d.bin", t);
        snprintf(nome_arquivo_c, sizeof(nome_arquivo_c), "C_%d.bin", t);

        if (processar_par(nome_arquivo_a, nome_arquivo_b, nome_arquivo_c,
                          matriz_a, matriz_b, matriz_c)) {
            sucessos_processamento++;
        }
    }

    /* 3. Processamento dos 3 testes unitários limítrofes */
    processar_par("A_identidade.bin", "B_identidade.bin", "C_identidade.bin",
                  matriz_a, matriz_b, matriz_c);
    processar_par("A_zeros.bin", "B_zeros.bin", "C_zeros.bin",
                  matriz_a, matriz_b, matriz_c);
    processar_par("A_mforcada.bin", "B_mforcada.bin", "C_mforcada.bin",
                  matriz_a, matriz_b, matriz_c);

    printf("C (Tiling): %d baterias estatisticas e %d testes unitarios calculados em 'bin_c/'.\n",
           sucessos_processamento, QUANTIDADE_TESTES_UNITARIOS);

    return (sucessos_processamento == total_testes) ? 0 : 1;
}
