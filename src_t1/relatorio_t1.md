# Relatório Técnico e Roteiro de Arguição — Tarefa 1 (T1)

**Disciplina:** Sistemas Embarcados  
**Professor:** Elias  
**Projeto:** Multiplicação de Matrizes com Otimização de Cache por Particionamento em Blocos (*Tiling / Blocking*)  
**Trabalho:** Fase T1 — Implementação e Validação Numérica em C no PC  

---

## 1. Qual o algoritmo?

O algoritmo escolhido e implementado é a **Multiplicação de Matrizes Quadradas com Otimização de Memória Cache por Particionamento em Blocos (*Matrix Multiplication with Tiling / Blocking*)**, calculando a operação matricial:

$$C = A \times B$$

### Enquadramento Rigoroso nos Requisitos do Professor Elias:
* **Laços de Repetição:** O algoritmo possui **6 laços de repetição aninhados** (3 laços externos para navegar entre os blocos e 3 laços internos para multiplicar os elementos dentro dos blocos), atendendo com folga ao requisito de *"pelo menos um laço de repetição, preferencialmente 2 ou mais aninhados"*.
* **Estrutura de Dados no Código da Aplicação:** Matriz bidimensional estática de ordem $N = 45$, totalizando $45 \times 45 = \mathbf{2025\text{ elementos float}}$. Como cada `float` ocupa 4 bytes no padrão IEEE 754, temos:
  $$\text{Tamanho da Estrutura} = 2025 \text{ elementos} \times 4 \text{ bytes} = \mathbf{8.100\text{ bytes}} \quad (\mathbf{7{,}91\text{ KB}} \approx \mathbf{8\text{ KB}})$$
  *Atende com precisão cirúrgica à exigência de "estrutura de dados com aproximadamente 2000 elementos float (equivalente a 8 Kbytes)".*
* **Não é uma lista de dados de entrada:** Trata-se de uma estrutura de dados bidimensional matricial genuína e contígua (`float M[N][N]`), indispensável à lógica matemática da aplicação, e não um simples vetor/lista de amostras unidimensionais.
* **Proibição de Recursão e Alocação Dinâmica:** O código **não utiliza recursividade** e **não utiliza alocação dinâmica** (`malloc`, `calloc`, `free`). Todas as matrizes são alocadas estaticamente na memória da aplicação (`static float A[N][N]`, `B[N][N]`, `C[N][N]`).

---

## 2. Onde ele é usado?

A multiplicação de matrizes particionada em blocos (*Tiling*) é aplicada em sistemas computacionais e embarcados sempre que há necessidade de processar volumes expressivos de dados sob restrições severas de memória e hierarquia de Cache:

1. **Fusão Sensorial e Navegação Inercial:**  
   Em veículos autônomos, robótica e instrumentação aeroespacial, o **Filtro de Kalman (KF)** e o **Filtro de Kalman Estendido (EKF)** executam multiplicações matriciais contínuas ($P_{k} = A P_{k-1} A^T + Q$) para fundir leituras de sensores (acelerômetros, giroscópios e magnetômetros).
2. **TinyML e Inteligência Artificial na Borda (*Edge AI*):**  
   Camadas totalmente conectadas (*Dense Layers*) e convoluções de redes neurais profundas transformadas via `im2col` são executadas no núcleo computacional através de rotinas **GEMM (*General Matrix Multiply*)**.
3. **Controle Moderno de Sistemas Dinâmicos:**  
   Controle por realimentação de estados no espaço de estados discreto ($x[k+1] = A x[k] + B u[k]$) e controladores preditivos baseados em modelo (**MPC** — *Model Predictive Control*).
4. **Processamento Digital de Sinais (DSP) e Visão Computacional:**  
   Transformadas ortogonais bidimensionais (como a DCT na compressão de imagens) e transformações afins de coordenadas geométricas (rotação, escala e projeção).

---

## 3. O que ele faz?

O algoritmo recebe duas matrizes reais $A$ e $B$ de dimensão $45 \times 45$ e calcula a matriz produto $C$ de mesma ordem.

Cada elemento individual $C_{ij}$ é o **produto escalar** entre a $i$-ésima linha da matriz $A$ e a $j$-ésima coluna da matriz $B$:

$$C_{ij} = \sum_{k=1}^N A_{ik} \cdot B_{kj} = A_{i,1} B_{1,j} + A_{i,2} B_{2,j} + \dots + A_{i,N} B_{N,j}$$

