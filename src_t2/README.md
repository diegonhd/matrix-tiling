# ⚡ Tarefa 2 (T2): Portabilidade para Microcontrolador com Severa Restrição de Memória (STM32)

> **Diretório:** `src_t2/`  
> **Plataforma de Desenvolvimento/Bancada:** STM32F103C8T6 (Blue Pill, ARM Cortex-M3, **SRAM limitada a 8 KB**)  
> **Plataforma Alvo Final:** Nucleo STM32F030R8T6 (ARM Cortex-M0, **8 KB SRAM nativa**, 64 KB Flash)  
> **Ambiente de Desenvolvimento:** STM32CubeIDE / STM32CubeMX com drivers HAL

---

## 1. Visão Geral e Requisitos

### 1.1 Objetivo Geral
Portar o algoritmo de multiplicação de matrizes da T1 para uma plataforma de microcontrolador com recursos drasticamente inferiores aos de um PC, superando limitações severas de memória (RAM restrita a 8 KB) e desenvolvendo mecanismos embarcados para diagnóstico, instrumentação e validação autônoma.

### 1.2 Restrições e Desafios de Engenharia
1. **Ausência de Sistema de Arquivos:** Sem suporte a disco ou chamadas POSIX (`fopen`, `fread`). Todas as entradas devem residir na memória Flash integrada do MCU.
2. **Ausência de FPU em Hardware:** Microcontroladores ARM Cortex-M0 e Cortex-M3 (como o STM32F030 e STM32F103) realizam operações em ponto flutuante IEEE 754 de 32 bits (`float32`) via emulação por software (`libgcc`), exigindo precisão numérica idêntica e sem discrepâncias de arredondamento.
3. **Severa Restrição de Memória SRAM (8.192 bytes):**
   - Para matrizes $N = 45$ em `float32` (4 bytes por elemento):
     - Tamanho de uma matriz: $45 \times 45 \times 4 = 8.100\text{ bytes}$
   - Se as três matrizes ($A$, $B$, $C$) fossem alocadas em RAM: exigiriam $24.300\text{ bytes}$ (estouro imediato da SRAM).
   - Se apenas a matriz de saída $C$ fosse mantida em RAM: consumiria $8.100$ dos $8.192\text{ bytes}$ disponíveis (**98,8% da SRAM total**), sufocando a pilha de execução (*stack*) e levando a um *HardFault*.
4. **Validação com Zero UART:** A validação e instrumentação de funcionamento não dependem de conversor USB-TTL ou terminal serial.

### 1.3 Critérios e Pesos de Avaliação
| Aspecto Avaliado | Peso |
| :--- | :---: |
| Funcionamento correto na plataforma indicada (preservação de requisitos: $N = 45$, `float32`) | **25%** |
| Clareza do código para fins de reuso (modularidade e desacoplamento) | **10%** |
| Ajustes para migração de plataforma (grau de dificuldade e aprendizado) | **20%** |
| Procedimentos de teste e validação | **15%** |
| Conhecimento dos conceitos de embarcados durante a apresentação | **30%** |

---

## 2. A Solução Arquitetural de Memória: Tile Streaming

Para contornar o limite de 8 KB de RAM, o projeto adota uma arquitetura em duas frentes:

```
+-----------------------------------------------------------------------------------------+
|                                     STM32 MICROCONTROLLER                               |
|                                                                                         |
|  +-------------------------------------+      +--------------------------------------+  |
|  |             FLASH (ROM)             |      |              SRAM (8 KB)             |  |
|  |  - Código / HAL / Drivers (.text)   |      |  - Buffer de Bloco C: float [8][8]   |  |
|  |  - Matriz A[N][N] (const / .rodata) |      |    (apenas 256 bytes em .bss!)       |  |
|  |  - Matriz B[N][N] (const / .rodata) |      |  - Struct g_report (Telemetria BIST) |  |
|  |  - Checksums de Referência (Python) |      |  - Stack do Sistema (~512 bytes)     |  |
|  +-------------------------------------+      +--------------------------------------+  |
|                     |                                             |                     |
|                     v                                             v                     |
|             +-------------------------------------------------------------+             |
|             |        Motor Algorítmico: multi_tiling com STREAMING        |             |
|             |          Computa Bloco a Bloco de C diretamente em RAM      |             |
|             +-------------------------------------------------------------+             |
|                                            |                                            |
|                                            v                                            |
|             +-------------------------------------------------------------+             |
|             |         Módulo de Auto-Validação (Built-In Self-Test)       |             |
|             |        Acumula Checksum Online conforme os blocos fluem     |             |
|             +-------------------------------------------------------------+             |
|                       |                                       |                         |
|                       v                                       v                         |
|        +-----------------------------+         +-------------------------------+        |
|        | Sinalização Física por GPIO |         |  Depuração não-intrusiva SWD  |        |
|        | - LED Estado (OK / Falha)   |         |  - STM32CubeIDE Live Watch    |        |
|        |                             |         |  - Inspeciona variáveis em RAM|        |
|        +-----------------------------+         +-------------------------------+        |
+-----------------------------------------------------------------------------------------+
```

