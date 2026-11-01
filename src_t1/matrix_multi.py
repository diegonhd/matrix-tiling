#!/usr/bin/env python3
"""
===============================================================================
matrix_multi.py
===============================================================================
Apresentação Geral:
    Motor de referência em Python para cálculo do produto matricial via NumPy
    sob o padrão de ponto flutuante IEEE 754 de precisão simples (float32).
    Gera as matrizes de referência ('ground truth') utilizadas pelo auditor metrológico
    para cálculo de desvios em ULPs e erros absolutos em relação
    ao motor de Tiling em C.

Autores / Estudantes:
    - Diego Amorim Vidal
    - André Queiroz Medeiros

Data de Revisão / Entrega:
    31 de Outubro de 2026

Plataforma Alvo:
    Host x86_64 (Python 3.10+ com biblioteca NumPy).

Permissões de Uso / Copyright:
    Uso estritamente didático e acadêmico no âmbito da avaliação da disciplina.

Como Usar (Help):
    1. Processamento em lote (padrão - processa 30 testes + 3 unitários):
       python src_t1/matrix_multi.py

    2. Processamento pontual por índice:
       python src_t1/matrix_multi.py --test 1

    3. Processamento de teste unitário limítrofe:
       python src_t1/matrix_multi.py --test identidade
       python src_t1/matrix_multi.py --test zeros
       python src_t1/matrix_multi.py --test mforcada

Entrada e Saída:
    - Formato de Arquivo: Binário bruto (raw IEEE 754 float32, little-endian, row-major).
    - Dimensões: N x N = 45 x 45 elementos = 2025 floats (exatamente 8.100 bytes por arquivo).
    - Entradas: "src_t1/input_matrix/A_<id>.bin" e "src_t1/input_matrix/B_<id>.bin".
    - Saídas:   "src_t1/bin_py/C_<id>.bin".

Classificação de Autoria:
    - Código Próprio: Gerenciamento de caminhos, validação de integridade de arquivos,
                      orquestração em lote via argparse e automação de testes.
    - Código Referência / Biblioteca: Operação matricial vetorizada via NumPy (@ operator).
    - Código Gerado: Não aplicável.
===============================================================================
"""

import sys
import argparse
from pathlib import Path
import numpy as np

# =============================================================================
# CONSTANTES NOMINATIVAS DO SISTEMA
# =============================================================================

## @var DIR_SRC
#  Caminho absoluto do diretório contendo o script (src_t1).
DIR_SRC: Path = Path(__file__).resolve().parent

## @var DIR_INPUT
#  Caminho do diretório contendo as matrizes binárias de entrada.
DIR_INPUT: Path = DIR_SRC / "input_matrix"

## @var DIR_BIN_PY
#  Caminho do subdiretório de saída para gravação das matrizes C de referência.
DIR_BIN_PY: Path = DIR_SRC / "bin_py"

## @var DIMENSAO_MATRIZ_N
#  Dimensão estritamente fixa das matrizes quadradas (N = 45).
DIMENSAO_MATRIZ_N: int = 45

## @var TOTAL_ELEMENTOS_MATRIZ
#  Quantidade total de elementos float32 contidos em cada matriz (2025).
TOTAL_ELEMENTOS_MATRIZ: int = DIMENSAO_MATRIZ_N * DIMENSAO_MATRIZ_N

## @var TAMANHO_BYTES_ESPERADO
#  Tamanho exato esperado do arquivo binário em disco (45 * 45 * 4 = 8.100 bytes).
TAMANHO_BYTES_ESPERADO: int = TOTAL_ELEMENTOS_MATRIZ * 4

## @var QUANTIDADE_TESTES_PADRAO
#  Quantidade padrão de testes estatísticos processados em lote.
QUANTIDADE_TESTES_PADRAO: int = 30


# =============================================================================
# FUNÇÕES DE MANIPULAÇÃO E CÁLCULO
# =============================================================================

