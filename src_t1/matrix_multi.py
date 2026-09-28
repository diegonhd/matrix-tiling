#!/usr/bin/env python3
import sys
from pathlib import Path
import numpy as np

N = 45
DIR_SRC = Path(__file__).resolve().parent
DIR_BIN_C = DIR_SRC / "bin_c"
DIR_BIN_PY = DIR_SRC / "bin_python"


def processar_matriz(quiet: bool = False):
    """
    Carrega bin_c/A.bin e bin_c/B.bin, computa C = A @ B com NumPy
    e grava a matriz de referência em bin_python/C.bin.
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

    # 1. Carrega as entradas geradas pelo C
    A = np.fromfile(caminho_a, dtype=np.float64).reshape((N, N))
    B = np.fromfile(caminho_b, dtype=np.float64).reshape((N, N))

    # 2. Computa a multiplicação de referência via NumPy
    C = A @ B

    # 3. Exporta a matriz resultante em formato binário bruto (.bin)
    C.tofile(caminho_c)

    if not quiet:
        print(f"NumPy: Matriz calculada e salva em '{caminho_c.name}' ({C.nbytes} bytes).")

    return C


def main():
    quiet_mode = "--quiet" in sys.argv
    processar_matriz(quiet=quiet_mode)


if __name__ == "__main__":
    main()