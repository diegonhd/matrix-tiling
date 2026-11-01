# Tarefa 2 (T2) — Hierarquia e Funções dos Arquivos Complementares
## Acelerador Matricial Serial Hardware-in-the-Loop (STM32 NUCLEO-F030R8)

---

## 1. Visão Geral da Arquitetura do Projeto

Este projeto implementa um co-processador matemático autônomo conectado ao computador via porta serial USB (Virtual COM Port a 460.800 bps acelerado por DMA). Como o microcontrolador dispõe de apenas **8 KB de SRAM física** e as matrizes completas $45 \times 45$ de floats exigiriam $24{,}3\text{ KB}$, o software foi modularizado em uma arquitetura de **Streaming**:

* **Host PC (Python):** Fatia a matriz $A$ em faixas horizontais de até 8 linhas e transmite via serial; em seguida, envia blocos verticais de $B$ de até 8 colunas e recebe os sub-blocos calculados de $C$.
* **Firmware (STM32 / C):** Mantém apenas uma faixa horizontal de $A$ na SRAM ($1.440\text{ bytes}$), recebe blocos de $B$ via DMA1 Canal 5, computa o sub-bloco $C$ em um buffer compacto de apenas $256\text{ bytes}$ ($64\text{ floats}$) e devolve o resultado via DMA1 Canal 4.

---


## 2. Função e Responsabilidade de Cada Arquivo

Abaixo detalha-se o papel de cada arquivo complementar dentro do sistema integrado:

### 2.1 Camada de Firmware Embarcado (C / HAL para STM32F030R8)

#### 1. `main.c` (Ponto de Entrada e Orquestração do Firmware)
* **Papel no Projeto:** Centraliza a inicialização de hardware do microcontrolador e o loop principal de atendimento a eventos seriais.
* **Responsabilidades:**
  - Inicializa o clock do sistema em 48 MHz (HSI + PLL), o periférico USART2 a 460.800 bps e os canais DMA1 Canal 5 (RX) e Canal 4 (TX).
  - Executa a sinalização visual de boot no LED LD2 (PA5) piscando 3 vezes na inicialização.
  - Implementa a máquina de estados síncrona: responde a `CMD_PING` ('P'), recebe a faixa de $A$ via DMA sob `CMD_STRIP_A` ('A'), e recebe o bloco de $B$, dispara o cálculo de $C$ e transmite os floats calculados via DMA sob `CMD_BLOCK_B` ('B').
  - Trata os callbacks de interrupção de hardware: `HAL_UART_RxCpltCallback`, `HAL_UART_TxCpltCallback` e `HAL_UART_ErrorCallback`.
* **Alocação de Memória:** Gerencia os buffers estáticos `buffer_A` ($1.440\text{ B}$), `buffer_B` ($1.440\text{ B}$) e `bloco_C_res` ($256\text{ B}$).
* **Relação com os outros arquivos:** Consome as definições de pinos de `main.h`, os opcodes de `protocolo.h` e aciona o motor de cálculo de `multi_tiling.h`.
* **Origem:** Código gerado pelo STM32CubeMX (funções de periféricos `MX_*`) combinado com lógica de controle própria do autor.

#### 2. `main.h` (Mapeamento de Hardware da Placa)
* **Papel no Projeto:** Centraliza as constantes simbólicas de hardware da placa NUCLEO-F030R8.
* **Responsabilidades:**
  - Mapeia os pinos físicos: `USART_TX_Pin` (PA2), `USART_RX_Pin` (PA3), `LD2_Pin` (PA5 — LED Verde de status) e `B1_Pin` (PC13 — Botão Azul de usuário).
  - Define os níveis lógicos de acionamento do LED (`LED_ON_STATE` e `LED_OFF_STATE`).
  - Estabelece as constantes de temporização visual do boot (`DELAY_PISCA_BOOT_MS` e `QTD_TRANSIÇÕES_BOOT_LED`).
  - Declara o protótipo da rotina de contenção de falhas críticas `Error_Handler()`.
* **Relação com os outros arquivos:** Incluído por `main.c` e pelos módulos de interrupção da placa.
* **Origem:** Base gerada pelo STM32CubeMX com customizações e mapeamento de telemetria do autor.

#### 3. `multi_tiling.h` (Interface do Motor Computacional Matricial)
* **Papel no Projeto:** Declaração das dimensões fundamentais, orçamentos de memória e protótipos das rotinas de álgebra linear particionada.
* **Responsabilidades:**
  - Define as constantes nomeadas do algoritmo: `DIM_MATRIZ_N` ($45$), `TAM_BLOCO_TILES` ($8$), `QTD_ELEMENTOS_BLOCO_C` ($64\text{ elementos}$), `TAM_BYTES_BLOCO_C` ($256\text{ bytes}$), `TAM_MAX_BYTES_FAIXA_A` ($1.440\text{ bytes}$) e `TAM_MAX_BYTES_BLOCO_B` ($1.440\text{ bytes}$).
  - Define os protótipos de `multi_tiling_set_faixa_A()` e `multi_tiling_computar_bloco()`.
  - Documenta rigorosamente as restrições de buffers (alinhamento de 4 bytes, não-nulos) e alertas explícitos de **sobrescrita de dados**.