def carregar_matriz(caminho_arquivo: Path) -> np.ndarray:
    """
    @brief  Carrega matriz binária bruta IEEE 754 float32 a partir do disco.

    @details Abre o arquivo binário em modo leitura, valida se o tamanho em bytes
             corresponde estritamente à matriz 45x45 float32 (8.100 bytes) e
             reestrutura os dados em um array bidimensional contíguo.

    @param[in]  caminho_arquivo  Instância Path apontando para o arquivo .bin a ser lido.

    @return     np.ndarray       Array NumPy 2D de dimensão (45, 45) com dtype float32.

    @throws     FileNotFoundError Caso o arquivo não exista no caminho indicado.
    @throws     ValueError        Caso o tamanho do arquivo divirja de 8.100 bytes.

    @note       Restrições de Buffer: O arquivo deve conter exatamente 8.100 bytes.
                Sobrescrita: Nenhuma (leitura estrita).
                Variáveis Globais Afetadas: Nenhuma.
                Classificação: Código próprio.
    """
    if not caminho_arquivo.exists():
        raise FileNotFoundError(f"[ERRO] Arquivo nao encontrado: {caminho_arquivo}")

    tamanho_bytes = caminho_arquivo.stat().st_size
    if tamanho_bytes != TAMANHO_BYTES_ESPERADO:
        raise ValueError(
            f"[ERRO] Tamanho do arquivo {caminho_arquivo.name} ({tamanho_bytes} bytes) "
            f"diverge do esperado ({TAMANHO_BYTES_ESPERADO} bytes para {DIMENSAO_MATRIZ_N}x{DIMENSAO_MATRIZ_N} float32)."
        )

    return np.fromfile(caminho_arquivo, dtype=np.float32).reshape((DIMENSAO_MATRIZ_N, DIMENSAO_MATRIZ_N))


def processar_par_nomeado(nome_arquivo_a: str, nome_arquivo_b: str, nome_arquivo_c: str, quiet: bool = False) -> np.ndarray:
    """
    @brief  Processa o produto matricial C = A x B para um par de arquivos com nomes customizados.

    @details Carrega as matrizes A e B de 'DIR_INPUT', computa C = A @ B utilizando precisão
             simples IEEE 754 (float32) e salva a matriz C resultante em formato binário bruto
             em 'DIR_BIN_PY'.
             ATENÇÃO: O arquivo de saída em disco é sobrescrito se já existir.

    @param[in]  nome_arquivo_a Nome do arquivo da matriz multiplicando (ex.: 'A_identidade.bin').
    @param[in]  nome_arquivo_b Nome do arquivo da matriz multiplicador (ex.: 'B_identidade.bin').
    @param[in]  nome_arquivo_c Nome do arquivo de saída em disco (ex.: 'C_identidade.bin').
    @param[in]  quiet          Flag booleana para suprimir mensagens informativas no terminal.

    @return     np.ndarray     Matriz resultante C (dimensão 45x45 em float32).

    @throws     FileNotFoundError Caso os arquivos de entrada não sejam localizados.

    @note       Restrições de Buffer: Matrizes devem ser 45x45 floats contíguos.
                Sobrescrita: O arquivo em 'DIR_BIN_PY / nome_arquivo_c' é COMPLETAMENTE SOBRESCRITO.
                Variáveis Globais Afetadas: Nenhuma.
                Classificação: Código próprio.
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)

    caminho_a = DIR_INPUT / nome_arquivo_a
    caminho_b = DIR_INPUT / nome_arquivo_b
    caminho_c = DIR_BIN_PY / nome_arquivo_c

    if not (caminho_a.exists() and caminho_b.exists()):
        raise FileNotFoundError(
            f"[ERRO] Arquivos '{nome_arquivo_a}' ou '{nome_arquivo_b}' ausentes em '{DIR_INPUT}'."
        )

    matriz_a = carregar_matriz(caminho_a)
    matriz_b = carregar_matriz(caminho_b)
    matriz_c = (matriz_a @ matriz_b).astype(np.float32)
    matriz_c.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: {nome_arquivo_c} gerado com sucesso em '{DIR_BIN_PY.name}/'.")

    return matriz_c


def processar_teste(teste_id: int, quiet: bool = False) -> np.ndarray:
    """
    @brief  Processa o par de teste estatístico correspondente ao ID numérico.

    @details Carrega 'A_<id>.bin' e 'B_<id>.bin' de 'input_matrix/', calcula o produto
             C = A @ B em float32 e persiste o resultado em 'bin_py/C_<id>.bin'.
             ATENÇÃO: O arquivo de saída em disco é sobrescrito se já existir.

    @param[in]  teste_id  Identificador numérico do teste (1 a 30).
    @param[in]  quiet     Flag booleana para supressão de mensagens de progresso.

    @return     np.ndarray Matriz resultante C (45x45 float32).

    @throws     FileNotFoundError Caso os arquivos binários correspondentes não existam.

    @note       Restrições de Buffer: Matrizes 45x45 float32.
                Sobrescrita: O arquivo 'bin_py/C_<id>.bin' é COMPLETAMENTE SOBRESCRITO.
                Variáveis Globais Afetadas: Nenhuma.
                Classificação: Código próprio.
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)

    caminho_a = DIR_INPUT / f"A_{teste_id}.bin"
    if not caminho_a.exists():
        caminho_a = DIR_INPUT / f"A_{teste_id:02d}.bin"

    caminho_b = DIR_INPUT / f"B_{teste_id}.bin"
    if not caminho_b.exists():
        caminho_b = DIR_INPUT / f"B_{teste_id:02d}.bin"

    caminho_c = DIR_BIN_PY / f"C_{teste_id}.bin"

    if not (caminho_a.exists() and caminho_b.exists()):
        raise FileNotFoundError(
            f"[ERRO] Matrizes para o teste {teste_id} nao encontradas em '{DIR_INPUT}'. "
            f"Execute previamente o executavel 'matrix_gen'."
        )

    matriz_a = carregar_matriz(caminho_a)
    matriz_b = carregar_matriz(caminho_b)

    matriz_c = (matriz_a @ matriz_b).astype(np.float32)
    matriz_c.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: C_{teste_id}.bin gerado com sucesso em '{DIR_BIN_PY.name}/'.")

    return matriz_c


