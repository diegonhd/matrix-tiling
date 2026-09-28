# 🚀 Multiplicação de Matrizes com Tiling (Otimização de Cache) e Validação Estatística

Este projeto implementa e valida o algoritmo de multiplicação de matrizes quadradas ($45 \times 45$) com particionamento em blocos (**Tiling / Blocking**, $B = 8$) em linguagem **C**, comparando os resultados contra o gabarito vetorizado do **NumPy** em Python sob o padrão IEEE 754 de precisão dupla (`double`).

---

## 📁 Estrutura do Projeto

```text
matrix-tiling/
├── .venv/                              # Ambiente virtual Python configurado (numpy, matplotlib)
├── requirements.txt                    # Dependências exatas do projeto
├── README.md                           # Guia do projeto e roteiro de apresentação
│
├── src_t1/                             # Tarefa 1: Tiling sem buffer, Validação e Auditoria
│   ├── matrix_multi.c                  # Código C com algoritmo mult_tiling em precisão dupla (dados bipolares)
│   ├── matrix_tiling.py                # Script de validação individual (C vs NumPy)
│   ├── validacao.py                    # Script CLI que calcula N e roda N testes auditados
│   ├── validacao.ipynb                 # Notebook interativo com orquestração, gráficos e auditoria
│   ├── analise_estatistica.md          # Fundamentação teórica das fórmulas de dimensionamento (N)
│   ├── metricas_validacao.md           # Guia didático das métricas (maior_erro_abs, erro_ulps, erro_rel_frob)
│   │
│   ├── audit/                          # Logs estruturados de auditoria
│   │   ├── auditoria_testes.csv        # Log CSV com SHA-256, seeds e métricas dos 30 testes
│   │   └── auditoria_testes.json       # Log JSON estruturado
│   │
│   ├── bin_c/                          # Arquivos .bin gerados pelo programa em C
│   │   ├── A.bin                       # Matriz de entrada A (16.200 bytes)
│   │   ├── B.bin                       # Matriz de entrada B (16.200 bytes)
│   │   └── C.bin                       # Matriz resultante C (16.200 bytes)
│   │
│   └── bin_python/                     # Arquivos .bin gerados pelo Python (NumPy)
│       └── C.bin                       # Matriz de referência C (16.200 bytes)
│
└── src_t2/                             # Tarefa 2: Tiling com buffers de bloco
    └── matrix_tiling.c                 # Implementação de mult_tiling_buffers
```

---

## 🎓 Roteiro para Explicar ao Professor (5 Minutos)

Se você tiver que explicar o projeto amanhã, siga esta sequência clara e direta:

### 1. O que é Tiling (Blocking) e por que usamos?
> *"Na multiplicação ingênua $O(N^3)$, a CPU percorre linhas inteiras de $A$ e colunas inteiras de $B$. Para matrizes grandes, isso estoura a memória Cache L1/L2, forçando o processador a buscar dados na RAM a todo momento (alto número de cache misses).  
> Com o **Tiling**, nós dividimos a matriz em sub-blocos menores ($8 \times 8$). Os 6 laços aninhados (3 externos para navegar nos blocos e 3 internos para multiplicar os elementos) garantem que os dados de cada bloco permaneçam na Cache rápida durante toda a sua computação, maximizando a localidade espacial e temporal."*

### 2. Por que o erro inicial em `float` foi na ordem de $10^{-4}$ e como resolvemos?
> *"Quando testamos inicialmente em `float32` (32 bits), o maior erro absoluto observado foi de $4{,}88 \times 10^{-4}$.  
> Isso não era um bug no algoritmo, mas o limite teórico da precisão simples:  
> - Os elementos de $C$ somam 45 multiplicações e chegam à magnitude de $\sim 1100$.  
> - Em `float32`, o bit menos significativo representável nessa magnitude (**1 ULP**) vale $2^{10 - 23} \approx 1{,}22 \times 10^{-4}$.  
> - O erro de $4{,}88 \times 10^{-4}$ representava apenas **4 ULPs** de divergência, provocados pela não-associatividade da soma e pelo fato de o NumPy usar instruções FMA/SIMD vetorizadas.  
> Para atingir precisão científica estrita, migramos todo o fluxo para **`double` (64 bits)**: a mantissa passou de 24 para 53 bits e o erro despencou para a ordem de **$10^{-13}$**, um ganho de 9 ordens de magnitude."*

