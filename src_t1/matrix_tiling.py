#!/usr/bin/env python3
"""
==============================================================================
PROJETO: Multiplicação de Matrizes com Otimização de Cache (Tiling / Blocking)
ARQUIVO: matrix_tiling.py (Tarefa 1 - src_t1)
OBJETIVO: Carregar as matrizes binárias geradas pelo executável em C,
          executar a multiplicação de referência via NumPy (gabarito) em float64,
          salvar a matriz C resultante em 'bin_python/C.bin' e validar a corretude.
==============================================================================

PONTOS-CHAVE PARA EXPLICAR AO PROFESSOR:
1. Por que NumPy é a referência (Gabarito)?
   O operador '@' do NumPy utiliza rotinas BLAS (Basic Linear Algebra Subprograms)
   altamente consolidadas e vetorizadas na CPU (com instruções SIMD/AVX).

2. Por que a tolerância adotada é 1e-11?
   Em matrizes 45x45 com valores no intervalo [-10.0, +9.9] (simulando sensores bipolares),
   o teorema de Higham define o limite superior do erro absoluto pelo produto das magnitudes
   máximas: E_max <= 45 * (45 * 10 * 10) * 2.22e-16 ≈ 4.5e-11.
   Em precisão dupla (double), o erro de arredondamento típico observado fica em ~1e-13 a 1e-12.
   A tolerância absoluta (atol) de 1e-11 (10^-11) fornece a margem teórica ideal para validar
   que a reordenação de laços do Tiling não corrompe a precisão, mesmo com cancelamento de termos.
==============================================================================
"""

import numpy as np
from pathlib import Path

# Dimensão da matriz (deve coincidir exatamente com o N definido em matrix_multi.c)
N = 45

# Tolerância matemática deduzida para precisão dupla (double / float64)
TOLERANCIA_ATOL = 1e-11
TOLERANCIA_RTOL = 1e-11

# Caminhos locais relativos ao diretório do script (src_t1)
DIR_SRC = Path(__file__).resolve().parent
DIR_BIN_C = DIR_SRC / "bin_c"
DIR_BIN_PY = DIR_SRC / "bin_python"
DIR_BIN_PY.mkdir(parents=True, exist_ok=True)


def main():
    print("=" * 65)
    print("  VALIDAÇÃO NUMÉRICA: C (TILING) vs PYTHON (NUMPY) - DOUBLE")
    print("=" * 65)

    # 1. Carrega as matrizes binárias brutas geradas pelo C
    # Em C, a matriz float64 bidimensional é armazenada contiguamente em 'row-major'
    caminho_A = DIR_BIN_C / "A.bin"
    caminho_B = DIR_BIN_C / "B.bin"
    caminho_C = DIR_BIN_C / "C.bin"

    if not (caminho_A.exists() and caminho_B.exists() and caminho_C.exists()):
        print("[ERRO] Arquivos binários não encontrados em:", DIR_BIN_C)
        print("Compile e execute primeiro o programa em C ('matrix_multi') antes de validar.")
        return

    print("1. Lendo matrizes de entrada e resultado de 'bin_c/'...")
    A = np.fromfile(caminho_A, dtype=np.float64).reshape((N, N))
    B = np.fromfile(caminho_B, dtype=np.float64).reshape((N, N))
    C_c = np.fromfile(caminho_C, dtype=np.float64).reshape((N, N))
    print(f"   Matrizes A, B e C_c carregadas com sucesso ({A.nbytes} bytes cada).")

    # 2. Multiplicação de referência usando NumPy
    print("\n2. Executando multiplicação de referência com NumPy (A @ B)...")
    C_numpy = A @ B

    # 3. Salva a matriz resultante gerada em Python na pasta dedicada
    caminho_saida_py = DIR_BIN_PY / "C.bin"
    C_numpy.tofile(caminho_saida_py)
    print(f"   Matriz resultante salva em: {caminho_saida_py.name} ({C_numpy.nbytes} bytes).")

    # 4. Avaliação das divergências numéricas
    erro_matriz = np.abs(C_c - C_numpy)
    maior_erro = float(np.max(erro_matriz))
    erro_medio = float(np.mean(erro_matriz))
    
    # 1 ULP para números da ordem de ~1100 (expoente 10): 2^(10 - 52) = 2^-42 ≈ 2.27e-13
    ulp_estimada = 2.0 ** (10 - 52)
    desvio_em_ulps = maior_erro / ulp_estimada

    # np.allclose verifica se todos os elementos cumprem: |C_c - C_numpy| <= (atol + rtol * |C_numpy|)
    correto = np.allclose(C_c, C_numpy, rtol=TOLERANCIA_RTOL, atol=TOLERANCIA_ATOL)

    print("\n" + "=" * 65)
    print("  RESULTADO DA VALIDAÇÃO")
    print("=" * 65)
    if correto:
        print("  STATUS: [CORRETO / APROVADO]")
    else:
        print("  STATUS: [ERRO / DIVERGÊNCIA]")

    print(f"  * Tolerância Adotada:           {TOLERANCIA_ATOL:.1e}")
    print(f"  * Maior Erro Absoluto:          {maior_erro:.16e}")
    print(f"  * Erro Médio Absoluto:          {erro_medio:.16e}")
    print(f"  * Equivalência em bits (ULP):   ~{desvio_em_ulps:.2f} ULPs")
    print("=" * 65)

    # 5. Visualização das primeiras posições para inspeção visual
    def mostrar_preview(nome, M, tamanho=5):
        print(f"\n{nome} (primeiras {tamanho}x{tamanho} posições):")
        print(np.round(M[:tamanho, :tamanho], 4))

    mostrar_preview("Matriz A (Entrada C)", A)
    mostrar_preview("Matriz B (Entrada C)", B)
    mostrar_preview("Matriz C - Algoritmo Tiling (C)", C_c)
    mostrar_preview("Matriz C - Referência NumPy (Python)", C_numpy)
    
    print("\nDiferença Absoluta (|C_c - C_numpy|) nas primeiras 5x5 posições:")
    print(erro_matriz[:5, :5])


if __name__ == "__main__":
    main()