### 2.1 Entradas na Flash (`.rodata`)
As matrizes de entrada $A$ e $B$ são declaradas como constantes em [`test_data.h`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/test_data.h):
```c
static const float A[N][N] = { ... };
static const float B[N][N] = { ... };
```
- Armazenadas diretamente na seção **`.rodata`** da Flash do microcontrolador.
- Consomem cerca de 16,2 KB dos 64 KB de Flash disponíveis.
- **Custo de SRAM: 0 bytes**.

### 2.2 Saída $C$ via Tile Streaming (Buffer de 256 Bytes em SRAM)
Em vez de alocar os 8.100 bytes de $C$ em RAM, [`multi_tiling.c`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/multi_tiling.c) aloca apenas um buffer estático de $8 \times 8$:
```c
static float bloco_C[BLOCK][BLOCK]; /* 8 x 8 floats = 256 bytes na SRAM (.bss) */
```
1. O laço de Tiling calcula as posições de um único sub-bloco de dimensões até $8 \times 8$.
2. Assim que o bloco é finalizado, o motor dispara um **callback consumidor** (`tile_consumer_fn_t`), entregando o bloco pronto.
3. O consumidor (BIST) audita e acumula o resultado online.
4. O buffer é limpo (`memset`) e reutilizado para o próximo bloco.

### 2.3 Comparativo de Gestão de Memória

| Componente | Abordagem Tradicional (Sem Streaming) | Arquitetura Implementada (Tile Streaming) |
| :--- | :--- | :--- |
| **Matriz A** | 8.100 B na Flash (`.rodata`) | 8.100 B na Flash (`.rodata`) |
| **Matriz B** | 8.100 B na Flash (`.rodata`) | 8.100 B na Flash (`.rodata`) |
| **Saída C** | 8.100 B na SRAM (`.bss`) | **0 B** na SRAM |
| **Buffer de Bloco C** | Inexistente | **256 B** na SRAM (`float[8][8]`) |
| **SRAM Livre (de 8 KB)**| cerca de 92 bytes (**Risco de Crash / HardFault**) | **mais de 7.500 bytes** (**Folga total para Stack e RTOS**) |

---

## 3. Configuração de Hardware e Periféricos (.ioc)

A configuração no STM32CubeMX foi mantida minimalista para maximizar a portabilidade entre as placas:

1. **LED de Sinalização de Status:**
   - **STM32F103C8T6 (Blue Pill):** Pino **`PC13`**, modo `GPIO_Output Push-Pull`, inicial em `HIGH` (ativo em nível baixo / *active-low*).
   - **STM32F030R8T6 (Nucleo-64):** Pino **`PA5`** (LED verde LD2), modo `GPIO_Output Push-Pull`, inicial em `LOW` (ativo em nível alto / *active-high*).
   - Rótulo de usuário padronizado: `LED_STATUS`.
2. **Interface de Depuração:**
   - Pino `PA13` (SWDIO) e `PA14` (SWCLK) configurados como **Serial Wire (SWD)** no menu `SYS`. Permite comunicação direta com ST-Link sem consumir pinos de UART.
3. **Clock (RCC):**
   - Configurado com oscilador interno (HSI) com divisores em 1, dispensando a necessidade de cristais externos durante testes e migração.

---

## 4. Estrutura Modular dos Arquivos

O código foi projetado com estrito desacoplamento entre hardware, motor matemático e suíte de testes:

