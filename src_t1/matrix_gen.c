/**
 * ============================================================================
 * @file        matrix_gen.c
 * @brief       Gerador determinístico de matrizes de teste em formato binário bruto IEEE 754 (float32).
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este programa é responsável por sintetizar as matrizes de entrada A e B para
 * validação numérica e metrológica do algoritmo de multiplicação de matrizes com
 * Tiling. Produz uma bateria de 30 pares de matrizes estatísticas (distribuição
 * uniforme bipolar) e 3 pares de testes unitários fundamentais da Álgebra Linear
 * (Identidade, Nula e Alinhamento Forçado de Expoentes).
 *
 * AUTORES / ESTUDANTES:
 * - Diego Amorim Vidal
 * - André Queiroz Medeiros
 *
 * DATA DE REVISÃO / ENTREGA:
 * 31 de Outubro de 2026
 *
 * PLATAFORMA ALVO:
 * Host x86_64 (Windows via MinGW/MSVC ou Linux via GCC/Clang).
 *
 * PERMISSÕES DE USO / COPYRIGHT:
 * Uso exclusivamente didático e acadêmico para fins de avaliação curricular.
 *
 * COMO USAR (HELP):
 * 1. Compilação (GCC recomendado):
 *    gcc -O2 -Wall -Wextra src_t1/matrix_gen.c -o matrix_gen
 * 2. Execução padrão (gera 30 pares estatísticos + 3 pares unitários):
 *    ./matrix_gen
 * 3. Execução personalizada (alterando quantidade de testes estatísticos):
 *    ./matrix_gen 50
 *
 * ENTRADA E SAÍDA:
 * - Entrada: Nenhuma (parâmetros de semente gerados internamente de forma determinística).
 * - Saída:   Arquivos binários raw (sem cabeçalho) gravados no subdiretório 'input_matrix/':
 *            - Pares estatísticos: A_1.bin a A_30.bin e B_1.bin a B_30.bin (8.100 bytes cada).
 *            - Casos unitários: A_identidade.bin, B_identidade.bin, A_zeros.bin, B_zeros.bin,
 *              A_mforcada.bin, B_mforcada.bin.
 *
 * CLASSIFICAÇÃO DE AUTORIA:
 * - Código Próprio: Lógica de geração de arquivos binários, infraestrutura de diretórios,
 *                   tratamento de argumentos CLI e caso de alinhamento forçado.
 * - Código Referência / Adaptado: Caso de estresse numérico fundamentado no modelo analítico
 *                                 de propagação de erros de Higham (2002).
 *   Referência: Higham, N. J. (2002). Accuracy and Stability of Numerical Algorithms. 2nd ed. SIAM.
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

/** @brief Dimensão das matrizes quadradas (N x N = 45 x 45 elementos). */
#define DIMENSAO_MATRIZ_N              45

/** @brief Total de elementos contidos em cada matriz (45 * 45 = 2025 floats). */
#define TOTAL_ELEMENTOS_MATRIZ         (DIMENSAO_MATRIZ_N * DIMENSAO_MATRIZ_N)

/** @brief Quantidade padrão de baterias de testes estatísticos aleatórios. */
#define QUANTIDADE_TESTES_ESTATISTICOS 30

/** @brief Quantidade de testes unitários específicos (Identidade, Zeros, Forçado). */
#define QUANTIDADE_TESTES_UNITARIOS    3

/** @brief Tamanho máximo dos buffers para composição de caminhos de arquivos. */
#define TAMANHO_BUFFER_CAMINHO         256

/** @brief Tamanho máximo dos buffers para nomes de arquivos. */
#define TAMANHO_BUFFER_NOME_ARQUIVO    64

/** @brief Offset base para geração determinística da semente pseudoaleatória da matriz A. */
#define SEMENTE_BASE_MATRIZ_A          1000

/** @brief Offset base para geração determinística da semente pseudoaleatória da matriz B. */
#define SEMENTE_BASE_MATRIZ_B          2000

/** @brief Amplitude de discretização dos valores pseudoaleatórios [-10.0f, +9.9f]. */
#define FATOR_DISCRETIZACAO_RAND       200