### Volume Matemático Computado ($N = 45$):
* **Multiplicações escalares:** $N^3 = 45^3 = \mathbf{91.125\text{ multiplicações}}$.
* **Adições escalares:** $N^2 (N - 1) = 45^2 \times 44 = \mathbf{89.100\text{ adições}}$.
* **Custo computacional total:** $2 N^3 = \mathbf{180.225 \approx 182.250\text{ operações de ponto flutuante (FLOPs)}}$ por ciclo de execução.

---

## 4. Como ele faz?

### O Problema da Multiplicação Ingênua:
Na multiplicação tradicional com 3 laços aninhados (`for i`, `for j`, `for k`), o algoritmo percorre a matriz $A$ por linhas e a matriz $B$ por colunas.
* Como a linguagem C armazena arrays bidimensionais em ordem de linhas (*Row-Major Order*), acessar elementos consecutivos de uma mesma coluna em $B$ exige "saltar" $N \times 4\text{ bytes} = 180\text{ bytes}$ a cada passo.
* Esse padrão de acesso quebra a **localidade espacial** e gera uma taxa severa de falhas de cache (*cache misses*), forçando a CPU a esperar a busca de dados na memória RAM lenta.

### A Estrutura por Tiling (6 Laços Aninhados):
O **Tiling** particiona as matrizes de $45 \times 45$ em submatrizes de tamanho $B \times B$ ($8 \times 8$). Em vez de calcular uma linha inteira de uma só vez, o algoritmo opera sobre blocos que residem integralmente na memória rápida:

* **3 laços externos (`ii`, `jj`, `kk`):** Navegam de bloco em bloco pela matriz em saltos de tamanho `BLOCK` ($8$ em $8$).
* **3 laços internos (`i`, `j`, `k`):** Multiplicam e acumulam os elementos dentro do sub-bloco atual com máxima localidade temporal e espacial.

```c
void mult_tiling(float A[N][N], float B[N][N], float C[N][N]) {
    memset(C, 0, sizeof(float) * N * N);

    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);

            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                for (int i = ii; i < i_max; i++) {
                    for (int j = jj; j < j_max; j++) {
                        float soma = C[i][j];
                        for (int k = kk; k < k_max; k++) {
                            soma += A[i][k] * B[k][j];
                        }
                        C[i][j] = soma;
                    }
                }
            }
        }
    }
}
```

### Tratamento Matemático de Borda (`min_int`):
Como $N = 45$ não é divisível por $B = 8$ ($45 = 5 \times 8 + 5$), a matriz é particionada em 5 blocos regulares de tamanho 8 e 1 bloco residual de borda de tamanho 5.  
A função auxiliar inline `min_int(a, b)` calcula dinamicamente o teto dos laços (`i_max`, `j_max`, `k_max`), impedindo acessos fora dos limites do array (*buffer overflow*) sem desperdiçar memória nem ciclos computando zeros artificiais (*padding*).

---

## 5. Quais os dados de entrada necessários para o algoritmo?

### A Estrutura de Dados de $\approx 8\text{ KB}$ no Código da Aplicação:
Conforme estabelecido nos requisitos do professor Elias, o algoritmo opera sobre estruturas de dados genuínas dimensionadas para $\approx 8\text{ KB}$ cada:

* **Dimensão:** Matriz quadrada $45 \times 45 = \mathbf{2025\text{ elementos}}$.
* **Tipo do dado:** `float` (ponto flutuante IEEE 754 de 32 bits = **4 bytes**).
* **Volumetria exata por matriz:**
  $$\text{Tamanho} = 2025 \times 4\text{ bytes} = \mathbf{8.100\text{ bytes}} \quad (\mathbf{7{,}91\text{ KB}} \approx \mathbf{8\text{ KB}})$$

| Estrutura de Dados | Papel no Algoritmo | Formato | Tamanho Exato |
| :--- | :--- | :--- | :--- |
| **`A[45][45]`** | Matriz de Entrada $A$ (dados simulados de sensores) | Binário bruto (`float32`) | **$8.100\text{ bytes}$** ($7{,}91\text{ KB}$) |
| **`B[45][45]`** | Matriz de Entrada $B$ (matriz de estados / coeficientes) | Binário bruto (`float32`) | **$8.100\text{ bytes}$** ($7{,}91\text{ KB}$) |
| **Total de Entrada** | Estruturas $A$ e $B$ combinadas | IEEE 754 estático | **$16.200\text{ bytes}$** ($15{,}82\text{ KB}$) |

