# 🚀 Multiplicação de Matrizes com Tiling (Otimização de Cache) e Validação Estatística

Este repositório implementa, otimiza e audita o algoritmo de multiplicação de matrizes quadradas ($45 \times 45$) com particionamento em blocos (**Tiling / Blocking**, $B = 8$) em linguagem **C**, comparando os resultados contra o gabarito vetorizado do **NumPy** em Python sob o padrão **IEEE 754** de precisão simples (`float` / 32 bits).

A arquitetura do projeto é **estritamente modular e desacoplada**: a geração dos dados de entrada, os motores de computação (C e Python) e o framework de auditoria são completamente independentes e comunicam-se exclusivamente por meio de arquivos binários brutos (*raw*) padronizados em disco.

---

## 📁 Estrutura do Repositório

```text
matrix-tiling/
├── .venv/                              # Ambiente virtual Python (numpy, matplotlib)
├── requirements.txt                    # Dependências do projeto
├── README.md                           # Documentação central do repositório e guia de apresentação
│
├── src_t1/                             # Tarefa 1: Tiling sem buffer, Validação e Auditoria
│   ├── matrix_gen.c                    # Gerador de matrizes: produz os 30 testes estatísticos + 3 unitários em input_matrix/
│   ├── matrix_multi.c                  # Motor C: computação mult_tiling (B=8), salva C em bin_c/ (documentado Doxygen)
│   ├── matrix_multi.py                 # Motor Python: computação NumPy (float32), salva C em bin_py/
│   ├── framework_testes.ipynb          # Caderno prático de validação metrológica e auditoria independente dos 33 testes
│   ├── fundamentacao_teorica.ipynb     # Caderno teórico: representação IEEE 754, limite de Higham, ULP e análise de erro
│   │
│   ├── input_matrix/                   # Matrizes binárias de entrada (float32 raw, 8.100 bytes cada)
│   │   ├── A_1.bin ... A_30.bin, B_1.bin ... B_30.bin
│   │   ├── A_identidade.bin, B_identidade.bin
│   │   ├── A_zeros.bin, B_zeros.bin
│   │   └── A_mforcada.bin, B_mforcada.bin
│   │
│   ├── bin_c/                          # Matrizes resultantes C calculadas pelo motor em C (float32)
│   │   ├── C_1.bin ... C_30.bin
│   │   └── C_identidade.bin, C_zeros.bin, C_mforcada.bin
│   │
│   ├── bin_py/                         # Matrizes de referência C calculadas pelo NumPy (float32)
│   │   ├── C_1.bin ... C_30.bin
│   │   └── C_identidade.bin, C_zeros.bin, C_mforcada.bin
│   │
│   └── audit/                          # Registros consolidados da auditoria metrológica
│       ├── auditoria_testes.csv        # Log em CSV com hashes SHA-256, seeds e métricas de erro
│       └── auditoria_testes.json       # Log estruturado em JSON para automação
│
└── src_t2/                             # Tarefa 2: Tiling com buffers locais de bloco
    └── matrix_tiling.c                 # Implementação de mult_tiling_buffers (otimizada para SRAM de microcontroladores)
```

---

## 🎓 Roteiro para Apresentação / Arguição Oral (5 Minutos)

Se você for apresentar o projeto ou responder a questionamentos em banca, siga esta sequência objetiva:

### 1. O que é Tiling (Blocking) e qual o seu benefício prático?
> *"Na multiplicação ingênua $O(N^3)$, a CPU percorre linhas inteiras de $A$ e colunas inteiras de $B$. Para matrizes grandes, esse padrão estoura as linhas de Cache L1/L2, forçando o processador a buscar dados repetidamente na RAM (*cache misses* frequentes).  
> Com o **Tiling**, particionamos a matriz em sub-blocos menores ($8 \times 8$). Os 6 laços aninhados (3 externos para navegar nos blocos e 3 internos para multiplicar os elementos) garantem que os dados de cada bloco permaneçam na Cache rápida durante toda a sua computação, maximizando a localidade espacial e temporal."*

### 2. Por que o tipo de dado adotado é estritamente `float` (32 bits)?
> *"Na fase T2, este algoritmo foi planejado para rodar no microcontrolador **STM32F030F4** (ARM Cortex-M0, 16 KB Flash, 4 KB RAM, **sem FPU em hardware**).  
> Se usássemos `double` (64 bits), cada operação aritmética seria emulada via rotinas de software, inflacionando o consumo de ciclos de clock, o tamanho do binário em Flash e dobrando os buffers de bloco em RAM (de 768 B para 1536 B).  
> O tipo `float` (4 bytes por elemento, 23 bits de mantissa) atende perfeitamente à aplicação com consumo enxuto de recursos: as matrizes de $45 \times 45$ ocupam exatamente $8.100\text{ bytes}$ cada."*