/** @brief Deslocamento de centro para geração de dados bipolares. */
#define DESLOCAMENTO_BIPOLAR           100.0f

/** @brief Fator de escala para ajuste de casas decimais. */
#define ESCALA_DECIMAL                 10.0f

/** @brief Magnitude dominante (2^20) para o teste de alinhamento forçado. */
#define MAGNITUDE_DOMINANTE_FORCADA    1048576.0f

/** @brief Fração pequena base (2^-4) para o teste de alinhamento forçado. */
#define FRACAO_PEQUENA_BASE            0.0625f

/*
 * ============================================================================
 * PROTÓTIPOS E IMPLEMENTAÇÃO DAS FUNÇÕES
 * ============================================================================
 */

/**
 * @brief       Preenche uma matriz com valores pseudoaleatórios em distribuição bipolar.
 *
 * @details     Utiliza uma semente determinística fixa para inicializar o gerador pseudoaleatório
 *              'rand()', preenchendo a matriz com valores no intervalo [-10.0f, +9.9f] com
 *              passo de 0.1f, simulando sinais capturados por sensores físicos.
 *
 * @param[out]  matriz_destino Matriz quadrada [N][N] de destino. Buffer pré-alocado.
 *                             ATENÇÃO: Todo o conteúdo anterior da matriz é sobrescrito.
 * @param[in]   semente        Valor numérico utilizado para inicializar 'srand()'.
 *
 * @return      void (função sem valor de retorno; resultado gravado em 'matriz_destino').
 *
 * @note        Restrições de Buffer: A matriz deve possuir dimensão estrita [45][45] de floats.
 *              Sobrescrita: Sobrescreve completamente os 2025 elementos de 'matriz_destino'.
 *              Variáveis Globais Afetadas: Nenhuma (afeta apenas o estado interno da libc `rand`).
 *              Classificação: Código próprio.
 */