* **Relação com os outros arquivos:** Fornece a interface matemática invocada por `main.c` e implementada em `multi_tiling.c`.
* **Origem:** Código próprio de adaptação do algoritmo de Loop Tiling clássico para microcontrolador com memória restrita.

#### 4. `multi_tiling.c` (Implementação do Loop Tiling em Streaming)
* **Papel no Projeto:** Núcleo de execução do produto matricial particionado $C[bi][bj] = A_{faixa}[bi][:] \times B_{bloco}[:][bj]$.
* **Responsabilidades:**
  - Armazena a faixa horizontal ativa de $A$ no buffer estático interno `faixa_A[8][45]` na seção `.bss` da SRAM ($1.440\text{ bytes}$).
  - Implementa a função `multi_tiling_set_faixa_A()` com proteção defensiva contra estouro de bordas.
  - Implementa `multi_tiling_computar_bloco()` empregando particionamento Tiling na dimensão interna $K$ (laço $kk$ com passo 8) e tratamento do bloco residual de borda ($45 \pmod 8 = 5$ elementos).
  - Mantém o acumulador escalar em registrador de 32 bits da CPU Cortex-M0, otimizando o processamento emulatório de ponto flutuante (*soft-float*).
* **Relação com os outros arquivos:** Implementa a interface de `multi_tiling.h`, sendo acionado pelo laço de eventos de `main.c`.
* **Origem:** Algoritmo clássico de Loop Tiling / Blocking (Lam et al., ASPLOS IV 1991; Hennessy & Patterson 2017) adaptado pelo autor para streaming na SRAM do STM32.

#### 5. `protocolo.h` (Interface do Protocolo de Comunicação Embarcado)
* **Papel no Projeto:** Especificação das constantes de protocolo, caracteres de controle e prazos limites (timeouts).
* **Responsabilidades:**
  - Define os comandos de 1 byte enviados pelo Host: `CMD_PING` ('P'), `CMD_STRIP_A` ('A') e `CMD_BLOCK_B` ('B').
  - Define as respostas enviadas pela placa: `RESP_OK` ('O'), `RESP_ACK` ('K') e `RESP_ERROR` ('E').
  - Define as constantes temporais e metrológicas: `PROTOCOLO_BAUD_RATE_NOMINAL` ($460.800\text{ bps}$), `PROTOCOLO_TIMEOUT_DMA_TRANSFER_MS` ($3.000\text{ ms}$), `PROTOCOLO_TIMEOUT_PARAMETRO_MS` ($1.000\text{ ms}$) e `PROTOCOLO_QTD_TESTES_AUDITORIA` ($30\text{ testes}$).
  - Define o enum `DmaTransferStatus_t` para controle dos estados de sincronismo das transações DMA.
* **Relação com os outros arquivos:** Incluído por `main.c` e `protocolo.c`; espelha exatamente os mesmos valores usados pelo Host em `protocolo_serial.py`.
* **Origem:** Código próprio desenvolvido para a Tarefa 2.

#### 6. `protocolo.c` (Rotinas de Apoio e Validação do Protocolo)
* **Papel no Projeto:** Funções utilitárias defensivas para inspeção e depuração do canal serial.
* **Responsabilidades:**
  - `protocolo_eh_comando_valido()`: Valida se um byte recebido corresponde a um comando homologado ('P', 'A' ou 'B'), filtrando ruídos de linha.
  - `protocolo_obter_nome_comando()`: Retorna o nome mnemônico constante em Flash ROM para mensagens de depuração/telemetria.
* **Relação com os outros arquivos:** Implementa os protótipos de `protocolo.h`.
* **Origem:** Código próprio.

---

### 2.2 Camada de Controle e Auditoria no Host PC (Python 3)

