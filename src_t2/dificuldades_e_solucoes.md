# 🛠️ Dificuldades de Engenharia, Soluções Arquiteturais e Mapeamento de Código (Tarefa 2)

> **Documento Técnico de Referência — Tarefa 2 (T2)**  
> **Tema:** Desafios de implementação de um co-processador bare-metal para multiplicação matricial ($N=45$, `float32`) em microcontrolador com restrição severa de recursos (NUCLEO-F030R8 / ARM Cortex-M0 / 8 KB SRAM).  
> **Objetivo:** Documentar formalmente os problemas enfrentados, a justificativa teórica das decisões tomadas e a localização exata de cada solução no código-fonte.

---

## 🧭 Índice dos Desafios Técnicos

1. [Desafio 1: Restrição Severa de Memória SRAM (8 KB vs. 24,3 KB Necessários)](#1-desafio-1-restrição-severa-de-memória-sram-8-kb-vs-243-kb-necessários)
2. [Desafio 2: Sobrecarga da CPU e Risco de *Overrun* na UART a 460.800 bps](#2-desafio-2-sobrecarga-da-cpu-e-risco-de-overrun-na-uart-a-460800-bps)
3. [Desafio 3: Ausência de FPU de Hardware no ARM Cortex-M0 e Gargalo de Latência](#3-desafio-3-ausência-de-fpu-de-hardware-no-arm-cortex-m0-e-gargalo-de-latência)
4. [Desafio 4: Dimensões Não-Múltiplas do Bloco ($N = 45$, $B = 8$) e Tratamento de Bordas](#4-desafio-4-dimensões-não-múltiplas-do-bloco-n--45-b--8-e-tratamento-de-bordas)
5. [Desafio 5: Dessincronização e Fragmentação de Pacotes no Canal Serial USB](#5-desafio-5-dessincronização-e-fragmentação-de-pacotes-no-canal-serial-usb)
6. [Desafio 6: Validação Numérica entre Plataformas Cruzadas (x86-64 vs. Cortex-M0)](#6-desafio-6-validação-numérica-entre-plataformas-cruzadas-x86-64-vs-cortex-m0)
7. [Quadro-Resumo de Desafios, Soluções e Arquivos](#7-quadro-resumo-de-desafios-soluções-e-arquivos)

---

## 1. Desafio 1: Restrição Severa de Memória SRAM (8 KB vs. 24,3 KB Necessários)

### 🔴 O Problema
Na multiplicação tradicional de matrizes $45 \times 45$ com floats de 32 bits (IEEE 754):
* Matriz $A$: $45 \times 45 \times 4\text{ B} = 8.100\text{ bytes}$
* Matriz $B$: $45 \times 45 \times 4\text{ B} = 8.100\text{ bytes}$
* Matriz $C$: $45 \times 45 \times 4\text{ B} = 8.100\text{ bytes}$
* **Total Requerido:** $\mathbf{24.300\text{ bytes}}\ (\approx 23{,}7\text{ KB})$.

O microcontrolador **STM32F030R8T6 possui apenas 8.192 bytes (8 KB) de SRAM física**. Armazenar as três matrizes na memória causa **estouro imediato de memória (*Stack/SRAM Overflow*)**, travando a placa no boot.

### 🟢 A Solução: *Tile-Strip Streaming* ($B = 8$)
Em vez de receber matrizes completas, a comunicação e o particionamento operam em regime de streaming de faixas horizontais de $A$ e sub-blocos verticais de $B$:
1. **Buffer de $A$ (`buffer_A`):** Armazena apenas 8 linhas completas de $A$ ($8 \times 45 \times 4\text{ B} = \mathbf{1.440\text{ bytes}}$). Esta faixa é reutilizada 6 vezes na SRAM.
2. **Buffer de $B$ (`buffer_B`):** Recebe sob demanda apenas 8 colunas completas de $B$ ($45 \times 8 \times 4\text{ B} = \mathbf{1.440\text{ bytes}}$).
3. **Buffer de $C$ (`bloco_C_res`):** Acumula apenas o sub-bloco resultante $8 \times 8$ ($8 \times 8 \times 4\text{ B} = \mathbf{256\text{ bytes}}$). Assim que calculado, é despachado pela serial e o buffer é reciclado.
* **Consumo Total de Dados:** $1.440 + 1.440 + 256 = \mathbf{3.136\text{ bytes}}$ (~38% da SRAM), deixando mais de 4,8 KB livres para a pilha (*Stack*) e o sistema.

### 📍 Onde se encontra no código
* **Definições de Tamanho e Orçamento:**  
  [`src_t2/firmware_stm32/Core/Inc/multi_tiling.h`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/multi_tiling.h#L22-L38) — Constantes `N = 45`, `BLOCK = 8` e dedução do orçamento de memória.
* **Alocação Estática dos Buffers:**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L28-L33) — Declaração de `buffer_A`, `buffer_B` e `bloco_C_res`.
* **Fatiamento e Orquestração no Host:**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L65-L100) — Função `processar_matriz_remota()` que fatia a matriz $A$ em blocos de até 8 linhas e transmite sob demanda.

---

## 2. Desafio 2: Sobrecarga da CPU e Risco de *Overrun* na UART a 460.800 bps

### 🔴 O Problema
Para executar os 30 conjuntos de matrizes com rapidez, adotou-se o baud rate de **460.800 bps**.  
Nessa velocidade, um novo byte chega a cada **$\approx 21{,}7\,\mu\text{s}$**.
* Se a recepção fosse tratada por interrupção convencional (`HAL_UART_Receive_IT`), o Cortex-M0 sofreria **1.440 interrupções em ~31 ms** a cada faixa de dados. O custo de salvar registradores na pilha e processar a rotina ISR consumiria uma fração enorme do tempo de CPU.
* Qualquer atraso no tratamento de uma interrupção dispararia o erro de **Overrun (`ORE`)**, travando o hardware da USART2 e abortando o teste.
* No retorno de $C$ para o PC, transmitir por polling bloqueava a CPU por ~5,5 ms por sub-bloco.

### 🟢 A Solução: Desacoplamento via DMA (*Direct Memory Access*)
* A interface física continua sendo a **UART (USART2)**, mas toda a transferência dos blocos em massa de floats é delegada ao controlador **DMA1**:
  * **Recepção (RX):** O canal **DMA1 Canal 5** puxa os bytes do registrador `USART2->RDR` diretamente para a SRAM (`HAL_UART_Receive_DMA()`). A CPU sofre **zero** interrupções durante os 1.440 bytes e é avisada apenas quando o vetor completo estiver disponível na RAM.
  * **Transmissão (TX):** O canal **DMA1 Canal 4** despacha os 256 bytes do sub-bloco de $C$ diretamente da SRAM para `USART2->TDR` (`HAL_UART_Transmit_DMA()`), liberando a CPU instantaneamente.
* Os bytes pontuais de controle (`'P'`, `'A'`, `'B'`, `'K'`) continuam em polling direto por serem de baixa latência e unitários.

### 📍 Onde se encontra no código
* **Configuração dos Handles e Callbacks do DMA:**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L20-L61) — Declaração de `hdma_usart2_rx`, `hdma_usart2_tx`, flags `dma_rx_status`/`dma_tx_status` e callbacks `HAL_UART_RxCpltCallback`/`HAL_UART_TxCpltCallback`.
* **Recepção Acelerada das Faixas de $A$ e Blocos de $B$:**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L106-L120) e [`L132-L137`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L132-L137).
* **Transmissão Desbloqueada de $C$:**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L144-L151).
* **Mapeamento de Hardware e IRQs do DMA:**  
  [`src_t2/firmware_stm32/Core/Src/stm32f0xx_hal_msp.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_hal_msp.c) e [`stm32f0xx_it.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_it.c).

---

## 3. Desafio 3: Ausência de FPU de Hardware no ARM Cortex-M0 e Gargalo de Latência

### 🔴 O Problema
O processador ARM Cortex-M0 é um núcleo RISC minimalista voltado para baixo custo e baixo consumo:
* **Não possui unidade de ponto flutuante em hardware (FPU)**.
* Todas as operações de multiplicação (`*`) e adição (`+`) de `float` (IEEE 754 de 32 bits) são convertidas em dezenas a centenas de instruções inteiras por software via chamadas à biblioteca auxiliar do GCC (`__aeabi_fmul`, `__aeabi_fadd`).
* Operando com o clock interno padrão HSI não calibrado de apenas 8 MHz, o cálculo das multiplicações ficava excessivamente lento.

### 🟢 A Solução: Overclock Seguro para 48 MHz (PLL) + Localidade de Registradores
1. **Configuração de Clock:** O oscilador interno HSI (8 MHz) foi roteado pelo sintetizador PLL com multiplicador $6\times$, elevando o barramento da CPU para **48 MHz** (máximo estável do STM32F030) e ajustando a latência de Flash para 1 wait-state (`FLASH_LATENCY_1`).
2. **Loop Tiling na Dimensão $K$:** No kernel multiplicador, a contração dimensional de 45 termos é fatiada em blocos de 8 elementos. A variável acumuladora `soma` é mantida em um registrador de propósito geral da CPU, minimizando acessos repetidos à SRAM.

### 📍 Onde se encontra no código
* **Configuração de Clock e PLL (48 MHz):**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L164-L190) — Função `SystemClock_Config()`.
* **Acumulador em Registrador e Particionamento Interno:**  
  [`src_t2/firmware_stm32/Core/Src/multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L89-L110) — Laços de multiplicação particionada em $K$ com acumulador `float soma = 0.0f;`.

---

## 4. Desafio 4: Dimensões Não-Múltiplas do Bloco ($N = 45$, $B = 8$) e Tratamento de Bordas

### 🔴 O Problema
O tamanho da matriz $N = 45$ não é divisível pelo tamanho do bloco $B = 8$:
$$\frac{45}{8} = 5 \text{ blocos completos de } 8 + 1 \text{ bloco residual de } 5\quad (45 \pmod 8 = 5)$$
Se os laços assumissem blocos estáticos de $8 \times 8$:
* O firmware tentaria ler e escrever até o índice 48, causando **acesso fora dos limites de memória (*Out-of-Bounds Memory Access*)**, corrompendo a SRAM ou operando sobre dados indefinidos ("lixo de memória").

### 🟢 A Solução: *Edge Tiling* Dinâmico (Tratamento de Borda)
* Tanto no orquestrador Host em Python quanto no firmware bare-metal em C, as dimensões dos blocos são calculadas dinamicamente como:
  $$bi = \min(B, N - ii), \quad bj = \min(B, N - jj)$$
* A matriz gera 36 sub-blocos no total:
  * 25 blocos regulares centrais: $8 \times 8$
  * 5 blocos na borda inferior: $5 \times 8$
  * 5 blocos na borda direita: $8 \times 5$
  * 1 bloco de canto inferior direito: $5 \times 5$
* O firmware ajusta dinamicamente a quantidade de bytes a receber e a calcular com base nos parâmetros `bi` e `bj` recebidos no cabeçalho do comando.

### 📍 Onde se encontra no código
* **Cálculo de Borda no Host (Python):**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L67-L72) e [`L83-L86`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L83-L86).
* **Clamp Defensivo e Controle de Borda no Firmware (C):**  
  [`src_t2/firmware_stm32/Core/Src/multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L75-L81) e [`L99-L101`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L99-L101) — Proteção contra estouro de bloco e cálculo do `k_max = (kk + BLOCK < N) ? (kk + BLOCK) : N`.

---

## 5. Desafio 5: Dessincronização e Fragmentação de Pacotes no Canal Serial USB

### 🔴 O Problema
A porta serial USB virtual (VCP) do sistema operacional pode entregar os dados em fragmentos arbitrários de bytes.
* Se o Host tentar ler 256 bytes de uma vez com `read(256)`, o sistema operacional pode retornar apenas os 64 bytes que já chegaram no buffer USB. Se o script não esperar os bytes restantes, a matriz será montada com valores corrompidos ou zeros.
* Se houver ruído de chaveamento durante a inicialização da porta COM, bytes residuais no buffer da UART fariam o STM32 interpretar dados binários como comandos inválidos.

### 🟢 A Solução: Protocolo Request-Response Estrito, Purga e Leitura Bloqueante Exata
1. **Leitura Exata com Timeout (`ler_exato`):** No Python, implementou-se uma função de leitura determinística que acumula os bytes em loop até atingir exatamente o tamanho esperado ou disparar timeout.
2. **Handshake e Purga de Buffers:** Ao abrir a porta COM, o Host descarta qualquer lixo residual com `reset_input_buffer()` e `reset_output_buffer()`, seguido de um comando de *Ping* (`'P'`) que exige a resposta *Pong* (`'O'`) do STM32 antes de iniciar qualquer teste.
3. **Controle de Fluxo por Confirmação (ACK `'K'`):** Após o envio da faixa horizontal de $A$, o Host bloqueia obrigatoriamente até receber o caractere `'K'`, garantindo que a SRAM do microcontrolador está carregada antes de qualquer bloco de $B$ ser transmitido.
4. **Pacing de Transmissão:** Inserção de micro-delays de 1 ms no Python entre o envio do cabeçalho de controle e o payload volumoso de floats para assegurar o arme do canal DMA no firmware.

### 📍 Onde se encontra no código
* **Função de Leitura Determinística no Host:**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L46-L59) — Função `ler_exato()`.
* **Purga de Buffers e Handshake Inicial:**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L122-L132).
* **Pacing e Envio Estruturado com ACK:**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L74-L82) e [`L89-L95`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L89-L95).
* **Máquina de Estados de Recepção com Timeout no Firmware:**  
  [`src_t2/firmware_stm32/Core/Src/main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L84-L157).

---

## 6. Desafio 6: Validação Numérica entre Plataformas Cruzadas (x86-64 vs. Cortex-M0)

### 🔴 O Problema
Ao multiplicar floats de 32 bits, a ordem das operações de adição e multiplicação afeta os últimos bits da mantissa devido ao truncamento e arredondamento da norma IEEE 754.
* O PC utiliza registradores vetoriais AVX/FMA de 64/128 bits e ordem de acumulação otimizada da biblioteca OpenBLAS/NumPy.
* O STM32 Cortex-M0 utiliza emulação inteira de 32 bits com a ordem estrita dos laços do algoritmo de Tiling particionado.
* Usar uma comparação estrita de igualdade (`A == B`) resultaria em **falsos negativos** em praticamente todas as matrizes devido a diferenças inevitáveis de frações de ULP (*Unit in the Last Place*).

### 🟢 A Solução: Auditoria Metrológica com Limite Teórico de Higham e Tolerância em ULPs
Em vez de tolerâncias arbitrárias, o sistema de teste adota o **limite rigoroso de estabilidade numérica de Higham** (1996) para produto interno em ponto flutuante de $N$ termos:
$$|C_{\text{stm32}} - C_{\text{numpy}}| \le \gamma_N \sum_{k=0}^{N-1} |A_{ik}| |B_{kj}|, \quad \text{onde } \gamma_N = \frac{N \cdot \epsilon}{1 - N \cdot \epsilon} \approx N \cdot 2^{-24}$$
Além disso, calcula-se a distância em ULPs inteiros. Desvios de até 4 ou 5 ULPs são matematicamente normais e esperados para produtos de 45 termos em precisão simples, atestando conformidade plena de 100%.

### 📍 Onde se encontra no código
* **Cálculo de Métricas Metrológicas e Limite de Higham no Host:**  
  [`src_t2/runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L150-L175) — Cálculo de $L_\infty$, MAE, Frobenius e validação metrológica.