void gerar_matriz_aleatoria(float matriz_destino[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                            unsigned int semente) {
    srand(semente);
    for (int i = 0; i < DIMENSAO_MATRIZ_N; i++) {
        for (int j = 0; j < DIMENSAO_MATRIZ_N; j++) {
            matriz_destino[i][j] = ((float)(rand() % FATOR_DISCRETIZACAO_RAND) - DESLOCAMENTO_BIPOLAR) / ESCALA_DECIMAL;
        }
    }
}

/**
 * @brief       Gera a Matriz Identidade I de dimensão N x N.
 *
 * @details     Atribui 1.0f aos elementos da diagonal principal (i == j) e 0.0f aos
 *              demais elementos, permitindo validar a propriedade neutra da multiplicação: A x I = A.
 *
 * @param[in]   (Nenhum parâmetro de entrada escalar; opera sobre a constante DIMENSAO_MATRIZ_N).
 * @param[out]  matriz_destino Matriz quadrada [N][N] de destino. Buffer pré-alocado.
 *                             ATENÇÃO: Todo o conteúdo anterior da matriz é sobrescrito.
 *
 * @return      void (função sem valor de retorno; resultado gravado em 'matriz_destino').
 *
 * @note        Restrições de Buffer: Dimensão estrita [45][45] de floats.
 *              Sobrescrita: Sobrescreve completamente os 2025 elementos de 'matriz_destino'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
void gerar_matriz_identidade(float matriz_destino[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    for (int i = 0; i < DIMENSAO_MATRIZ_N; i++) {
        for (int j = 0; j < DIMENSAO_MATRIZ_N; j++) {
            matriz_destino[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

/**
 * @brief       Gera a Matriz Nula (Zeros) de dimensão N x N.
 *
 * @details     Preenche todos os bytes da matriz com zero via 'memset', permitindo verificar
 *              a propriedade aniquiladora da multiplicação: A x 0 = 0.
 *
 * @param[in]   (Nenhum parâmetro de entrada escalar; opera sobre a constante DIMENSAO_MATRIZ_N).
 * @param[out]  matriz_destino Matriz quadrada [N][N] de destino. Buffer pré-alocado.
 *                             ATENÇÃO: Todo o conteúdo anterior da matriz é sobrescrito.
 *
 * @return      void (função sem valor de retorno; buffer preenchido via memset).
 *
 * @note        Restrições de Buffer: Dimensão estrita [45][45] de floats.
 *              Sobrescrita: Sobrescreve completamente os 2025 elementos de 'matriz_destino'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
void gerar_matriz_zeros(float matriz_destino[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    memset(matriz_destino, 0, sizeof(float) * TOTAL_ELEMENTOS_MATRIZ);
}

/**
 * @brief       Gera par de matrizes de teste para forçar o limite de alinhamento de expoentes no IEEE 754.
 *
 * @details     Em cada linha i da matriz A, um único elemento k = (i % N) recebe a magnitude
 *              dominante 2^20 (1048576.0f), enquanto os demais 44 elementos recebem frações na ordem
 *              de 2^-4 (0.0625f). A diferença de expoentes atinge delta_E = 20 - (-4) = 24 bits,
 *              o que iguala o comprimento total da mantissa do float32 (24 bits significativos),
 *              forçando o descarte de bits e evidenciando a não-associatividade do ponto flutuante.
 *
 * @param[in]   (Nenhum parâmetro de entrada escalar; cálculo determinístico fixo).
 * @param[out]  matriz_a Matriz [N][N] de destino para os operandos com magnitude extrema.
 *                       ATENÇÃO: Conteúdo anterior completamente sobrescrito.
 * @param[out]  matriz_b Matriz [N][N] de destino para os coeficientes multiplicadores controlados.
 *                       ATENÇÃO: Conteúdo anterior completamente sobrescrito.
 *
 * @return      void (função sem valor de retorno; resultados gravados em 'matriz_a' e 'matriz_b').
 *
 * @note        Restrições de Buffer: Ambos os buffers devem ter dimensão estrita [45][45] floats.
 *              Sobrescrita: Sobrescreve integralmente 'matriz_a' e 'matriz_b'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código Próprio fundamentado no modelo analítico de Higham (2002).
 */
void gerar_matrizes_alinhamento_forcado(float matriz_a[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N],
                                        float matriz_b[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    for (int i = 0; i < DIMENSAO_MATRIZ_N; i++) {
        for (int k = 0; k < DIMENSAO_MATRIZ_N; k++) {
            if (k == (i % DIMENSAO_MATRIZ_N)) {
                matriz_a[i][k] = MAGNITUDE_DOMINANTE_FORCADA;
            } else {
                matriz_a[i][k] = FRACAO_PEQUENA_BASE * (float)(1 + (i + k) % 5);
            }
        }
    }

    for (int k = 0; k < DIMENSAO_MATRIZ_N; k++) {
        for (int j = 0; j < DIMENSAO_MATRIZ_N; j++) {
            matriz_b[k][j] = 1.0f + 0.1f * (float)((k + j) % 3);
        }
    }
}

/**
 * @brief       Monta o caminho relativo do arquivo conforme a pasta de execução atual.
 *
 * @details     Detecta dinamicamente se o programa foi disparado a partir da raiz do repositório
 *              (onde 'src_t1' existe como subdiretório) ou de dentro de 'src_t1/', formatando
 *              o caminho de modo transparente para portabilidade.
 *
 * @param[out]  caminho_destino  Buffer para receber a string com o caminho resultante.
 * @param[in]   tamanho_maximo   Capacidade máxima em bytes do buffer de destino.
 * @param[in]   nome_subpasta    Nome do subdiretório alvo (ex.: "input_matrix").
 * @param[in]   nome_arquivo     Nome do arquivo binário a ser acessado (ex.: "A_1.bin").
 *
 * @return      void (sem valor de retorno; caminho formatado gravado em 'caminho_destino').
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
 * @brief       Assegura a existência física do diretório de destino no sistema de arquivos.
 *
 * @details     Cria a pasta alvo tanto com prefixo "src_t1/" quanto de forma relativa direta,
 *              evitando falhas de I/O caso a pasta ainda não exista em disco.
 *
 * @param[in]   nome_subpasta Nome do diretório a ser verificado/criado (ex.: "input_matrix").
 *
 * @return      void (sem valor de retorno; opera diretamente no sistema de arquivos do SO).
 *
 * @note        Restrições de Buffer: 'nome_subpasta' deve ser string nula terminada válida.
 *              Sobrescrita: Cria diretórios no sistema de arquivos (não sobrescreve dados existentes).
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
 * @brief       Grava uma matriz bidimensional em arquivo binário bruto float32 no disco.
 *
 * @details     Abre o arquivo alvo em modo binário destrutivo ("wb"), gravando exatamente
 *              N * N floats de 4 bytes sequencialmente em formato raw (sem cabeçalhos ou metadados).
 *              ATENÇÃO: Qualquer arquivo pré-existente no mesmo caminho será integralmente sobrescrito.
 *
 * @param[in]   caminho_arquivo Caminho completo ou relativo para o arquivo de gravação.
 * @param[in]   matriz_dados    Matriz bidimensional [N][N] contendo os dados float32.
 *
 * @return      int 1 em caso de gravação completa com êxito (exatamente 2025 floats gravados);
 *                  0 em caso de falha de abertura do arquivo ou escrita incompleta.
 *
 * @note        Restrições de Buffer: A matriz de entrada deve ter [45][45] elementos float32 contíguos.
 *              Sobrescrita: O arquivo em disco é COMPLETAMENTE SOBRESCRITO via fopen("wb").
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int salvar_matriz_bin(const char *caminho_arquivo,
                      float matriz_dados[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N]) {
    FILE *arquivo = fopen(caminho_arquivo, "wb");
    if (!arquivo) {
        const char *barra = strrchr(caminho_arquivo, '/');
        if (barra) {
            arquivo = fopen(barra + 1, "wb");
        }
    }

    if (!arquivo) {
        fprintf(stderr, "[ERRO] Nao foi possivel criar o arquivo binario: %s\n", caminho_arquivo);
        return 0;
    }

    size_t elementos_escritos = fwrite(matriz_dados, sizeof(float), TOTAL_ELEMENTOS_MATRIZ, arquivo);
    fclose(arquivo);

    if (elementos_escritos != (size_t)TOTAL_ELEMENTOS_MATRIZ) {
        fprintf(stderr, "[ERRO] Gravacao incompleta em %s (%lu/%d elementos)\n",
                caminho_arquivo, (unsigned long)elementos_escritos, TOTAL_ELEMENTOS_MATRIZ);
        return 0;
    }
    return 1;
}

/**
 * @brief       Ponto de entrada principal do gerador de matrizes de teste.
 *
 * @details     Inicializa o diretório de destino, interpreta argumentos de linha de comando
 *              (quantidade de testes) e orquestra a geração e persistência das matrizes A e B
 *              para os testes estatísticos e os 3 testes unitários limítrofes.
 *
 * @param[in]   argc Quantidade de argumentos passados na linha de comando.
 * @param[in]   argv Vetor de argumentos (argv[1] opcional: número de testes estatísticos).
 *
 * @return      int 0 se todos os testes foram gerados e gravados com sucesso;
 *                  1 se ocorreu falha na escrita de algum arquivo.
 *
 * @note        Restrições de Buffer: Utiliza buffers estáticos no BSS para evitar estouro de pilha.
 *              Sobrescrita: Sobrescreve todos os arquivos de entrada em 'input_matrix/'.
 *              Variáveis Globais Afetadas: Nenhuma.
 *              Classificação: Código próprio.
 */
int main(int argc, char *argv[]) {
    int total_testes = QUANTIDADE_TESTES_ESTATISTICOS;

    if (argc >= 2) {
        total_testes = atoi(argv[1]);
        if (total_testes <= 0) {
            fprintf(stderr, "[AVISO] Numero de testes invalido (%s). Usando padrao: %d testes.\n",
                    argv[1], QUANTIDADE_TESTES_ESTATISTICOS);
            total_testes = QUANTIDADE_TESTES_ESTATISTICOS;
        }
    }

    garantir_diretorio("input_matrix");

    /* Buffers estáticos para alocação no segmento BSS (evita estouro de stack) */
    static float matriz_a[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N];
    static float matriz_b[DIMENSAO_MATRIZ_N][DIMENSAO_MATRIZ_N];
    char caminho_completo_a[TAMANHO_BUFFER_CAMINHO];
    char caminho_completo_b[TAMANHO_BUFFER_CAMINHO];
    char nome_arquivo_a[TAMANHO_BUFFER_NOME_ARQUIVO];
    char nome_arquivo_b[TAMANHO_BUFFER_NOME_ARQUIVO];
    int sucessos_geracao = 0;

    /* 1. Geração dos pares estatísticos aleatórios bipolares */
    for (int t = 1; t <= total_testes; t++) {
        unsigned int semente_a = SEMENTE_BASE_MATRIZ_A + t;
        unsigned int semente_b = SEMENTE_BASE_MATRIZ_B + t;

        gerar_matriz_aleatoria(matriz_a, semente_a);
        gerar_matriz_aleatoria(matriz_b, semente_b);

        snprintf(nome_arquivo_a, sizeof(nome_arquivo_a), "A_%d.bin", t);
        snprintf(nome_arquivo_b, sizeof(nome_arquivo_b), "B_%d.bin", t);

        obter_caminho_arquivo(caminho_completo_a, sizeof(caminho_completo_a), "input_matrix", nome_arquivo_a);
        obter_caminho_arquivo(caminho_completo_b, sizeof(caminho_completo_b), "input_matrix", nome_arquivo_b);

        int status_a = salvar_matriz_bin(caminho_completo_a, matriz_a);
        int status_b = salvar_matriz_bin(caminho_completo_b, matriz_b);

        if (status_a && status_b) {
            sucessos_geracao++;
        }
    }

    /* 2. Teste Unitário 1: Matriz Identidade (A_identidade.bin, B_identidade.bin) */
    gerar_matriz_aleatoria(matriz_a, SEMENTE_BASE_MATRIZ_A + 1);
    gerar_matriz_identidade(matriz_b);
    obter_caminho_arquivo(caminho_completo_a, sizeof(caminho_completo_a), "input_matrix", "A_identidade.bin");
    obter_caminho_arquivo(caminho_completo_b, sizeof(caminho_completo_b), "input_matrix", "B_identidade.bin");
    salvar_matriz_bin(caminho_completo_a, matriz_a);
    salvar_matriz_bin(caminho_completo_b, matriz_b);

    /* 3. Teste Unitário 2: Matriz Nula / Zeros (A_zeros.bin, B_zeros.bin) */
    gerar_matriz_aleatoria(matriz_a, SEMENTE_BASE_MATRIZ_A + 1);
    gerar_matriz_zeros(matriz_b);
    obter_caminho_arquivo(caminho_completo_a, sizeof(caminho_completo_a), "input_matrix", "A_zeros.bin");
    obter_caminho_arquivo(caminho_completo_b, sizeof(caminho_completo_b), "input_matrix", "B_zeros.bin");
    salvar_matriz_bin(caminho_completo_a, matriz_a);
    salvar_matriz_bin(caminho_completo_b, matriz_b);

    /* 4. Teste Unitário 3: Alinhamento Forçado de Expoentes (A_mforcada.bin, B_mforcada.bin) */
    gerar_matrizes_alinhamento_forcado(matriz_a, matriz_b);
    obter_caminho_arquivo(caminho_completo_a, sizeof(caminho_completo_a), "input_matrix", "A_mforcada.bin");
    obter_caminho_arquivo(caminho_completo_b, sizeof(caminho_completo_b), "input_matrix", "B_mforcada.bin");
    salvar_matriz_bin(caminho_completo_a, matriz_a);
    salvar_matriz_bin(caminho_completo_b, matriz_b);

    printf("matrix_gen: %d testes estatisticos e %d testes unitarios gravados em 'input_matrix/'.\n",
           sucessos_geracao, QUANTIDADE_TESTES_UNITARIOS);

    return (sucessos_geracao == total_testes) ? 0 : 1;
}