#### 7. `runner_serial.py` (Orquestrador Mestre de Testes e Auditoria HIL)
* **Papel no Projeto:** Programa principal do Host PC responsável por conduzir toda a bateria de ensaios estatísticos em bancada.
* **Responsabilidades:**
  - Realiza a auto-detecção da porta serial COM conectada ao ST-LINK (`auto_detectar_porta_stlink()`).
  - Estabelece a conexão e envia o ping de handshake validando a prontidão da placa física.
  - Carrega sequencialmente os 30 pares de matrizes de teste (`A_k.bin` e `B_k.bin`) gerados na Tarefa 1 em formato `float32`.
  - Executa a rotina `processar_matriz_remota()`: divide a matriz $A$ em fatias de até 8 linhas, transmite ao STM32, solicita os blocos verticais de $B$, recebe os sub-blocos calculados e reconstrói a matriz global $C_{stm32}$.
  - Computa a referência matemática formal no NumPy ($C_{py} = A \times B$).
  - Calcula as métricas de auditoria metrológica: maior erro absoluto ($L_\infty = \max |C_{stm32} - C_{py}|$) e o desvio em ULPs (*Units in the Last Place*) segundo a norma IEEE 754-2019 e a formulação de David Goldberg (1991).
  - Exporta o laudo metrológico tabular consolidado em `src_t2/audit/auditoria_serial_30_testes.csv`.
* **Relação com os outros arquivos:** Importa as definições de `protocolo_serial.py` e comunica-se bidirecionalmente via UART/DMA com o firmware `main.c`.
* **Origem:** Código próprio.

#### 8. `protocolo_serial.py` (Módulo de Transporte e Constantes do Host)
* **Papel no Projeto:** Camada de compatibilidade e abstração dos comandos seriais no ambiente Python.
* **Responsabilidades:**
  - Define as mesmas constantes geométricas, comandos e respostas do firmware C: `N = 45`, `BLOCK = 8`, `CMD_PING = b'P'`, `CMD_STRIP_A = b'A'`, `CMD_BLOCK_B = b'B'`, `RESP_OK = b'O'`, `RESP_ACK = b'K'`.
  - Define prazos limites de timeout (`TIMEOUT_LEITURA_PADRAO_S = 2.0`, `TIMEOUT_PING_S = 3.0`) e intervalos de estabilização (`DELAY_ENTRE_PACOTES_S = 0.001`).
  - Implementa funções de conferência: `eh_comando_valido()` e `obter_nome_comando()`.
* **Relação com os outros arquivos:** Importado por `runner_serial.py` para garantir conformidade estrita com o protocolo de `protocolo.h`.
* **Origem:** Código próprio.

---

## 4. Diagrama de Integração e Fluxo entre os Arquivos

O fluxo síncrono de dados e comandos percorre os arquivos do sistema na seguinte ordem:

```text
[HOST PC — Python 3]                                [MICROCONTROLADOR STM32F030R8 — C]
┌───────────────────────────────────────┐            ┌────────────────────────────────────────┐
│ runner_serial.py                      │            │ main.c                                 │
│                                       │            │                                        │
│ 1. Importa constantes de:             │            │ 1. Inclui hardware e protocolos de:    │
│    protocolo_serial.py                │            │    main.h, protocolo.h                 │
│                                       │            │                                        │
│ 2. Envia CMD_PING ('P') ─────────────►│ USART2 VCP │ 2. Decodifica opcode via:              │
│    Recebe RESP_OK ('O') ◄─────────────┤ (460.8 kbps│    protocolo.c                         │
│                                       │            │    Devolve RESP_OK ('O')               │
│                                       │            │                                        │
│ 3. Envia CMD_STRIP_A + bi + faixa_A ──►  (DMA1 C5) │ 3. Recebe via DMA em buffer_A          │
│    Recebe RESP_ACK ('K') ◄────────────┤            │    Copia para SRAM interna via:        │
│                                       │            │    multi_tiling_set_faixa_A()          │
│                                       │            │    (em multi_tiling.c)                 │
│                                       │            │                                        │
│ 4. Envia CMD_BLOCK_B + bj + bloco_B ──►  (DMA1 C5) │ 4. Recebe via DMA em buffer_B          │
│                                       │            │    Computa sub-bloco C via:            │
│                                       │            │    multi_tiling_computar_bloco()       │
│                                       │            │    (em multi_tiling.c, 256 B de SRAM)  │
│                                       │            │                                        │
│ 5. Recebe sub-bloco C (floats) ◄──────┤  (DMA1 C4) │ 5. Transmite bloco_C_res via DMA       │
│    Reconstrói C global                │            │                                        │
│                                       │            │                                        │
│ 6. Audita vs NumPy (C_py)             │            └────────────────────────────────────────┘
│    Gera auditoria_serial_30_testes.csv│
└───────────────────────────────────────┘
```

---

## 5. Como Executar o Projeto (Guia Rápido)

1. **Gravar o Firmware:** Abra a pasta `firmware_stm32` no STM32CubeIDE e efetue a gravação na placa NUCLEO-F030R8 via cabo USB. O LED verde LD2 piscará 3 vezes indicando prontidão.
2. **Executar a Auditoria:** No terminal do computador, execute:
   ```bash
   python runner_serial.py
   ```
   O script detectará a porta COM automaticamente, executará os 30 testes consecutivos, validará os resultados contra o NumPy e exibirá o laudo metrológico consolidado no terminal.