### 3. Por que o erro em `float` é na ordem de $10^{-4}$ e comprova a exatidão?
> *"Os elementos da matriz resultante acumulam 45 multiplicações de fatores bipolares $[-10{,}0, +9{,}9]$, atingindo magnitudes médias de $\sim 1125$ ($E = 10$).  
> Em precisão simples (`float32`), a resolução do bit menos significativo nessa magnitude (**1 ULP**) vale:  
> $$1\text{ ULP} = 2^{10 - 23} = 2^{-13} \approx 1{,}22 \times 10^{-4}$$  
> O maior erro absoluto observado entre o C e o NumPy foi de apenas $3{,}05 \times 10^{-4}$, o que corresponde a meros **2 a 5 ULPs** de desvio (ruído natural de arredondamento e reordenação de somas). Dos 24 bits da mantissa, **21 a 23 bits foram rigorosamente idênticos**."*

### 4. Como foi estruturado o pipeline e a auditoria independente?
> *"Separamos o pipeline em componentes independentes:  
> 1. `matrix_gen.c` gera deterministicamente 30 pares de matrizes $A$ e $B$ e 3 casos unitários em `input_matrix/`.  
> 2. `matrix_multi.c` lê de `input_matrix/` e processa todas as multiplicações em C via Tiling, salvando em `bin_c/`.  
> 3. `matrix_multi.py` lê de `input_matrix/` e gera as referências NumPy em `bin_py/`.  
> 4. O notebook `framework_testes.ipynb` atua como um auditor neutro: consome exclusivamente os binários em disco, calcula hashes SHA-256, avalia os ULPs e a Norma de Frobenius, e valida os testes unitários de Álgebra Linear ($A \times I = A$ e $A \times 0 = 0$) com erro zero absoluto."*

---

## ⚡ Guia Rápido de Compilação e Execução

Todos os comandos podem ser executados a partir do diretório raiz do repositório:

### 1. Compilar e Gerar as Matrizes de Entrada (30 Testes + 3 Unitários)
```powershell
gcc -O2 -Wall -Wextra src_t1/matrix_gen.c -o matrix_gen
./matrix_gen
```
*Gera os 60 arquivos `src_t1/input_matrix/A_1.bin`..`A_30.bin`, `B_1.bin`..`B_30.bin` e os pares unitários (`identidade`, `zeros`, `mforcada`) de 8.100 bytes cada.*

### 2. Compilar e Executar o Motor em C (Tiling)
```powershell
gcc -O3 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi
./matrix_multi
```
*Lê os dados de `input_matrix/`, computa a multiplicação com Tiling ($B=8$) e salva os binários resultantes em `src_t1/bin_c/`.*

#### Execução Pontual (Opcional):
O executável compilado também aceita parâmetros para execução de testes individuais:
```powershell
./matrix_multi 1            # Executa apenas o teste 1 -> gera bin_c/C_1.bin
./matrix_multi identidade  # Executa o teste de identidade -> gera bin_c/C_identidade.bin
```

### 3. Executar o Motor de Referência em Python (NumPy)
```powershell
.\.venv\Scripts\python.exe src_t1/matrix_multi.py
# Ou no Linux / macOS:
# python3 src_t1/matrix_multi.py
```
*Lê os dados de `input_matrix/`, calcula o produto via NumPy em precisão `float32` e grava os gabaritos em `src_t1/bin_py/`.*

### 4. Cadernos Jupyter Interativos (Auditoria e Teoria)

- **[`src_t1/framework_testes.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/framework_testes.ipynb)**:
  - Framework de auditoria metrológica desacoplada.
  - Consome diretamente os arquivos em `input_matrix/`, `bin_c/` e `bin_py/` sem invocar subprocessos de compilação.
  - Gera os arquivos de auditoria `src_t1/audit/auditoria_testes.csv` e `.json` com hashes **SHA-256**.
  - Valida todos os 30 testes estatísticos sob a tolerância $\text{atol} = 1{,}0 \times 10^{-3}$ e avalia métricas de Frobenius e ULPs.
  - Executa a verificação dos 3 testes unitários de controle (Identidade, Matriz Nula e Alinhamento Forçado de Expoentes).

- **[`src_t1/fundamentacao_teorica.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/fundamentacao_teorica.ipynb)**:
  - Fundamentação matemática, arquitetural e metrológica completa.
  - Análise em nível de bit do padrão IEEE 754 de precisão simples (1 bit de sinal, 8 bits de expoente, 23 bits de fração).
  - Cálculo formal de 1 ULP ($1\text{ ULP} = 2^{E-23} \approx 1{,}22 \times 10^{-4}$ para magnitude $\sim 1125$).
  - Dedução do **Limite Teórico de Erro de Higham** (limite determinístico $\gamma_N$ versus limite heurístico/estatístico $\tilde{\gamma}_N$).
  - Análise da não-associatividade aritmética de hardware gerada pela reordenação das somas no Tiling.
  - Estudo de absorção numérica e cancelamento catastrófico por desalinhamento extremo de expoentes ($\Delta E = 24\text{ bits}$).
