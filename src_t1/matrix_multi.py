#!/usr/bin/env python3
import sys
import math
from pathlib import Path
import numpy as np

DIR_SRC = Path(__file__).resolve().parent
DIR_BIN_C = DIR_SRC / "bin_c"
DIR_BIN_PY = DIR_SRC / "bin_python"


def processar_matriz(quiet: bool = False, dim: int = None):
    """
    Carrega bin_c/A.bin e bin_c/B.bin (gerados pelo executável C),
    computa C = A @ B com NumPy em precisão simples (float32)
    e grava a matriz de referência em bin_python/C.bin.

    CONFIRMAÇÃO DE FLUXO E INTEGRIDADE:
    - O Python lê EXCLUSIVAMENTE os arquivos A.bin e B.bin gravados pelo executável C.
    - O Python NÃO gera entradas próprias nem dados pseudo-aleatórios independentes.
    - O cálculo de referência C_numpy = A @ B é realizado sobre exatamente os mesmos dados.
    - Tipo de dado: IEEE 754 float32 (single precision, 4 bytes por elemento).
    """
    DIR_BIN_PY.mkdir(parents=True, exist_ok=True)
    caminho_a = DIR_BIN_C / "A.bin"
    caminho_b = DIR_BIN_C / "B.bin"
    caminho_c = DIR_BIN_PY / "C.bin"

    if not (caminho_a.exists() and caminho_b.exists()):
        raise FileNotFoundError(
            f"Matrizes de entrada não encontradas em '{DIR_BIN_C}'. "
            "Execute primeiro o programa em C ('matrix_multi') para gerar as matrizes."
        )

    # 1. Determina a dimensão N da matriz
    if dim is not None:
        n_matriz = dim
    else:
        tamanho_bytes = caminho_a.stat().st_size
        n_elementos = tamanho_bytes // 4  # 4 bytes por float32
        n_matriz = math.isqrt(n_elementos)
        if n_matriz * n_matriz != n_elementos:
            raise ValueError(
                f"Tamanho do arquivo {caminho_a.name} ({tamanho_bytes} bytes) "
                "não corresponde a uma matriz quadrada float32 válida."
            )

    # 2. Carrega as entradas geradas pelo C (garantindo consumo dos mesmos dados gerados pelo C)
    A = np.fromfile(caminho_a, dtype=np.float32).reshape((n_matriz, n_matriz))
    B = np.fromfile(caminho_b, dtype=np.float32).reshape((n_matriz, n_matriz))

    # 3. Computa a multiplicação de referência via NumPy (float32)
    C = (A @ B).astype(np.float32)

    # 4. Exporta a matriz resultante em formato binário bruto (.bin) float32
    C.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: Matriz ({n_matriz}x{n_matriz}) calculada e salva em '{caminho_c.name}' ({C.nbytes} bytes).")

    return C


def main():
    quiet_mode = "--quiet" in sys.argv
    dim = None
    for i, arg in enumerate(sys.argv):
        if arg == "--dim" and i + 1 < len(sys.argv):
            dim = int(sys.argv[i + 1])
        elif arg.startswith("--dim="):
            dim = int(arg.split("=")[1])
    processar_matriz(quiet=quiet_mode, dim=dim)


if __name__ == "__main__":
    main()