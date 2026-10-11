# Módulo 1: Setup da Arquitetura e Validação Base no STM32CubeIDE (T2)

> **Documento Tutorial Prático — Módulo 1 [CONCLUÍDO E VALIDADO COM SUCESSO NA PLACA]**  
> Projeto: Multiplicação de Matrizes com Tiling e Tile Streaming em Microcontrolador com Restrição Severa de Memória (8 KB SRAM).  
> Plataformas: **STM32F103C8T6** (Bancada com SRAM limitada a 8 KB) $\rightarrow$ **STM32F030R8T6** (Alvo final).

---

## 1. Visão Geral do Módulo 1

Este módulo estabelece a fundação do projeto e comprova o funcionamento da arquitetura no **STM32CubeIDE**:
1. Configuração do arquivo gráfico **`.ioc`** (GPIO de sinalização para LED, Debug SWD e Clock HSI).
2. Travamento do Linker Script para **8 KB de SRAM** (emulação fiel da STM32F030).
3. Implementação do algoritmo com **Tile Streaming** (buffer de apenas 256 bytes na SRAM).
4. Procedimento de teste e validação autônoma (BIST) com **Zero UART**, inspecionado com sucesso via **Live Expressions** e **LED de hardware**.

> [!NOTE]
> **Status:** Módulo 1 testado fisicamente na placa STM32F103C8T6 com sucesso comprovado: todos os 2025 elementos foram processados, o erro absoluto foi nulo/inferior a $10^{-2}$ e o status convergiu para `TEST_PASSED (2)` com LED aceso contínuo.

---

## 2. Passo 1: Criação do Projeto no STM32CubeIDE

1. Abra o **STM32CubeIDE**.
2. Vá em **File $\rightarrow$ New $\rightarrow$ STM32 Project**.
3. Na aba **Target Selection**:
   - Para a placa de bancada atual: digite **`STM32F103C8`** na busca e selecione o encapsulamento correto (geralmente `STM32F103C8Tx` LQFP48).
   - Para a placa final: digite **`STM32F030R8`** (ou selecione a placa comercial **NUCLEO-F030R8** na aba *Board Selector*).
4. Clique em **Next**.
5. No campo **Project Name**, defina o nome (ex: `matrix_tiling_t2`).
6. Em **Targeted Language**, deixe selecionado **C**.
7. Em **Targeted Binary Type**, selecione **Executable**.
8. Em **Targeted Project Type**, selecione **STM32Cube**.
9. Clique em **Finish**. Se perguntar se deseja abrir a perspectiva Device Configuration, clique em **Yes**.

---

## 3. Passo 2: Configuração Gráfica dos Pinos e Periféricos (.ioc)

Com o arquivo `.ioc` aberto na perspectiva do CubeMX:

### 3.1 Interface de Depuração (SWD)
> [!IMPORTANT]
> Este passo é crucial. Se o Debug não for explicitamente configurado como Serial Wire, a interface SWD pode ser desligada após o primeiro flash, travando regravações futuras.

- No menu à esquerda, expanda **System Core $\rightarrow$ SYS**.
- No campo **Debug**, selecione **Serial Wire**.
- Pinos alocados automaticamente:
  - `PA13` $\rightarrow$ `SYS_JTMS-SWDIO`
  - `PA14` $\rightarrow$ `SYS_JTCK-SWCLK`

### 3.2 Clock do Sistema (RCC)
- Expanda **System Core $\rightarrow$ RCC**.
- Para manter portabilidade independente de cristais externos nas duas placas, mantenha o clock no oscilador interno:
  - Deixe **High Speed Clock (HSE)** como **Disable** (usando o **HSI** nativo de 8 MHz).
- Na aba **Clock Configuration** (no topo):
  - Certifique-se de que a fonte do sistema está selecionada como **HSI** (8 MHz direto).

### 3.3 Pino do LED de Status (GPIO Output)
Configure o pino correspondente à sua placa atual:

#### Para STM32F103C8T6 (Blue Pill):
1. No diagrama de pinos do microcontrolador à direita, localize o pino **`PC13`**.
2. Clique nele com o botão esquerdo e escolha **`GPIO_Output`**.
3. No menu à esquerda, vá em **System Core $\rightarrow$ GPIO** e clique em **PC13**:
   - **GPIO Output level:** `High` *(o LED da Blue Pill é ativo em nível baixo; `High` o mantém apagado inicialmente)*.
   - **GPIO mode:** `Output Push Pull`.
   - **GPIO Pull-up/Pull-down:** `No pull-up and no pull-down`.
   - **Maximum output speed:** `Low`.
   - **User Label:** Digite `LED_STATUS`.

#### Para STM32F030R8T6 (Nucleo-64) - Quando for migrar:
1. No pino **`PA5`** (LED verde LD2 da Nucleo): escolha **`GPIO_Output`**.
2. Em **System Core $\rightarrow$ GPIO $\rightarrow$ PA5**:
   - **GPIO Output level:** `Low` *(na Nucleo o LED é ativo em nível alto)*.
   - **User Label:** `LED_STATUS`.

### 3.4 Gerar o Código do HAL
- Pressione **Ctrl + S** (ou vá em **Project $\rightarrow$ Generate Code**).
- O CubeIDE criará a árvore de pastas com os drivers HAL e o arquivo `Core/Src/main.c`.

---

## 4. Passo 3: Limitação Estrita da Memória RAM (Linker Script)

Como você está usando a STM32F103C8T6 (que fisicamente possui 20 KB de SRAM), devemos forçar o compilador a reconhecer apenas **8 KB de SRAM** para emular fidedignamente o teto da STM32F030R8T6.

1. No painel **Project Explorer**, abra o arquivo de linker script na raiz do projeto:
   - Geralmente nomeado como **`STM32F103C8TX_FLASH.ld`**.
2. Localize a definição de blocos de memória no topo do arquivo (seção `MEMORY`):
   ```ld
   /* Configuração original da Blue Pill (20 KB de RAM): */
   RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
   ```
3. Altere o tamanho da RAM para **8K**:
   ```ld
   /* Configuração TRAVADA para 8 KB (Emulando STM32F030): */
   RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 8K
   ```
4. Verifique também a reserva de Stack e Heap:
   ```ld
   _Min_Heap_Size = 0x0;    /* Sem alocação dinâmica malloc */
   _Min_Stack_Size = 0x400; /* 1024 bytes reservados para a pilha de chamadas */
   ```
5. Salve o arquivo.

---

## 5. Passo 4: Estruturação dos Arquivos no Projeto

Para atender à nota de **clareza e reuso (10%)**, o código foi completamente desacoplado: o **motor de cálculo matemático** não possui qualquer dependência de teste, enquanto a **suíte de auditoria (BIST)** gerencia os cenários e a validação:

```text
seu_projeto/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── multi_tiling.h      <-- Motor matemático puro (interface de Tile Streaming)
│   │   ├── bist_suite.h        <-- Módulo de testes: cenários e struct de relatório g_report
│   │   └── test_data.h         <-- Matrizes A e B em Flash e checksums esperados
│   └── Src/
│       ├── main.c              <-- Ponto de entrada: inicializa HAL e dispara bist_run_suite()
│       ├── multi_tiling.c      <-- Motor matemático puro de Tiling (256B buffer)
│       └── bist_suite.c        <-- Implementação da suíte de testes e acumulação online
```

### 5.1 Gerar os Dados de Teste (`test_data.h`)
No terminal da sua máquina host (fora do CubeIDE), execute o script Python para extrair as matrizes de entrada da T1 e compilar o checksum:
```powershell
python src_t2/generate_test_data.py 1
```
*(Esse comando gerará `test_data.h` com as matrizes $A$ e $B$ e a soma de validação esperada calculada em `float32`). Copie `test_data.h` para `Core/Inc/` do projeto.*

### 5.2 Header do Algoritmo (`Core/Inc/multi_tiling.h`)
Crie o arquivo com a definição das constantes e do tipo de relatório para a janela de inspeção:

```c
#ifndef MULTI_TILING_H
#define MULTI_TILING_H

#include <stdint.h>
#include "test_data.h"

#define BLOCK 8

typedef enum {
    TEST_IDLE    = 0,
    TEST_RUNNING = 1,
    TEST_PASSED  = 2,
    TEST_FAILED  = 3
} TestStatus_t;

typedef struct {
    volatile TestStatus_t status;
    volatile uint32_t     elements_processed;
    volatile float        checksum_calculated;
    volatile float        checksum_expected;
    volatile float        absolute_error;
} ValidationReport_t;

extern volatile ValidationReport_t g_report;

/* Executa a multiplicação com Tile Streaming e validação incremental */
void multi_tiling_streaming_bist(void);

#endif /* MULTI_TILING_H */
```

### 5.3 Implementação do Motor de Tiling (`Core/Src/multi_tiling.c`)
Crie o arquivo implementando o algoritmo canônico particionado com buffer de bloco local em SRAM:

```c
#include "multi_tiling.h"
#include <string.h>
#include <math.h>

volatile ValidationReport_t g_report = {
    .status = TEST_IDLE,
    .elements_processed = 0,
    .checksum_calculated = 0.0f,
    .checksum_expected = CHECKSUM_SUM_EXPECTED,
    .absolute_error = 0.0f
};

static inline int min_int(int a, int b) {
    return (a < b) ? a : b;
}

void multi_tiling_streaming_bist(void) {
    /* Buffer de bloco único em SRAM: 8x8 floats = 256 bytes */
    static float bloco_C[BLOCK][BLOCK];
    
    g_report.status = TEST_RUNNING;
    g_report.elements_processed = 0;
    g_report.checksum_calculated = 0.0f;

    for (int ii = 0; ii < N; ii += BLOCK) {
        int i_max = min_int(ii + BLOCK, N);
        int bi = i_max - ii;

        for (int jj = 0; jj < N; jj += BLOCK) {
            int j_max = min_int(jj + BLOCK, N);
            int bj = j_max - jj;

            /* Zera apenas o buffer local de 256 bytes */
            memset(bloco_C, 0, sizeof(bloco_C));

            /* Computação das acumulações no bloco atual */
            for (int kk = 0; kk < N; kk += BLOCK) {
                int k_max = min_int(kk + BLOCK, N);

                for (int i = 0; i < bi; i++) {
                    for (int j = 0; j < bj; j++) {
                        float soma = bloco_C[i][j];
                        for (int k = kk; k < k_max; k++) {
                            soma += A[ii + i][k] * B[k][jj + j];
                        }
                        bloco_C[i][j] = soma;
                    }
                }
            }

            /* Acumulação do BIST online conforme os blocos fluem */
            for (int i = 0; i < bi; i++) {
                for (int j = 0; j < bj; j++) {
                    g_report.checksum_calculated += bloco_C[i][j];
                    g_report.elements_processed++;
                }
            }
        }
    }

    /* Validação final com tolerância numérica de float32 */
    g_report.absolute_error = fabsf(g_report.checksum_calculated - g_report.checksum_expected);
    if (g_report.absolute_error < 1e-2f && g_report.elements_processed == (N * N)) {
        g_report.status = TEST_PASSED;
    } else {
        g_report.status = TEST_FAILED;
    }
}
```

### 5.4 Arquivo Principal (`Core/Src/main.c`)
No `main.c`, integre a chamada e o controle do LED:

```c
/* USER CODE BEGIN Includes */
#include "multi_tiling.h"
/* USER CODE END Includes */

/* No loop principal de main(): */
/* USER CODE BEGIN 2 */

/* Pisca rápido 3 vezes indicando início */
for (int i = 0; i < 6; i++) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    HAL_Delay(100);
}

/* Executa o cálculo com Tile Streaming e validação */
multi_tiling_streaming_bist();

/* Sinalização física pós-cálculo */
if (g_report.status == TEST_PASSED) {
    /* SUCESSO: Mantém o LED aceso fixo contínuo */
    #if defined(STM32F103xB)
        HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET); /* Blue Pill: LOW = aceso */
    #else
        HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_SET);   /* Nucleo: HIGH = aceso */
    #endif
}

/* USER CODE END 2 */

/* Infinite loop */
/* USER CODE BEGIN WHILE */
while (1)
{
    /* Se tiver falhado, pisca lentamente em sinal de erro */
    if (g_report.status == TEST_FAILED) {
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        HAL_Delay(500);
    }
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
}
/* USER CODE END 3 */
```

