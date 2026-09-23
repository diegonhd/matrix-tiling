# Projeto de multiplicação de matrizes com tiling

Implementação em C de multiplicação de matrizes para a validação no PC de um
trabalho acadêmico de sistemas embarcados. O projeto compara uma versão ingênua
com uma versão usando blocking/tiling.

## Hierarquia

```text
projeto-tiling/
├── src/
│   ├── matriz_mult.h
│   ├── matriz_mult.c
│   └── main.c
├── testes/
│   ├── gerar_dados.py
│   ├── comparar.py
│   └── casos_especiais/
├── docs/
│   ├── decisoes_projeto.md
│   └── fontes.md
└── README.md
```

`N=45` e `BLOCK=8` são definidos no header. Não há recursão nem alocação
dinâmica; as matrizes grandes locais são declaradas como `static`.

## Requisitos

- GCC com suporte a C99;
- Python 3 com NumPy.

## Compilação

A partir da raiz do repositório:

```bash
gcc -std=c99 -Wall -Wextra -O0 -Isrc src/main.c src/matriz_mult.c -o matriz_tiling
```

No Windows, o executável pode ser chamado como `matriz_tiling.exe`.

## Execução e validação

1. Gere as entradas e o gabarito:

   ```bash
   python testes/gerar_dados.py
   ```

2. Execute o programa na raiz, onde estão `A.txt` e `B.txt`:

   ```bash
   ./matriz_tiling
   ```

   Ele imprime os tempos de `mult_ingenua()` e `mult_tiling()`, compara os dois
   resultados e cria `C_saida.txt`.

3. Compare com NumPy:

   ```bash
   python testes/comparar.py
   ```

## Casos especiais

Para gerar identidade, uma matriz pequena conhecida e uma matriz de dimensão
10 (não múltipla de 8), execute:

```bash
python testes/casos_especiais/gerar_caso.py
```

Esses arquivos são material auxiliar para testes parametrizados; o executável
principal segue fixo em `N=45`, conforme o enunciado.