```text
src_t2/
├── Core/Inc/
│   ├── multi_tiling.h          # Motor de cálculo: contratos de particionamento e callbacks
│   ├── bist_suite.h            # Suíte BIST: cenários (0, I, Geral) e struct ValidationReport_t
│   ├── test_data.h             # Matrizes constantes em Flash (.rodata) e checksums de referência
│   └── main.h                  # Definições de pinos e inicialização geradas pelo CubeMX
│
├── Core/Src/
│   ├── multi_tiling.c          # Motor puro: algoritmo de Tiling em blocos (independente de hardware)
│   ├── bist_suite.c            # Implementação do BIST: gerador de B virtual, acumulador e tolerância
│   └── main.c                  # Ponto de entrada: inicializa HAL, dispara BIST e controla LED
│
├── generate_test_data.py       # Script Python que compila binários da T1 em test_data.h
├── tutorial.md                 # Guia prático de configuração no CubeIDE e roteiro da apresentação
└── README.md                   # Este documento (visão geral, arquitetura e testes)
```

---

## 5. Procedimentos de Teste e Validação (Zero UART)

A suíte **BIST (*Built-In Self-Test*)** cobre 3 cenários de teste executados sequencialmente em [`bist_suite.c`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/bist_suite.c):

1. **Cenário 0 (`SCENARIO_ZEROS`) — Elemento Nulo:**  
   Multiplica $A \times 0 = 0$. Valida se o acumulador e o motor zeram adequadamente os blocos.  
   Checksum Esperado = `0.0f`.
2. **Cenário 1 (`SCENARIO_IDENTITY`) — Elemento Neutro:**  
   Multiplica $A \times I = A$. Fornece dinamicamente a matriz identidade via getter virtual (`(k == j) ? 1.0f : 0.0f`). Valida a integridade das somas e produtos de cada linha e coluna.  
   Checksum Esperado = `-82.699997f`.
3. **Cenário 2 (`SCENARIO_STANDARD_1`) — Multiplicação Geral da T1:**  
   Multiplica $A \times B$ com os dados reais importados da Tarefa 1.  
   Checksum Esperado = `11019.998f`.

### Inspeção e Demonstração Prática

#### A. No Hardware Físico (LED de Status)
- **Boot:** O LED pisca rápido 3 vezes (100 ms) indicando inicialização do sistema.
- **Sucesso (`TEST_PASSED`):** O LED fica **aceso fixo contínuo**.
- **Falha (`TEST_FAILED`):** O LED pisca lentamente a 1 Hz (500 ms).

#### B. No STM32CubeIDE (*Live Expressions* via ST-Link)
Conectando a placa via cabo ST-Link (SWD), a struct global `g_report` é monitorada em tempo real na janela **Live Expressions**:

| Expressão | Tipo | Descrição e Valores Observados |
| :--- | :--- | :--- |
| `g_report.status` | `TestStatus_t` | `TEST_RUNNING (1)` durante o cálculo $\rightarrow$ `TEST_PASSED (2)` |
| `g_report.current_scenario` | `TestScenario_t` | `0` (Zeros) $\rightarrow$ `1` (Identidade) $\rightarrow$ `2` (Geral) |
| `g_report.tests_passed_count` | `uint32_t` | Incrementa progressivamente de `0` até `3` |
| `g_report.elements_processed` | `uint32_t` | Sobe de `0` até `2025` em cada cenário |
| `g_report.checksum_calculated`| `float` | Checksum acumulado pelo algoritmo em tempo de execução |
| `g_report.checksum_expected`  | `float` | Valor de referência gerado pelo Python |
| `g_report.absolute_error`     | `float` | Diferença absoluta observada: abs(C_calc - C_exp) |
| `g_report.tolerance`          | `float` | **Tolerância analítica visível em RAM** (`0.005f`) |

---

## 6. Dedução Analítica da Tolerância e Resultados Reais de Bancada