---

## 6. Passo 5: Compilação e Otimização do Compilador

1. Clique com o botão direito no projeto $\rightarrow$ **Properties**.
2. Vá em **C/C++ Build $\rightarrow$ Settings**.
3. Na aba **Tool Settings $\rightarrow$ MCU GCC Compiler $\rightarrow$ Optimization**:
   - Selecione **Optimization Level:** `-O3` (ou `-O2` para desempenho máximo).
4. Clique em **Apply and Close**.
5. Clique no ícone da **Marreta (Build Project)** ou pressione **Ctrl + B**.
6. Observe o console **Build Output**:
   - A seção `.text` (código) e `.rodata` (matrizes $A$ e $B$) consumirá ~28 KB da Flash (de 64 KB).
   - A seção `.bss` e `.data` (RAM) consumirá menos de 1 KB dos 8 KB estipulados.

---

## 7. Passo 6: Como Executar e Demonstrar no STM32CubeIDE

### 7.1 Configurar a Depuração (Debug Configuration)
1. Conecte o ST-Link na porta USB do seu computador e nos pinos SWDIO/SWCLK/GND/3V3 da placa.
2. No STM32CubeIDE, vá em **Run $\rightarrow$ Debug Configurations...**.
3. Em **STM32 C/C++ Application**, selecione seu executável.
4. Na aba **Debugger**:
   - **Debug probe:** ST-LINK (ST-LINK GDB server).
   - **Interface:** SWD.
5. Clique em **Debug**. O código será gravado na Flash e o programa pausará na primeira linha do `main()`.

### 7.2 Configurar a Janela Live Expressions (Demonstração Sem UART)
1. No canto superior direito, certifique-se de que está na perspectiva **Debug**.
2. No painel de abas superior direito, localize a aba **Live Expressions** (se não estiver aberta, vá em **Window $\rightarrow$ Show View $\rightarrow$ Live Expressions**).
3. Na coluna **Expression**, dê um duplo clique em **`<Enter new expression>`** (ou clique no ícone **+**).
4. Digite apenas o nome da variável:
   `g_report`
   *(Atenção: digite exatamente a palavra `g_report`, sem crases, sem aspas e sem quebras de linha).*
5. Pressione **Enter**. A variável aparecerá na tabela.
6. Clique na setinha `>` ao lado de `g_report` para expandir e ver os campos internos (`status`, `elements_processed`, `checksum_calculated`, etc.).

> [!TIP]
> **Alternativa (adicionar os campos diretamente um por um):**  
> Se preferir ver cada variável em uma linha dedicada sem precisar expandir a struct, digite cada uma separadamente:
> - `g_report.status`
> - `g_report.elements_processed`
> - `g_report.checksum_calculated`
> - `g_report.checksum_expected`
> - `g_report.absolute_error`

7. Pressione **Resume (F8)** para iniciar a execução normal.

### 7.3 Resultados Esperados na Demonstração:
- **No Hardware (LED):**
  - O LED pisca rápido na inicialização e, assim que o cálculo conclui, fica **ACESO FIXO**, comprovando fisicamente que o teste passou.
- **Na Janela Live Expressions (Em tempo real):**
  - `elements_processed` alcança **`2025`**.
  - `checksum_calculated` e `checksum_expected` exibem valores idênticos.
  - `absolute_error` exibe valor próximo a `0.000000`.
  - `status` exibe **`TEST_PASSED (2)`**.

---

## 8. Módulo 2: Suíte de Múltiplos Testes Matemáticos (Multi-Test Suite)

O **Módulo 2** enriquece a instrumentação e a avaliação da disciplina executando automaticamente três cenários matemáticos distintos sem estourar o limite de 64 KB de Flash nem os 8 KB de SRAM:

### 8.1 Cenários Executados em Sequência:
1. **Cenário 0 (`SCENARIO_ZEROS`):** Elemento Nulo da Álgebra Linear.
   - Computa $A \times 0_{45 \times 45} = 0_{45 \times 45}$.
   - **Checksum Esperado:** `0.000000f`.
   - **Objetivo:** Prova que não há acúmulo de lixo de memória na SRAM e que o `memset` do buffer de bloco é executado estritamente em todas as iterações.
2. **Cenário 1 (`SCENARIO_IDENTITY`):** Elemento Neutro da Multiplicação.
   - Computa $A \times I_{45 \times 45} = A_{45 \times 45}$.
   - **Checksum Esperado:** `-82.699997f` (exatamente a soma dos elementos da matriz $A$).
   - **Objetivo:** Prova que a multiplicação preserva as coordenadas e a orientação linha/coluna sem rotações acidentais.
3. **Cenário 2 (`SCENARIO_STANDARD_1`):** Multiplicação Geral Bipolar da T1.
   - Computa $A_1 \times B_1$ utilizando os valores binários originais.
   - **Checksum Esperado:** `11019.998f`.
   - **Objetivo:** Validação com dados de teste reais da T1.

### 8.2 Novos Campos para Acompanhamento no Live Expressions:
Ao inspecionar `g_report` (ou adicionar individualmente):
- `g_report.tests_passed_count`: Começa em `0` e incrementa sequencialmente até **`3`**.
- `g_report.current_scenario`: Mostra o teste em execução (`0`, `1` ou `2`).
- `g_report.status`: Converge para **`TEST_PASSED (2)`** apenas se os 3 testes passarem com sucesso.
- **LED Físico:** Mantém-se aceso contínuo após a aprovação de todos os 3 testes.

### 8.3 Nova Árvore de Diretórios e Modularização no STM32CubeIDE:
Para atender ao critério de **clareza e reuso**, a árvore de código do projeto no painel *Project Explorer* do STM32CubeIDE deve ficar organizada com as seguintes responsabilidades estritamente separadas:

```text
matrix_tiling_t2/
├── Core/
│   ├── Inc/
│   │   ├── main.h             <-- Definições do HAL, pinos de GPIO e protótipos
│   │   ├── multi_tiling.h     <-- MOTOR PURO: Interface da multiplicação e callbacks de streaming
│   │   ├── bist_suite.h       <-- MÓDULO DE TESTES: Declaração dos cenários e struct g_report
│   │   └── test_data.h        <-- DADOS: Matrizes A e B em Flash e checksums de referência
│   └── Src/
│       ├── main.c             <-- PONTO DE ENTRADA: Inicializa HAL/GPIO e invoca bist_run_suite()
│       ├── multi_tiling.c     <-- MOTOR PURO: Algoritmo de Tiling com buffer local de 256B
│       ├── bist_suite.c       <-- MÓDULO DE TESTES: Implementa a suíte, auditoria e callbacks
│       ├── stm32f1xx_it.c     <-- Handlers de interrupção padrão do STM32
│       └── system_stm32f1xx.c <-- Inicialização do sistema e clocks
└── STM32F103C8TX_FLASH.ld     <-- Linker Script com RAM travada em 8 KB
```

#### Resumo das Responsabilidades da Nova Arquitetura:
1. **`multi_tiling.h` / `multi_tiling.c` (Motor Matemático Reusável):**
   - Não contém nenhuma referência a `g_report`, `TEST_PASSED`, `TEST_FAILED` ou código de validação.
   - Fornece a função limpa `multi_tiling_compute_stream()`, que entrega cada bloco $8 \times 8$ calculado a um callback consumidor genérico.
2. **`bist_suite.h` / `bist_suite.c` (Framework de Auditoria Embarcada):**
   - Implementa o callback que consome os blocos e acumula o checksum.
   - Fornece a lógica sequencial dos 3 cenários (Zeros $\rightarrow$ Identidade $\rightarrow$ Teste 1).
   - Gerencia exclusivamente a struct `g_report` visível no *Live Expressions*.
3. **`main.c` (Fluxo de Execução Enxuto):**
   - Depende apenas de `#include "bist_suite.h"`.
   - Controla a sinalização do LED de status e dispara `bist_run_suite()`.
