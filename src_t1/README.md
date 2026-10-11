# 🔬 Tarefa 1 (T1): Multiplicação de Matrizes com Tiling, Validação Estatística e Auditoria Metrológica

> **Diretório:** `src_t1/`  
> **Ambiente Alvo:** Host x86_64 (Linux / Windows)  
> **Linguagens:** C (C99/C11) e Python (NumPy, Matplotlib)

---

## 1. O que é pedido na Tarefa 1 (T1)

### 1.1 Objetivo Geral
Desenvolver, otimizar e auditar o algoritmo clássico de multiplicação de matrizes quadradas ($N = 45$) utilizando a técnica de particionamento em blocos (**Tiling / Blocking** com tamanho de bloco $B = 8$) sob o padrão de ponto flutuante **IEEE 754 de precisão simples** (`float32`).

### 1.2 Atividades Obrigatórias
1. **Geração Determinística de Dados:** Gerar matrizes de entrada $A$ e $B$ com dimensões $45 \times 45$ (2025 elementos / 8.100 bytes cada) para:
   - 30 pares de testes estatísticos com distribuição uniforme bipolar $[-10{,}0, +9{,}9]$.
   - 3 casos unitários/limítrofes de Álgebra Linear: **Identidade** ($A \times I = A$), **Zeros** ($A \times 0 = 0$) e **Matriz Forçada** (estresse numérico com valores extremos).
2. **Motor de Computação em C:** Implementar o algoritmo de Tiling com 6 laços aninhados (3 externos para as coordenadas dos blocos e 3 internos para a multiplicação), salvando a matriz resultante $C$ em formato binário bruto (*raw*, sem cabeçalhos).
3. **Motor de Computação em Python (NumPy):** Processar exatamente os mesmos arquivos de entrada utilizando o gabarito vetorizado do NumPy em `float32` para servir de referência matemática absoluta.
4. **Framework de Auditoria e Validação Metrológica:** Construir um auditor independente (Jupyter Notebook) que consome exclusivamente os arquivos gerados em disco, calculando:
   - Maior erro absoluto ($L_\infty$).
   - Desvio em Unidades no Último Lugar (**ULPs**).
   - Distância por Norma de Frobenius ($\|C_C - C_{py}\|_F$).
   - Verificação de hashes criptográficos **SHA-256** para garantia de integridade e não-viciação dos dados.

---

## 2. Estrutura de Arquivos de `src_t1/`

```text
src_t1/
├── matrix_gen.c                # Gerador C: produz os 30 pares estatísticos + 3 unitários em input_matrix/
├── matrix_multi.c              # Motor de cálculo em C com Tiling (B=8), salvando em bin_c/
├── matrix_multi.py             # Motor de referência em Python (NumPy float32), salvando em bin_py/
│
├── framework_testes.ipynb      # Caderno prático de validação, cálculo de ULPs, Frobenius e aprovação (100%)
├── fundamentacao_teorica.ipynb # Caderno teórico: padrão IEEE 754, limite de Higham e propagação de erro
│
├── input_matrix/               # Matrizes binárias de entrada (8.100 bytes cada, float32 raw)
│   ├── A_1.bin ... A_30.bin, B_1.bin ... B_30.bin
│   ├── A_identidade.bin, B_identidade.bin
│   ├── A_zeros.bin, B_zeros.bin
│   └── A_mforcada.bin, B_mforcada.bin
│
├── bin_c/                      # Matrizes C resultantes calculadas pelo executável em C
│   ├── C_1.bin ... C_30.bin
│   └── C_identidade.bin, C_zeros.bin, C_mforcada.bin
│
├── bin_py/                     # Matrizes C de referência geradas pelo script em Python
│   ├── C_1.bin ... C_30.bin
│   └── C_identidade.bin, C_zeros.bin, C_mforcada.bin
│
└── audit/                      # Logs formais da auditoria metrológica
    ├── auditoria_testes.csv    # Registro tabular com seeds, hashes SHA-256 e erros
    └── auditoria_testes.json   # Registro estruturado em JSON para automação
```

---

## 3. Como Executar a Pipeline da T1

A partir da raiz do repositório:

### Passo 1: Compilar e Gerar as Entradas
```powershell
gcc -O2 -Wall -Wextra src_t1/matrix_gen.c -o matrix_gen
./matrix_gen
```

### Passo 2: Executar o Motor em C (Tiling)
```powershell
gcc -O3 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi
./matrix_multi
```

### Passo 3: Executar o Motor em Python (NumPy)
```powershell
python src_t1/matrix_multi.py
```

### Passo 4: Executar a Auditoria Completa
Abra e execute o notebook:
```powershell
jupyter notebook src_t1/framework_testes.ipynb
```
*Gera os gráficos de controle de processo, histogramas de erro em ULPs e o laudo de conformidade de 100% dos testes aprovados.*