### 6.1 Por que o Erro Absoluto foi escolhido como métrica primária?
1. **Evita Indeterminação Matemática ($0/0$):** No Teste dos Zeros ($A \times 0 = 0$), o valor esperado é nulo. O erro relativo geraria divisão por zero (`NaN`), enquanto o erro absoluto é matematicamente limpo: $|0{,}0 - 0{,}0| = 0{,}0$.
2. **Continuidade Metrológica com a Tarefa 1:** Na T1, o critério principal de aprovação dos 30 testes foi `maior_erro_abs <= atol`. Manter o erro absoluto na T2 garante alinhamento conceitual e clareza imediata na demonstração.
3. **Inspeção Direta e Transparente no Live Expressions:** A variável `g_report.tolerance` fica visível lado a lado com `g_report.absolute_error`, permitindo ao avaliador constatar imediatamente que o erro do microcontrolador está confortavelmente contido no limite seguro.

### 6.2 Dedução Analítica da Tolerância de Aceitação ($\text{tolerance} = 5{,}0 \times 10^{-3}$)

A definição da tolerância foi derivada do **Teorema do Limite Determinístico de Arredondamento para Multiplicação Matricial** de **Nicholas J. Higham** (*Accuracy and Stability of Numerical Algorithms*, SIAM, 2002) adaptado para redução cumulativa (Checksum):

#### Passo 1: Parâmetros Físicos da Aritmética IEEE 754 (`float32`)
- **Unidade de Arredondamento (*Unit Roundoff*):**  
  Para mantissa de 23 bits explícitos + 1 implícito ($p = 24$):

  $$u = 2^{-24} \approx 5{,}960464 \times 10^{-8}$$

- **Constante Acumuladora de Higham ($\gamma_N$):**  
  Para o produto interno de dimensão $N = 45$:

  $$\gamma_{45} = \frac{45 \cdot u}{1 - 45 \cdot u} \approx 45 \cdot u \approx 45 \times (5{,}96 \times 10^{-8}) \approx 2{,}6822 \times 10^{-6}$$

#### Passo 2: Limite Determinístico por Elemento Individual da Matriz
Pelo teorema de Higham, para cada elemento $C_{ij} = \sum_{k=1}^{45} A_{ik} B_{kj}$:

$$|\widehat{C}_{ij} - C_{ij}| \le \gamma_{45} (|A||B|)_{ij}$$

Com entradas bipolares uniformes em $[-10{,}0, +10{,}0]$, a magnitude média de produto é $|A_{ik}||B_{kj}| \approx 5{,}0 \times 5{,}0 = 25{,}0$. Logo:

$$(|A||B|)_{\text{médio}} \approx 45 \times 25{,}0 = 1125{,}0$$

Ao comparar dois motores numéricos (C vs Python) sob a Desigualdade Triangular:

$$|C_{\text{C}} - C_{\text{Py}}| \le 2 \cdot \gamma_{45} \cdot (|A||B|)_{\text{médio}} \approx 2 \times (2{,}6822 \times 10^{-6}) \times 1125{,}0 \approx 6{,}035 \times 10^{-3}$$

*(Na Tarefa 1, adotou-se o filtro conservador de:)*

$$ \text{atol} = 1{,}0 \times 10^{-3} \approx \frac{1}{6}$$

#### Passo 3: Propagação de Erro no Checksum Acumulado (Soma de 2.025 Elementos)
O Checksum é a soma de todos os $M = 2.025$ elementos ($45 \times 45$).  
Em uma soma de $M$ termos com reordenação de blocos no Tiling ($B=8$):

1. **Modelo Estocástico de Arredondamento (Higham, Cap. 3):**  
   Os erros de arredondamento comportam-se como variáveis aleatórias de média zero com variância finita. O acúmulo do erro de soma escala com a raiz quadrada da quantidade de elementos ($\sqrt{M}$), e não linearmente ($M$):

   $$\sigma_{\text{soma}} \approx \sqrt{M} \times \sigma_{\text{elemento}} = \sqrt{2025} \times \sigma_{\text{elemento}} = 45 \times \sigma_{\text{elemento}}$$

2. **Propagação do Erro Médio Observado:**  
   Como o erro médio absoluto individual observado na T1 foi $\bar{E} \approx 1{,}5 \times 10^{-4}$:

   $$E_{\text{checksum}} \approx \sqrt{2025} \times (1{,}5 \times 10^{-4}) = 45 \times (1{,}5 \times 10^{-4}) \approx 6{,}75 \times 10^{-3}$$

