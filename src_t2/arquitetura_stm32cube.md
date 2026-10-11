# Arquitetura de Software e Hardware no STM32Cube (T2)

> **Documento de Arquitetura de Referência**  
> Projeto: Multiplicação de Matrizes com Tiling em Microcontroladores com Restrição Severa de Recursos.  
> Plataformas: **STM32F103C8T6** (Bancada com 8 KB SRAM limitada) $\rightarrow$ **STM32F030R8T6** (Placa Alvo Final, 8 KB SRAM nativa).

---

## 1. Visão Geral da Arquitetura

O sistema foi arquitetado para satisfazer duas restrições fundamentais:
1. **Restrição Severa de SRAM (8 KB):** Matrizes de dimensão $N = 45$ em ponto flutuante IEEE 754 de precisão simples (`float32`, 4 bytes) demandam $45 \times 45 \times 4 = 8.100\text{ bytes}$ por matriz. Três matrizes ocupariam $24{,}3\text{ KB}$ (inviável para 8 KB). Mesmo manter a matriz $C$ inteira em RAM consumiria 98,8% da memória total, sufocando o stack. A solução adotada é:
   - Entradas $A$ e $B$ armazenadas na **Flash** (`.rodata`, ocupando $\approx 16{,}2\text{ KB}$ dos 64 KB disponíveis).
   - Saída $C$ computada via **Tile Streaming** (em blocos de $8 \times 8$ em RAM), reduzindo drasticamente o consumo de SRAM para centenas de bytes.
2. **Ausência de Interface UART / Conversor USB-TTL:** A validação e instrumentação de funcionamento ocorrem sem depender de terminal serial, utilizando:
   - **Protocolo de Diagnóstico por GPIO (LED de Status)** no hardware físico.
   - **Depuração Não-Intrusiva via SWD (ST-Link)** no STM32CubeIDE utilizando a ferramenta **Live Expressions**.