> **Observação Fundamental para a Banca:**  
> Essa estrutura **não é uma lista plana unidimensional de entrada**. É uma estrutura matricial bidimensional genuína da aplicação, alocada estaticamente na memória (`static float A[N][N]`, `static float B[N][N]`), indispensável à computação algorítmica.

### Natureza Física dos Dados (Dados Bipolares):
* As matrizes $A$ e $B$ são preenchidas com números reais no intervalo uniforme de **$-10{,}0\text{f}$ a $+9{,}9\text{f}$**, simulando sinais físicos de sensores com leituras positivas e negativas (dados bipolares).
* A geração utiliza sementes determinísticas (`seedA`, `seedB`) passadas via linha de comando (`./matrix_multi 1001 2001`), assegurando **reprodutibilidade metrológica estrita**.

### Parâmetros de Configuração:
* Dimensão parametrizável com guarda: `#ifndef N / #define N 45 / #endif`.
* Tamanho do bloco de cache: `#define BLOCK 8`.

---

## 6. O que o algoritmo entrega na saída?

1. **A Matriz Resultante $C = A \times B$:**
   * Dimensão: $45 \times 45 = 2025$ elementos de tipo `float`.
   * Tamanho exato: **$8.100\text{ bytes}$** ($7{,}91\text{ KB}$).
   * Serializada em formato binário bruto contíguo em [`src_t1/bin_c/C.bin`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/bin_c/C.bin) através de `fwrite(C, sizeof(float), N * N, arquivo)`.

2. **Auditoria Estruturada e Rastreabilidade:**
   * O pipeline exporta logs completos em [`src_t1/audit/auditoria_testes.csv`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/audit/auditoria_testes.csv) e [`src_t1/audit/auditoria_testes.json`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/audit/auditoria_testes.json), registrando as sementes, os hashes criptográficos **SHA-256** dos binários em disco e as métricas numéricas calculadas.

---

## 7. Como foi o procedimento de teste e o que garante que ele funciona?