def processar_todos(total_testes: int = QUANTIDADE_TESTES_PADRAO, quiet: bool = False) -> int:
    """
    @brief  Processa em lote toda a bateria de testes estatísticos e os casos unitários.

    @details Itera sobre os identificadores de teste de 1 ate 'total_testes', invocando
             'processar_teste' para cada um, e em seguida processa os 3 casos unitários
             limítrofes ('identidade', 'zeros' e 'mforcada').
             ATENÇÃO: Todos os arquivos prévios em 'bin_py/' são sobrescritos.

    @param[in]  total_testes Quantidade de testes estatísticos a executar (padrão: 30).
    @param[in]  quiet        Flag para silenciar mensagens intermediárias de console.

    @return     int          Quantidade de matrizes estatísticas processadas com sucesso.

    @note       Restrições de Buffer: Processamento em lote de arquivos 45x45 floats.
                Sobrescrita: Sobrescreve integralmente todos os arquivos em 'bin_py/'.
                Variáveis Globais Afetadas: Nenhuma.
                Classificação: Código próprio.
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)

    sucessos = 0
    for t in range(1, total_testes + 1):
        try:
            processar_teste(t, quiet=True)
            sucessos += 1
        except Exception as erro:
            print(f"[ERRO] Falha no teste estatistico {t:02d}: {erro}", file=sys.stderr)

    # Processamento dos 3 testes unitários limítrofes
    try:
        processar_par_nomeado("A_identidade.bin", "B_identidade.bin", "C_identidade.bin", quiet=True)
        processar_par_nomeado("A_zeros.bin", "B_zeros.bin", "C_zeros.bin", quiet=True)
        processar_par_nomeado("A_mforcada.bin", "B_mforcada.bin", "C_mforcada.bin", quiet=True)
    except Exception as erro:
        print(f"[ERRO] Falha nos testes unitarios: {erro}", file=sys.stderr)

    if not quiet:
        print(f"NumPy: {sucessos} baterias estatisticas e 3 testes unitarios calculados em '{DIR_BIN_PY.name}/'.")

    return sucessos


def main():
    """
    @brief  Ponto de entrada do script CLI de referência em Python.

    @details Faz o parseamento dos argumentos de linha de comando (--test, --total, --quiet)
             e despacha a execução pontual ou em lote.

    @param[in]  Nenhum (argumentos capturados via sys.argv / CLI argparse).
    @return     None (execução direta de terminal; status retornado via código do processo).

    @note       Restrições de Buffer: Não se aplica.
                Sobrescrita: Sobrescreve os arquivos binários correspondentes em 'bin_py/'.
                Variáveis Globais Afetadas: Nenhuma.
                Classificação: Código próprio.
    """
    parser = argparse.ArgumentParser(
        description="Motor de Multiplicacao de Matrizes NumPy de Referencia (float32, N=45)"
    )
    parser.add_argument("--test", type=str, default=None, help="Executar teste especifico por ID ou nome")
    parser.add_argument("--total", type=int, default=QUANTIDADE_TESTES_PADRAO, help="Total de testes estatisticos (padrao: 30)")
    parser.add_argument("--quiet", action="store_true", help="Modo silencioso")

    args = parser.parse_args()

    if args.test is not None:
        if args.test.isdigit():
            processar_teste(int(args.test), quiet=args.quiet)
        else:
            nome_limpo = args.test.replace(".bin", "")
            processar_par_nomeado(f"A_{nome_limpo}.bin", f"B_{nome_limpo}.bin", f"C_{nome_limpo}.bin", quiet=args.quiet)
    else:
        processar_todos(total_testes=args.total, quiet=args.quiet)


if __name__ == "__main__":
    main()