```
+-----------------------------------------------------------------------------------------+
|                                     STM32 MICROCONTROLLER                               |
|                                                                                         |
|  +-------------------------------------+      +--------------------------------------+  |
|  |             FLASH (ROM)             |      |              SRAM (8 KB)             |  |
|  |  - Código / HAL / Drivers           |      |  - Buffer de Bloco C: float [8][8]   |  |
|  |  - Matriz A[N][N] (const / .rodata) |      |    (apenas 256 bytes!)               |  |
|  |  - Matriz B[N][N] (const / .rodata) |      |  - Variáveis de Status / BIST        |  |
|  |  - Checksum de Referência (Python)  |      |  - Stack do Sistema (~512 bytes)     |  |
|  +-------------------------------------+      +--------------------------------------+  |
|                     |                                             |                     |
|                     v                                             v                     |
|             +-------------------------------------------------------------+             |
|             |          Motor Algorítmico: mult_tiling com STREAMING       |             |
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

---

## 2. Camadas do Software e Configurações de Hardware

### Camada 1: Configuração de Hardware e Periféricos no STM32Cube (.ioc)

A configuração no CubeMX (.ioc) é deliberadamente minimalista para manter máxima portabilidade e footprint reduzido de código.

#### Configuração de Pinos e Periféricos (.ioc):
1. **Pino de LED de Sinalização (Status LED):**
   - **Na STM32F103C8T6 (Blue Pill):**
     - Pino: **`PC13`**
     - Modo: **`GPIO_Output`**
     - Configurações do GPIO:
       - *GPIO output level:* `High` (inicia apagado, pois o LED da Blue Pill é ativo em nível baixo)
       - *GPIO mode:* `Output Push Pull`
       - *GPIO Pull-up/Pull-down:* `No pull-up and no pull-down`
       - *Maximum output speed:* `Low`
       - *User Label:* `LED_STATUS`
   - **Na STM32F030R8T6 (Nucleo-64):**
     - Pino: **`PA5`** (LED verde LD2 da Nucleo)
     - Modo: **`GPIO_Output`**
     - Configurações do GPIO:
       - *GPIO output level:* `Low` (inicia apagado, LED ativo em nível alto)
       - *GPIO mode:* `Output Push Pull`
       - *GPIO Pull-up/Pull-down:* `No pull-up and no pull-down`
       - *Maximum output speed:* `Low`
       - *User Label:* `LED_STATUS`
2. **Interface de Programação e Depuração (Debug):**
   - **STM32F103C8T6:** `SYS` $\rightarrow$ `Debug` = **Serial Wire** (Pinos `PA13` / SWDIO e `PA14` / SWCLK).
   - **STM32F030R8T6:** `SYS` $\rightarrow$ `Debug` = **Serial Wire** (Pinos `PA13` / SWDIO e `PA14` / SWCLK).
3. **Clock (RCC):**
   - Fonte: Oscilador Interno (**HSE**) ativado para dispensar cristais externos na migração.
   - Prescalers em divisão 1 (clock nativo HSI).

---

### Camada 2: Algoritmo de Tiling e Viabilidade com Referência Python

#### É possível manter a mesma referência do código Python usando apenas `mult_tiling`?
**Sim, perfeitamente.**
- O algoritmo `mult_tiling` matemático é determinístico e utiliza as mesmas operações de soma e produto em ponto flutuante IEEE 754 de 32 bits (`float32`) tanto no Python (`numpy.float32`) quanto no GCC ARM (`arm-none-eabi-gcc`).
- Como você descartou o uso de `mult_tiling_buffers`, a lógica fica limpa e focada em uma única implementação canônica.

#### A Solução de Memória: Tile Streaming
Como não podemos mudar o tamanho de entrada ($N = 45$) e a RAM inteira da placa final é de apenas 8.192 bytes, alocar `float C[45][45]` na SRAM (8.100 bytes) causa colapso do stack e risco iminente de HardFault.

Com o **Tile Streaming**:
1. Não alocamos a matriz $C$ inteira em memória RAM.
2. Alocamos apenas um pequeno buffer de bloco em RAM:
   $$\text{Buffer } C_{\text{tile}} = 8 \times 8 \times 4\text{ bytes} = 256\text{ bytes}$$
3. O laço de Tiling calcula um bloco de coordenadas $(ii, jj)$ de tamanho até $8 \times 8$:
   - O bloco é preenchido e somado usando as matrizes $A$ e $B$ fixas na Flash.
   - Assim que o bloco $(ii, jj)$ é finalizado, seus valores são passados diretamente para o validador de teste (que acumula o checksum).
   - O mesmo buffer de 256 bytes é reutilizado para o próximo bloco!
4. **Economia de RAM:** Reduzimos a necessidade de SRAM de **8.100 bytes** para **256 bytes** (economia superior a 96% de RAM). O restante dos 8 KB de SRAM fica 100% livre para o stack e variáveis do sistema.

---

### Camada 3: Avaliação do `test_data.h` e Alternativas de Teste e Validação

#### Avaliação Crítica da abordagem atual com `test_data.h`:
- **Vantagens:** 
  - Coloca as matrizes $A$ e $B$ na Flash (`.rodata`), contornando a restrição de RAM.
  - Garante que a entrada de dados permaneça exatamente a mesma da T1 (mesmos binários convertidos).
- **Desvantagem da abordagem pura (sem streaming):** 
  - Se `test_data.h` exigir que a saída esperada $C$ também seja embutida inteira na Flash (mais 8.100 bytes), gastamos espaço de Flash que poderia ser poupado.

#### Soluções e Procedimentos Alternativos de Teste (Sem UART):

1. **Solução 1: BIST com Checksum Condensado (Recomendada & Mais Robusta)**
   - O script Python gera no `test_data.h`:
     - As matrizes $A$ e $B$ em `.rodata`.
     - Dois checksums de referência da matriz $C$:
       - `CHECKSUM_SUM_EXPECTED`: soma algébrica de todos os elementos ($\sum C_{ij}$).
       - `CHECKSUM_HASH_EXPECTED`: hash FNV-1a de 32 bits calculado sobre os bytes de $C$ em float32.
   - O microcontrolador acumula ambos os valores durante o Tile Streaming e disponibiliza uma struct global em RAM (`volatile ValidationReport_t g_report;`).
   - **Como visualizar essa validação na prática (mesmo sem UART):**
     - **Visualização 1 (No STM32CubeIDE via Live Expressions):**
       Ao colocar a struct `g_report` na janela *Live Expressions*, o avaliador vê em tempo real:
       - `g_report.status`: muda de `TEST_RUNNING (0)` para `TEST_PASSED (1)` ou `TEST_FAILED (2)`.
       - `g_report.elements_processed`: contador subindo de 0 até 2025.
       - `g_report.checksum_calculated` vs `g_report.checksum_expected`: ambos os números lado a lado comprovando que batem idênticos.
       - `g_report.absolute_error`: erro numérico (ex: `0.000000e+00` ou `< 1e-4`).
     - **Visualização 2 (Na Placa Física via LED de Status):**
       - LED pisca rápido a 5 Hz enquanto `elements_processed` incrementa.
       - Se `status == TEST_PASSED`: LED acende fixo contínuo.
       - Se `status == TEST_FAILED`: LED pisca lento (1 Hz).
   - **Vantagem Acadêmica:** Gasta apenas alguns bytes em RAM, valida **100% dos 2025 elementos** de $C$ e fornece uma tela clara de auditoria no CubeIDE.

---

## 3. Gestão de Memória: Mapa Comparativo

| Componente | Sem Tile Streaming (Alocação Estática Total) | Com Tile Streaming (Arquitetura Adotada) |
| :--- | :--- | :--- |
| **Matriz A** | 8.100 B na Flash (`.rodata`) | 8.100 B na Flash (`.rodata`) |
| **Matriz B** | 8.100 B na Flash (`.rodata`) | 8.100 B na Flash (`.rodata`) |
| **Saída C** | 8.100 B na SRAM (`.bss`) | **0 B** na SRAM |
| **Buffer de Bloco C** | Não aplicável | **256 B** na SRAM (`float[8][8]`) |
| **SRAM Livre (de 8 KB)** | $\approx \mathbf{92\text{ bytes}}$ (Risco altíssimo de Crash) | $\mathbf{> 7.500\text{ bytes}}$ (**Segurança total para Stack**) |

---

## 4. Estratégia de Demonstração na Placa Física (8 Minutos)

Conforme definido, a demonstração e avaliação serão fundamentadas estritamente nas estratégias **1** e **3**:

### 4.1 Estratégia 1: Sinalização Física Autónoma por GPIO (LED de Status)
Demonstração visual imediata na placa física sem necessidade de computador conectado:
- **Estado 1 (Processando):** LED piscando rapidamente a 5 Hz enquanto o algoritmo executa o Tile Streaming.
- **Estado 2 (Sucesso / PASS):** LED permanece **ACESO FIXO** contínuo após o término, indicando que o BIST calculou todos os blocos e o checksum conferiu com a referência do Python.
- **Estado 3 (Falha / FAIL):** LED pisca lentamente a 1 Hz (ou padrão de erro), indicando divergência numérica.

### 4.2 Estratégia 2: Depuração e Inspeção Não-Intrusiva no STM32CubeIDE (Live Expressions)
Demonstração técnica com a placa conectada via cabo USB do ST-Link:
1. No STM32CubeIDE, inicia-se a sessão de depuração e abre-se a janela **Live Expressions**.
2. Adicionam-se as variáveis globais de instrumentação:
   - `test_status`: Exibe `TEST_RUNNING (0)`, `TEST_PASSED (1)` ou `TEST_FAILED (2)`.
   - `checksum_calculated`: Valor em ponto flutuante acumulado pelo microcontrolador.
   - `checksum_expected`: Valor de referência derivado do script Python da T1.
   - `current_block_i`, `current_block_j`: Mostram o streaming de blocos acontecendo em tempo real sem interrupção do código.
3. Demonstra-se ao avaliador que os dados processados na placa física batem com rigor numérico com o resultado de referência do Python, comprovando o domínio da arquitetura de memória e das restrições do sistema embarcado.
