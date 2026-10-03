# 🚀 Multiplicação de Matrizes com Tiling (Otimização de Cache) e Validação Estatística

Este projeto implementa e valida o algoritmo de multiplicação de matrizes quadradas ($45 \times 45$) com particionamento em blocos (**Tiling / Blocking**, $B = 8$) em linguagem **C**, comparando os resultados contra o gabarito vetorizado do **NumPy** em Python sob o padrão IEEE 754 de precisão simples (`float` / 32 bits).

---

## 📁 Estrutura do Projeto

```text
matrix-tiling/
├── .venv/                              # Ambiente virtual Python configurado (numpy, matplotlib)
├── requirements.txt                    # Dependências exatas do projeto
├── README.md                           # Guia do projeto e roteiro de apresentação
│
├── src_t1/                             # Tarefa 1: Tiling sem buffer, Validação e Auditoria
│   ├── matrix_multi.c                  # Módulo C: computação mult_tiling em precisão simples (float, dados bipolares)
│   ├── matrix_multi.py                # Módulo Python: multiplicação de referência com NumPy (float32)
│   ├── validacao.ipynb                 # Notebook interativo: orquestração de C e Python, gráficos e auditoria
│   ├── metricas_validacao.md           # Guia didático das métricas (maior_erro_abs, erro_ulps, erro_rel_frob)
│   │
│   ├── audit/                          # Logs estruturados de auditoria
│   │   ├── auditoria_testes.csv        # Log CSV com SHA-256, seeds e métricas dos 30 testes
│   │   └── auditoria_testes.json       # Log JSON estruturado
│   │
│   ├── bin_c/                          # Arquivos .bin gerados pelo programa em C (float32)
│   │   ├── A.bin                       # Matriz de entrada A (8.100 bytes)
│   │   ├── B.bin                       # Matriz de entrada B (8.100 bytes)
│   │   └── C.bin                       # Matriz resultante C (8.100 bytes)
│   │
│   └── bin_python/                     # Arquivos .bin gerados pelo Python / NumPy (float32)
│       └── C.bin                       # Matriz de referência C (8.100 bytes)
│
└── src_t2/                             # Tarefa 2: Tiling com buffers de bloco
    └── matrix_tiling.c                 # Implementação de mult_tiling_buffers (float, com guarda #ifndef N)
```

---

## 🎓 Roteiro para Explicar ao Professor (5 Minutos)

Se você tiver que explicar o projeto na arguição oral, siga esta sequência clara e direta:

### 1. O que é Tiling (Blocking) e por que usamos?
> *"Na multiplicação ingênua $O(N^3)$, a CPU percorre linhas inteiras de $A$ e colunas inteiras de $B$. Para matrizes grandes, isso estoura a memória Cache L1/L2, forçando o processador a buscar dados na RAM a todo momento (alto número de cache misses).  
> Com o **Tiling**, nós dividimos a matriz em sub-blocos menores ($8 \times 8$). Os 6 laços aninhados (3 externos para navegar nos blocos e 3 internos para multiplicar os elementos) garantem que os dados de cada bloco permaneçam na Cache rápida durante toda a sua computação, maximizando a localidade espacial e temporal."*

### 2. Por que o tipo de dado adotado é estritamente `float` (32 bits)?
> *"Na fase T2, este algoritmo será migrado para o microcontrolador **STM32F030F4** (ARM Cortex-M0, 16 KB Flash, 4 KB RAM, **sem FPU em hardware**).  
> Se usássemos `double` (64 bits), cada operação aritmética seria pesadamente emulada por rotinas de software, inflacionando o consumo de ciclos de clock, o tamanho do binário em Flash e dobrando os buffers de bloco em RAM (de 768 B para 1536 B).  
> O tipo `float` (4 bytes por elemento, 23 bits de mantissa) atende perfeitamente à aplicação com consumo enxuto de recursos: as matrizes de $45 \times 45$ ocupam $8.100\text{ bytes}$ cada."*

### 3. Por que o erro em `float` é na ordem de $10^{-4}$ e comprova a exatidão?
> *"Os elementos da matriz resultante acumulam 45 multiplicações de fatores bipolares $[-10{,}0, +9{,}9]$, atingindo magnitudes médias de $\sim 1125$ ($E = 10$).  
> Em precisão simples (`float32`), a resolução do bit menos significativo nessa magnitude (**1 ULP**) vale:  
> $$1\text{ ULP} = 2^{10 - 23} = 2^{-13} \approx 1{,}22 \times 10^{-4}$$  
> O maior erro absoluto observado entre o C e o NumPy foi de apenas $3{,}05 \times 10^{-4}$, o que corresponde a meros **2 a 5 ULPs** de desvio (ruído natural de arredondamento e reordenação de somas). Dos 24 bits da mantissa, **21 a 23 bits foram exatamente idênticos**."*

### 4. De onde veio o número de testes ($N_{\text{TESTES}} = 30$)?
> *"Utilizamos a fórmula estatística do Intervalo de Confiança da Média:  
> $$n = \left( \frac{Z_{\alpha/2} \cdot \sigma}{E} \right)^2$$  
> Executamos uma amostra piloto de $n_0 = 10$ repetições em C com sementes determinísticas para medir a dispersão real ($\bar{x} \approx 1{,}86 \times 10^{-4}$ e $\sigma \approx 5{,}47 \times 10^{-5}$). Com 95% de confiança ($Z = 1{,}96$) e margem admissível $E = 15\%$ da média piloto, a fórmula resultou em $n = 15$.  
> Pelo princípio do **Teorema Central do Limite (TCL)**, que exige $N \ge 30$ para garantir normalidade assintótica, adotamos exatamente $\mathbf{30\text{ testes}}$."*

### 5. Como os testes foram auditados?
> *"O notebook orquestra o executável C passando sementes exclusivas para cada teste (`./matrix_multi 1001 2001`).  
> O Python consome exclusivamente os arquivos `A.bin` e `B.bin` gerados pelo C e computa a referência $C_{\text{NumPy}} = A \times B$.  
> Para garantir **rastreabilidade e imutabilidade**, calculamos o hash criptográfico **SHA-256** de todos os arquivos em disco e registramos tudo em `audit/auditoria_testes.csv`.  
> **Resultado:** 100% dos 30 testes foram aprovados frente à tolerância $\text{atol} = 1{,}0 \times 10^{-3}$, e o erro relativo de Frobenius convergiu para $\approx 1{,}23 \times 10^{-7}$, exatamente coincidente com o épsilon da máquina ($\epsilon_{\text{mach}} \approx 1{,}19 \times 10^{-7}$)."*

---

## ⚡ Comandos Rápidos de Execução

Todos os comandos podem ser executados diretamente no terminal a partir da raiz do repositório:

### 1. Compilar e Rodar o Código em C
```powershell
gcc -O2 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi
./matrix_multi
```
*(Gera os arquivos `src_t1/bin_c/A.bin`, `B.bin` e `C.bin` de 8.100 bytes cada)*

### 2. Rodar a Validação Individual em Python
```powershell
.\.venv\Scripts\python.exe src_t1/matrix_multi.py
```
*(Lê os binários de C, calcula o produto via NumPy em float32, grava `src_t1/bin_python/C.bin` de 8.100 bytes e valida a tolerância)*

### 3. Abrir o Notebook Interativo no VS Code (Validação e Auditoria)
- **[`src_t1/validacao.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/validacao.ipynb)**: Orquestra tanto o executável C (`matrix_multi.exe`) quanto o módulo Python (`matrix_multi.py`), executa a bateria estatística de 30 testes auditados com SHA-256 e plota os gráficos de controle e histogramas.
