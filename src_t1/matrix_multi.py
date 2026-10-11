#!/usr/bin/env python3
"""
Motor de multiplicação de matrizes de referência via NumPy (IEEE 754 float32).
Executa as multiplicações com dimensão estritamente fixa N = 45 (45x45 = 2025 elementos).
"""
import sys
import argparse
from pathlib import Path
import numpy as np

DIR_SRC = Path(__file__).resolve().parent
DIR_INPUT = DIR_SRC / "input_matrix"
DIR_BIN_PY = DIR_SRC / "bin_py"
N_MATRIZ = 45
TAMANHO_BYTES_ESPERADO = N_MATRIZ * N_MATRIZ * 4  # 8.100 bytes por matriz float32


def carregar_matriz(caminho: Path) -> np.ndarray:
    """
    Carrega arquivo binário bruto no formato IEEE 754 float32 e retorna array 2D (45x45).
    """
    if not caminho.exists():
        raise FileNotFoundError(f"Arquivo não encontrado: {caminho}")

    tamanho_bytes = caminho.stat().st_size
    if tamanho_bytes != TAMANHO_BYTES_ESPERADO:
        raise ValueError(
            f"Tamanho do arquivo {caminho.name} ({tamanho_bytes} bytes) "
            f"não corresponde a uma matriz fixa {N_MATRIZ}x{N_MATRIZ} float32 ({TAMANHO_BYTES_ESPERADO} bytes)."
        )

    return np.fromfile(caminho, dtype=np.float32).reshape((N_MATRIZ, N_MATRIZ))


def processar_par_nomeado(nome_a: str, nome_b: str, nome_c: str, quiet: bool = False) -> np.ndarray:
    """
    Processa a multiplicação NumPy para um par de arquivos com nomes customizados (N=45).
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)

    caminho_a = DIR_INPUT / nome_a
    caminho_b = DIR_INPUT / nome_b
    caminho_c = DIR_BIN_PY / nome_c

    if not (caminho_a.exists() and caminho_b.exists()):
        raise FileNotFoundError(
            f"Arquivos '{nome_a}' ou '{nome_b}' não encontrados em '{DIR_INPUT}'."
        )

    A = carregar_matriz(caminho_a)
    B = carregar_matriz(caminho_b)
    C = (A @ B).astype(np.float32)
    C.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: {nome_c} gerado em '{DIR_BIN_PY.name}/'.")

    return C


def processar_teste(teste_id: int, quiet: bool = False) -> np.ndarray:
    """
    Carrega A_i.bin e B_i.bin de input_matrix/, computa C = A @ B via NumPy (float32, N=45)
    e grava a matriz resultante de referência em bin_py/C_i.bin.
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
            f"Matrizes de entrada para o teste {teste_id} não encontradas em '{DIR_INPUT}'. "
            f"Execute primeiro 'matrix_gen' para gerar os dados de teste."
        )

    A = carregar_matriz(caminho_a)
    B = carregar_matriz(caminho_b)

    C = (A @ B).astype(np.float32)
    C.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: C_{teste_id}.bin gerado em '{DIR_BIN_PY.name}/'.")

    return C


def processar_todos(total_testes: int = 30, quiet: bool = False) -> int:
    """
    Processa todos os testes disponíveis e salva todas as matrizes de referência em bin_py/ (N=45).
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)

    sucessos = 0
    for t in range(1, total_testes + 1):
        try:
            processar_teste(t, quiet=True)
            sucessos += 1
        except Exception as e:
            print(f"Erro no teste estatístico {t:02d}: {e}", file=sys.stderr)

    # Processa os 3 testes unitários
    try:
        processar_par_nomeado("A_identidade.bin", "B_identidade.bin", "C_identidade.bin", quiet=True)
        processar_par_nomeado("A_zeros.bin", "B_zeros.bin", "C_zeros.bin", quiet=True)
        processar_par_nomeado("A_mforcada.bin", "B_mforcada.bin", "C_mforcada.bin", quiet=True)
    except Exception as e:
        print(f"Erro nos testes unitários: {e}", file=sys.stderr)

    if not quiet:
        print(f"NumPy: {sucessos} matrizes calculadas e salvas em '{DIR_BIN_PY.name}/'.")

    return sucessos


def main():
    parser = argparse.ArgumentParser(
        description="Motor de Multiplicação de Matrizes NumPy (float32, dimensão fixa N=45)"
    )
    parser.add_argument("--test", type=str, default=None, help="Executar teste específico por ID ou nome")
    parser.add_argument("--total", type=int, default=30, help="Total de testes a processar (padrão: 30)")
    parser.add_argument("--quiet", action="store_true", help="Modo silencioso")

    args = parser.parse_args()

    if args.test is not None:
        if args.test.isdigit():
            processar_teste(int(args.test), quiet=args.quiet)
        else:
            nome = args.test.replace(".bin", "")
            processar_par_nomeado(f"A_{nome}.bin", f"B_{nome}.bin", f"C_{nome}.bin", quiet=args.quiet)
    else:
        processar_todos(total_testes=args.total, quiet=args.quiet)


if __name__ == "__main__":
    main()