* **Caderno de Auditoria Estatística e Gráfica:**  
  [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) — Cartas de controle de processo, gráficos de resíduos e histogramas de ULPs.

---

## 7. Quadro-Resumo de Desafios, Soluções e Arquivos

| Desafio de Engenharia | Sintoma / Risco | Solução Arquitetural Implementada | Arquivos e Linhas Envolvidos |
| :--- | :--- | :--- | :--- |
| **1. Estouro de SRAM (8 KB)** | 24,3 KB exigidos causaria travamento por falta de RAM. | *Tile-Strip Streaming* com $B=8$. RAM total utilizada: apenas **~3,1 KB**. | [`multi_tiling.h`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/multi_tiling.h#L31-L37)<br>[`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L28-L33)<br>[`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L65-L100) |
| **2. Sobrecarga e Overrun na UART** | Interrupções a cada 21,7 µs levariam a perda de dados (`ORE`). | Uso de **DMA1 (Canais 4 e 5)** para transferir floats diretamente para a RAM com 0% de CPU. | [`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L106-L151)<br>[`stm32f0xx_hal_msp.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_hal_msp.c)<br>[`stm32f0xx_it.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_it.c) |
| **3. Ausência de FPU no Cortex-M0** | Ponto flutuante por software a 8 MHz deixaria o cálculo inviável. | PLL configurado para **48 MHz**, 1 ciclo de Flash latency e acumulador em registrador. | [`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L164-L190)<br>[`multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L89-L110) |
| **4. Borda não-múltipla ($N=45$)** | Acessos além da matriz causariam corrupção ou segmentation fault. | Tratamento dinâmico de blocos de borda: $bi, bj \in \{8, 5\}$ e $k_{\max} \le N$. | [`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L67-L72)<br>[`multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L75-L101) |
| **5. Fragmentação Serial USB** | `read()` incompleto gerava matrizes vazias ou corrompidas. | Função `ler_exato()`, purga de buffers, timeout defensivo e sincronismo por ACK (`'K'`). | [`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L46-L59)<br>[`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L84-L157) |
| **6. Arredondamento IEEE 754** | Falsos negativos por divergências na ordem das operações x86 vs ARM. | Auditoria metrológica via **Limite de Higham**, MAE, Frobenius e desvios em ULPs. | [`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L150-L175)<br>[`framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) |