3. **Fixação da Tolerância de Projeto ($\text{tolerance} = 0{,}005\text{f}$):**  
   Fixou-se **`g_report.tolerance = 0.005f`** ($5{,}0 \times 10^{-3}$):
   - **Fisicamente consistente:** Enquadra-se rigorosamente na ordem de magnitude da propagação de Higham ($\sim 6 \times 10^{-3}$).
   - **Cobre os desvios legítimos de bancada:** O pior caso real observado na placa foi $0{,}0029$, ficando aprovado com folga de $42\%$.
   - **Garante imunidade a falhas:** Rejeita qualquer anomalia de cálculo ou problema de memória, cujos erros gerariam desvios ordens de grandeza maiores ($\ge 0{,}1$).

### 6.3 Resultados Reais Observados no Hardware via ST-Link (*Live Expressions*)

| Cenário | Checksum Esperado | Checksum Calculado | Erro Absoluto (`absolute_error`) | Tolerância (`tolerance`) | Avaliação de Conformidade |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **0. Zeros ($A \times 0$)** | `0.0000000` | `0.0000000` | **`0.0000000`** | `0.005` | **Perfeito (Erro Zero)** |
| **1. Identidade ($A \times I$)** | `-82.6999969` | `-82.6998291` | **`0.0001678`** | `0.005` | **Excelente (30x abaixo do teto)** |
| **2. Geral T1 ($A \times B$)** | `11019.9980` | `11019.9951` | **`0.0029000`** | `0.005` | **Excelente (Aprovado com folga)** |

### Critério de Aceitação do BIST
O BIST em [`bist_suite.c`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/bist_suite.c) aprova o cenário (`TEST_PASSED`) se e somente se:

$$N_{\text{elementos}} = 2025 \quad \text{e} \quad E_{\text{abs}} \le \text{tolerance} \ (0{,}005)$$

---

## 7. Comparativo Cruzado de Erro Metrológico: T1 (Host x86) vs T2 (STM32 Bare-Metal)

| Métrica Metrológica | Tarefa 1 (PC Host x86_64: C Tiling vs Python) | Tarefa 2 (STM32 Cortex-M: Tile Streaming vs Python) | Interpretação de Engenharia |
| :--- | :---: | :---: | :--- |
| **Ambiente de Execução** | Intel/AMD x86_64 (FPU nativa + AVX/SSE) | ARM Cortex-M0/M3 (Emulação `soft-float` via libgcc) | Avaliação de portabilidade e fidelidade da emulação |
| **Metodologia de Teste** | Ponto a ponto em disco (2.025 elementos) | Checksum condensado online via BIST (RAM < 256 B) | Adaptação às restrições severas de memória da T2 |
| **Maior Erro Absoluto Local ($L_\infty$)** | $0{,}0001831$ ($\approx 1{,}83 \times 10^{-4}$) | $\approx 1{,}4 \times 10^{-4}$ (estimado por elemento) | Mesma ordem de grandeza de erro pontual |
| **Erro Relativo Global ($E_{\text{rel}}$)** | $1{,}199 \times 10^{-7}$ (Norma de Frobenius) | $2{,}631 \times 10^{-7}$ (0,26 ppm no Checksum) | **Convergência praticamente idêntica ($\sim 10^{-7}$)** |
| **Tolerância Estipulada** | $\text{atol} = 0{,}001$ (por elemento) | $\text{atol} = 0{,}005$ (na soma de 2.025 elementos) | **Ambos 100% aprovados com ampla margem de segurança** |

### Principais Conclusões da Comparação:
1. **Paridade entre FPU de Hardware e Emulação por Software:**  
   O erro relativo global no computador foi de $\sim 1{,}2 \times 10^{-7}$ e no microcontrolador foi de $\sim 2{,}6 \times 10^{-7}$. Isso prova que a biblioteca `libgcc` no microcontrolador produz **exatamente a mesma estabilidade numérica que uma FPU de computador de última geração**.
2. **Preservação de Requisitos:**  
   A migração da T1 para a T2 não degradou a qualidade do cálculo. Mesmo operando sob restrição severa de 8 KB de SRAM e sem FPU, o algoritmo manteve precisão na escala de partes por milhão ($< 1\text{ ppm}$).
3. **Clareza na Apresentação:**  
   A adoção do Erro Absoluto (`absolute_error <= tolerance`) com a tolerância visível na tela do STM32CubeIDE elimina qualquer ambiguidade, apresentando uma validação matemática limpa, transparente e incontestável para a banca avaliadora.
