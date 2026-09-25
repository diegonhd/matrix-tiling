import numpy as np
from pathlib import Path

N = 45
NUM_TESTES = 10

todos_corretos = True

BASE_DIR = Path(__file__).resolve().parent

PASTA_OUTPUT = BASE_DIR / "output"

for teste in range(1, NUM_TESTES + 1):

    nomeA = PASTA_OUTPUT / f"teste_{teste}_A.txt"
    nomeB = PASTA_OUTPUT / f"teste_{teste}_B.txt"
    nomeC = PASTA_OUTPUT / f"teste_{teste}_C.txt"

    # Lê as matrizes geradas pelo C
    A = np.loadtxt(nomeA, dtype=np.float32)
    B = np.loadtxt(nomeB, dtype=np.float32)
    C_c = np.loadtxt(nomeC, dtype=np.float32)

    # Multiplicação usando NumPy
    C_numpy = A @ B

    # Diferença absoluta
    erro = np.abs(C_c - C_numpy)

    maior_erro = np.max(erro)

    # Verifica tolerância
    correto = np.allclose(
        C_c,
        C_numpy,
        rtol=1e-4,
        atol=1e-4
    )

    if correto:
        print(
            f"Teste {teste:02d}: CORRETO | "
            f"maior erro = {maior_erro:.8f}"
        )
    else:
        print(
            f"Teste {teste:02d}: ERRO | "
            f"maior erro = {maior_erro:.8f}"
        )
        todos_corretos = False


print("\n==============================")

if todos_corretos:
    print("RESULTADO FINAL: TODOS OS TESTES CORRETOS")
else:
    print("RESULTADO FINAL: EXISTEM TESTES COM ERRO")

print("==============================")



def mostrar_matriz(nome, M, tamanho=5):
    print(f"\n{nome} (primeiras {tamanho}x{tamanho} posições):")
    print(M[:tamanho, :tamanho])


mostrar_matriz("Matriz A", A)
mostrar_matriz("Matriz B", B)
mostrar_matriz("C - Blocking", C_c)
mostrar_matriz("C - NumPy", C_numpy)
mostrar_matriz("Diferença absoluta", erro)