### Arquitetura de Validação Cruzada Unidirecional:
Para garantir que a validação fosse 100% científica e auditável:
1. **O C gera as entradas e calcula a saída:** O programa em C compila, gera deterministicamente as matrizes $A$ e $B$, calcula $C_{\text{tiling}}$ e salva os binários em `bin_c/`.
2. **O Python consome estritamente os dados do C:** O módulo [`matrix_multi.py`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_multi.py) **não gera matrizes aleatórias próprias**. Ele lê exclusivamente `bin_c/A.bin` e `bin_c/B.bin`, executa o gabarito $C_{\text{NumPy}} = A @ B$ com a biblioteca de álgebra linear do NumPy em `float32`, e grava `bin_python/C.bin`.
3. **Auditoria no Notebook:** O notebook [`validacao.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/validacao.ipynb) orquestra os testes, confere os hashes SHA-256 e audita os resultados frente a três métricas rigorosas.

### Dimensionamento Amostral Estatístico ($N_{\text{TESTES}} = 30$):
O número de testes não foi arbitrário; foi derivado formalmente pela fórmula do **Intervalo de Confiança da Média**:

$$n = \left( \frac{Z_{\alpha/2} \cdot \sigma}{E} \right)^2$$

* Executou-se um ensaio piloto de $n_0 = 10$ testes determinísticos para medir a dispersão real: média piloto $\bar{x} \approx 1{,}86 \times 10^{-4}$ e desvio-padrão amostral $\sigma \approx 5{,}47 \times 10^{-5}$.
* Adotando nível de confiança de 95% ($Z_{\alpha/2} = 1{,}96$) e margem admissível $E = 15\% \cdot \bar{x}$, a fórmula calculou:
  $$n = \left( \frac{1{,}96 \times 5{,}47 \times 10^{-5}}{0{,}15 \times 1{,}86 \times 10^{-4}} \right)^2 \approx 14{,}7 \implies \lceil 14{,}7 \rceil = \mathbf{15\text{ testes}}$$
* Pelo princípio clássico do **Teorema Central do Limite (TCL)**, que preconiza amostras com $N \ge 30$ para assegurar normalidade assintótica, adotou-se oficialmente:
  $$N_{\text{TESTES}} = \max(30, 15) = \mathbf{30\text{ testes}}$$

### As Três Métricas Numéricas Oficiais:
1. **`maior_erro_abs` (Critério Primário PASS/FAIL):**  
   $$\text{maior\_erro\_abs} = \max_{i,j} |C_{ij}^{\text{C}} - C_{ij}^{\text{NumPy}}|$$
   Avalia o erro do pior elemento da matriz inteira frente à tolerância analítica $\mathbf{\text{atol} = 1{,}0 \times 10^{-3}}$.
2. **`erro_ulps` (Desvio Físico no Silício):**  
   Mede quantos "degraus mínimos" (*Units in the Last Place*) do bit menos significativo da mantissa o C se afastou do NumPy:
   $$1\text{ ULP}(1125) = 2^{10 - 23} = 2^{-13} \approx \mathbf{1{,}2207 \times 10^{-4}}$$
   $$\text{erro\_ulps} = \frac{\text{maior\_erro\_abs}}{1\text{ ULP}}$$
3. **`erro_rel_frob` (Norma Relativa de Frobenius):**  
   Calcula a razão da energia euclidiana total de erro sobre a energia total do gabarito, sendo imune a cancelamentos pontuais com dados bipolares:
   $$\text{erro\_rel\_frob} = \frac{\|C_{\text{C}} - C_{\text{NumPy}}\|_F}{\|C_{\text{NumPy}}\|_F}$$

### Resultados Obtidos na Bateria Oficial:
* **Taxa de Aprovação:** **30 de 30 testes aprovados (100% PASS)**.
* **Maior Erro Absoluto Observado:** $\mathbf{3{,}05 \times 10^{-4}}$ (com folga de mais de 3 vezes frente ao limite $\text{atol} = 1{,}0 \times 10^{-3}$).
* **Desvio em ULPs:** Pior caso de **$5{,}00\text{ ULPs}$** (média de $2{,}75\text{ ULPs}$), atestando que **21 a 23 bits da mantissa foram perfeitamente idênticos** nos dois códigos.
* **Erro de Frobenius:** $\mathbf{1{,}23 \times 10^{-7}}$, convergindo perfeitamente para o **épsilon da máquina** em precisão simples ($\epsilon_{\text{mach}} = 2^{-23} \approx 1{,}19 \times 10^{-7}$).

---

## 8. Quais as dificuldades encontradas e como foram resolvidas? *(Ponto crítico para a banca)*

### Dificuldade 1: A "Ilusão do Erro Zero" vs. A Restrição de 8 KB da Aplicação
* **O Problema:** Inicialmente, esperava-se que o resultado de C e Python fosse bit-a-bit idêntico. Ao constatar um erro absoluto na ordem de $\approx 3 \times 10^{-4}$, a primeira reação intuitiva foi migrar o código para `double` (64 bits), onde o erro caiu para $10^{-13}$.
* **Por que isso violava os requisitos:** O uso de `double` (8 bytes por número) **dobrou a estrutura de dados para $16.200\text{ bytes}$ ($16{,}2\text{ KB}$)**, violando frontalmente a especificação do professor Elias, que exige uma estrutura de dados de $\approx 8\text{ KB}$ (aproximadamente 2000 elementos de float). Além disso, em computação de baixo nível, dobrar desnecessariamente o consumo de memória compromete a ocupação de cache e a eficiência do hardware.
* **A Resolução:** Realizamos a análise matemática formal da mantissa IEEE 754 de 23 bits. Provamos que para a magnitude acumulada de $\approx 1125$, o valor físico de **1 ULP vale exatamente $1{,}22 \times 10^{-4}$**. Portanto, um erro de $3 \times 10^{-4}$ representa meros **2 a 3 bits de arredondamento natural** provocados pela reordenação das somas no Tiling e pelo uso de instruções vetoriais SIMD/FMA no NumPy. Retornamos para `float32` de forma 100% consistente, restabelecendo a estrutura nos exatos **$8.100\text{ bytes}$** exigidos.

### Dificuldade 2: Divisão por Zero e Falsos Alarmes com Dados Bipolares
* **O Problema:** Como os dados simulam sinais reais de sensores com valores positivos e negativos em $[-10, +10]$, termos opostos se anulam na soma ponderada, gerando elementos nulos ($C_{ij} = 0{,}0$) ou minúsculos ($C_{ij} \approx 10^{-7}$). A métrica convencional de erro relativo elemento a elemento tentava dividir por zero, provocando falhas fatais (`ZeroDivisionError`) ou acusando falsamente erros de $1000\%$ em elementos que divergiam apenas no ruído de fundo.
* **A Resolução:** Substituímos o erro relativo escalar pela **Norma Relativa de Frobenius**. A norma quantifica a energia global da matriz inteira de 2025 números elevados ao quadrado. O denominador $\|C_{\text{NumPy}}\|_F$ soma todos os quadrados e atinge valores estáveis na ordem de milhares ($\approx 5.000$), tornando a métrica imune a cancelamentos pontuais e provando que o erro percentual global coincide exatamente com o épsilon da máquina ($\approx 1{,}2 \times 10^{-7}$).

### Dificuldade 3: Tratamento de Borda da Matriz ($N$ não divisível pelo Bloco)
* **O Problema:** A dimensão exigida era $N = 45$ e o bloco ideal de cache era $B = 8$. Como $45$ não é múltiplo de $8$ ($45 = 5 \times 8 + 5$), a navegação em blocos regulares transbordaria os limites do array na última iteração ($48 > 45$), provocando violação de memória (*segmentation fault*).
* **A Resolução:** Implementamos a função auxiliar inline `min_int(ii + BLOCK, N)` diretamente nos limites superiores dos laços internos (`i_max`, `j_max`, `k_max`). Isso garantiu que os blocos periféricos fossem calculados com sua dimensão real ($5 \times 8$, $8 \times 5$ ou $5 \times 5$), sem violar os limites de memória e sem alocar memória artificial (*padding* de zeros).

### Dificuldade 4: Conflito Interno de Derivações e Erro de Escala no Notebook
* **O Problema:** O notebook continha duas derivações conflitantes para a tolerância (uma por Higham em $10^{-11}$ e outra por ULP em $10^{-13}$ herdada do antigo double), além de um erro de digitação na fórmula de tolerância dinâmica com diferença de fator 10 entre a fórmula e o exemplo numérico.
* **A Resolução:** Unificamos toda a teoria na derivação direta por ULP e magnitude acumulada em `float32`. Corrigimos a fórmula dinâmica para $\operatorname{atol}_{din}(N) = 1{,}0 \times 10^{-3} \times (N/45)$, harmonizando a teoria, a fórmula e a tolerância de corte ($\text{atol} = 1{,}0 \times 10^{-3}$) com precisão matemática estrita.

---

## 9. O que vocês aprenderam durante o T1? *(Ponto crítico para a banca)*

### 1. Ponto Flutuante no Silício não é Associativo: A Ordem Importa
Aprendemos que em computação numérica de precisão finita, **$(a + b) + c \neq a + (b + c)$**.  
Mesmo utilizando exatamente o mesmo tipo de dado (`float32`), alterar a ordem dos laços para otimizar a memória Cache altera a sequência dos arredondamentos parciais de hardware. Além disso, bibliotecas de alta performance como o NumPy utilizam instruções vetoriais SIMD e FMA (*Fused Multiply-Add*), que realizam reduções em árvore e arredondamentos únicos. Portanto, **validação científica em ponto flutuante não busca erro zero**, mas sim a comprovação de que o desvio observado corresponde a meros passos de resolução de hardware (ULPs).

### 2. O Valor de Projetar sob Restrições Rígidas de Recursos
Compreendemos na prática o significado de dimensionar estruturas de dados em código de baixo nível:
* A especificação de $\approx 8\text{ KB}$ não é um número aleatório; ela ensina o desenvolvedor a calcular a pegada de memória (*footprint*) da sua aplicação ($45 \times 45 \times 4\text{ B} = 8.100\text{ bytes}$).
* Compreendemos por que recursão e alocação dinâmica são vetadas: recursão consome a pilha (*stack*) de forma imprevisível, e alocação dinâmica fragmenta a memória e introduz comportamento não determinístico. O uso de arrays estáticos contíguos garante determinismo absoluto de tempo e espaço.

### 3. A Importância do Rigor Metrológico
Aprendemos a estruturar um pipeline de validação profissional:
* 
* Aplicação de **métodos estatísticos formais** (Intervalo de Confiança e Teorema Central do Limite) para dimensionar cientificamente o número de testes ($N = 30$), em vez de simplesmente "chutar" um número arbitrário.

* Uso da métrica de tolerância a fim de observar os valores de erros absolutos e relativos de outra forma. O cálculo de tolerância e as comparações conseguintes em relação ao valor obtido nos ajuda a ter uma base do nível de limitações do nosso algoritmo. 

### 4. Compreensão Prática da Hierarquia de Memória e Otimização de Cache
Compreendemos o impacto da localidade temporal e espacial na arquitetura de computadores. Entendemos por que um algoritmo com 6 laços aninhados (Tiling) supera a multiplicação ingênua de 3 laços: o particionamento em blocos reutiliza os dados enquanto eles residem nas linhas de cache mais rápidas, minimizando os acessos caros à memória RAM principal.
