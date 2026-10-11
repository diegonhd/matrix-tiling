#!/usr/bin/env python3
"""
Gera test_data.h com as matrizes A e B (N=45) a partir dos binários em input_matrix/.

Uso:
    python generate_test_data.py [teste_id]

Exemplos:
    python generate_test_data.py               # Usa teste 1 por padrão
    python generate_test_data.py 1             # Teste estatístico 1
    python generate_test_data.py identidade    # Teste unitário de identidade
"""
import sys
from pathlib import Path
import numpy as np

N = 45
DIR_INPUT = Path(__file__).resolve().parent / "input_matrix"
if not DIR_INPUT.exists():
    DIR_INPUT = Path(__file__).resolve().parent.parent / "src_t1" / "input_matrix"


def format_float(v: float) -> str:
    s = f"{float(v):.9g}"
    if "." not in s and "e" not in s and "E" not in s:
        s += ".0"
    return f"{s}f"


def dump(nome: str, M: np.ndarray) -> str:
    linhas = [f"static const float {nome}[N][N] = {{"]
    for row in M:
        linhas.append("    {" + ", ".join(format_float(v) for v in row) + "},")
    linhas.append("};")
    return "\n".join(linhas)


def main():
    teste = sys.argv[1] if len(sys.argv) > 1 else "1"

    caminho_a = DIR_INPUT / f"A_{teste}.bin"
    caminho_b = DIR_INPUT / f"B_{teste}.bin"

    if not caminho_a.exists() or not caminho_b.exists():
        print(f"[ERRO] Não encontrei A_{teste}.bin ou B_{teste}.bin em '{DIR_INPUT}'.")
        sys.exit(1)

    A = np.fromfile(caminho_a, dtype=np.float32).reshape((N, N))
    B = np.fromfile(caminho_b, dtype=np.float32).reshape((N, N))
    C_esperado = np.matmul(A, B, dtype=np.float32)
    checksum_esperado = float(np.sum(C_esperado, dtype=np.float32))

    saida = Path(__file__).resolve().parent / "test_data.h"
    conteudo = (
        "#ifndef TEST_DATA_H\n"
        "#define TEST_DATA_H\n\n"
        f"#define N {N}\n\n"
        f"/* Checksum esperado (Soma de todos os elementos de C em float32) */\n"
        f"#define CHECKSUM_SUM_EXPECTED {format_float(checksum_esperado)}\n\n"
        f"/* Gerado a partir de A_{teste}.bin e B_{teste}.bin (N={N}) */\n\n"
        f"{dump('A', A)}\n\n"
        f"{dump('B', B)}\n\n"
        "#endif /* TEST_DATA_H */\n"
    )

    saida.write_text(conteudo, encoding="utf-8")
    print(f"OK: test_data.h gerado com o teste '{teste}' (N={N}, Checksum={checksum_esperado:.6f}).")


if __name__ == "__main__":
    main()