### 3. De onde veio o número de testes ($N = 30$)?
> *"Não chutamos o valor de $N$. Utilizamos a **Fórmula 1.A** da nossa fundamentação teórica (`analise_estatistica.md`):  
> $$n = \left( \frac{Z_{\alpha/2} \cdot \sigma}{E} \right)^2$$  
> Rodamos um teste piloto inicial de $n_0 = 10$ repetições em C com dados bipolares (positivo e negativo, $[-10{,}0, +9{,}9]$) para medir a dispersão real ($\bar{x} \approx 3{,}47 \times 10^{-13}$ e $\sigma \approx 6{,}26 \times 10^{-14}$). Com 95% de confiança ($Z = 1{,}96$) e margem admissível $E = 10\%$ da média, a fórmula calculou $n = 13$.  
> Pela regra clássica do **Teorema Central do Limite (TCL)**, adotamos $N = \max(30, 13) = \mathbf{30\text{ testes}}$ para assegurar normalidade assintótica."*

### 4. Como foi deduzida a tolerância de aceitação (com números negativos)?
> *"Para simular sinais reais de sensores embarcados (como acelerômetros e IMUs), as matrizes $A$ e $B$ variam de $-10{,}0$ a $+9{,}9$.  
> Pelo **Teorema do Limite de Erro de Higham**, o erro absoluto máximo no pior caso depende da soma das magnitudes máximas:  
> $$E_{\text{máx}} \le \gamma_K \sum_{k=1}^K |A_{ik}| \cdot |B_{kj}| \le 45 \times (45 \times 10 \times 10) \times 2{,}22 \times 10^{-16} \approx 4{,}5 \times 10^{-11}$$  
> Fixamos a tolerância absoluta $\mathbf{\text{atol} = 10^{-11}}$, que fornece margem segura de $\sim 50$ ULPs.  
> Como termos positivos e negativos se cancelam e geram valores próximos de zero, evitamos divisões perigosas elemento a elemento e usamos a **Norma Relativa de Frobenius** ($\|C_C - C_{Py}\|_F / \|C_{Py}\|_F$), que resultou em $\approx 2{,}4 \times 10^{-16}$, atestando precisão perfeita no nível da máquina."*

### 5. Como os testes foram auditados?
> *"O notebook orquestra o executável C passando sementes exclusivas para cada teste (`./matrix_multi 1001 2001`).  
> Para garantir **rastreabilidade e imutabilidade**, calculamos o hash criptográfico **SHA-256** dos arquivos binários gerados em disco e exportamos tudo para `audit/auditoria_testes.csv`.  
> **Resultado:** 100% dos 30 testes foram aprovados com dados bipolares, com erro máximo observado de apenas $5{,}68 \times 10^{-13}$ (no máximo 5 a 6 ULPs)."*

---

## ⚡ Comandos Rápidos de Execução

Todos os comandos podem ser executados diretamente no terminal a partir da raiz do repositório:

### 1. Compilar e Rodar o Código em C
```powershell
gcc -O2 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi
./matrix_multi
```
*(Gera os arquivos `src_t1/bin_c/A.bin`, `B.bin` e `C.bin` de 16.200 bytes cada)*

### 2. Rodar a Validação Individual em Python
```powershell
.\.venv\Scripts\python.exe src_t1/matrix_tiling.py
```
*(Lê os binários de C, calcula o produto via NumPy, grava `src_t1/bin_python/C.bin` e valida a tolerância)*

### 3. Rodar a Bateria Completa de 30 Testes Auditados (CLI)
```powershell
.\.venv\Scripts\python.exe src_t1/validacao.py
```
*(Dimensiona $N$, compila C, executa os 30 testes com seeds diferentes, gera os hashes SHA-256 e grava `src_t1/auditoria_testes.csv`)*

### 4. Abrir o Notebook Interativo no VS Code
- **[`src_t1/validacao.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/validacao.ipynb)**: Contém o Gráfico de Controle Temporal dos 30 testes, histograma em ULPs, dedução teórica com números negativos e tabela completa de auditoria com SHA-256.
