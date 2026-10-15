# 📘 Tutorial Completo: Configuração de Pinos, USART DMA, Firmware e Worktree na Nova Arquitetura Serial (Tarefa 2)

> **Documento Tutorial de Engenharia — Tarefa 2 (T2)**  
> **Modelo:** Co-processador Acelerador Bare-Metal com Streaming Serial Bidirecional em Tempo Real (*Hardware-in-the-Loop* / HIL)  
> **Plataforma Alvo:** NUCLEO-F030R8 (ARM Cortex-M0, 8 KB SRAM nativa, 64 KB Flash)  
> **Canal Serial:** USART2 via ST-LINK Virtual COM Port (VCP) no cabo USB  
> **Ambiente:** STM32CubeIDE v1.14+ / v1.18+ (Firmware C) e VS Code (Python 3 / Jupyter)

---

## 1. Visão Geral da Nova Arquitetura de Testes

Nesta nova arquitetura, o microcontrolador STM32 **não armazena dados fixos em Flash nem executa um teste estático isolado**. A placa atua como um **co-processador acelerador em streaming contínuo**:

1. **O PC (Host)** envia sequencialmente lotes de matrizes de entrada ($A_k$ e $B_k$, $45 \times 45$, `float32`) pela porta serial USB.
2. **O STM32** recebe as faixas horizontais de $A$ e blocos de $B$ via **USART com DMA**, computa os blocos de $C$ em tempo real com **Tile Streaming ($B=8$)** dentro dos **8 KB de SRAM**, e devolve cada bloco resultante imediatamente pela serial.
3. **O PC** reconstrói a matriz $C_k$, valida matematicamente contra o NumPy e emite o laudo de conformidade para **todos os 30 casos estatísticos + casos unitários** em sequência contínua de poucos segundos.

---

## 2. Mapa de Pinagem Físico (NUCLEO-F030R8)

A NUCLEO-F030R8 não necessita de nenhuma fiação externa ou protoboard: **toda a comunicação serial, gravação e depuração trafega pelo próprio cabo USB** através do chip ST-LINK on-board.

| Pino no Chip | Periférico / Função | Rótulo no CubeMX | Destino Físico na Nucleo | Comportamento Operacional |
| :---: | :---: | :---: | :---: | :--- |
| **`PA2`** | **USART2_TX** | `USART_TX` | Conectado internamente ao RX do ST-LINK VCP | Transmite blocos de $C$ do STM32 para o PC |
| **`PA3`** | **USART2_RX** | `USART_RX` | Conectado internamente ao TX do ST-LINK VCP | Recebe faixas de $A$ e blocos de $B$ do PC |
| **`PA5`** | **GPIO_Output** | `LD2` / `LED_STATUS` | LED verde de usuário integrado (LD2) | **Pisca rápido** em tráfego serial, **Fixo** em repouso |
| **`PC13`** | **GPIO_Input** | `B1` | Botão azul de pressão integrado (B1 USER) | Botão de intervenção manual / reinicialização |
| **`PA13`** | **SYS_JTMS-SWDIO** | `SWDIO` | Conector SWD interno do ST-LINK | Dados de gravação e depuração Serial Wire |
| **`PA14`** | **SYS_JTCK-SWCLK** | `SWCLK` | Conector SWD interno do ST-LINK | Clock de gravação e depuração Serial Wire |

```text
                                NUCLEO-F030R8
                       +-----------------------------+
                       |        ST-LINK v2.1         |
                       |       (Gravador e VCP)      |
                       +--------------+--------------+
                                      |
                           Cabo USB   | Linhas Internas:
                           para o PC  | - PA2 (USART2_TX)
                                      | - PA3 (USART2_RX)
                                      | - PA13/PA14 (SWD)
                                      v
                       +-----------------------------+
                       |        STM32F030R8T6        |
                       |        (Cortex-M0)          |
                       |                             |
                       |   [PA5]  ---> LED Verde LD2 |
                       |   [PC13] ---> Botão Azul B1 |
                       +-----------------------------+
```

---

## 3. Configuração Gráfica Passo a Passo no STM32CubeMX (`.ioc`)

Abra o arquivo de configuração de periféricos (`matrix_multi.ioc`) no STM32CubeIDE e siga as etapas:

### 3.1 Interface de Depuração (SWD) — Pinos `PA13` e `PA14`
1. No menu lateral esquerdo, vá em **System Core $\rightarrow$ SYS**.
2. No menu suspenso **Debug**, selecione **Serial Wire**.
3. *Garante que `PA13` e `PA14` permaneçam dedicados ao ST-LINK, permitindo depuração e regravação.*

---

### 3.2 Configuração de Clock (RCC: HSI 8 MHz Seguro)
Para garantir estabilidade absoluta sem depender de cristais externos:
1. No menu lateral, acesse **System Core $\rightarrow$ RCC**:
   - **High Speed Clock (HSE):** `Disable` (ou `BYPASS` se usar o MCO do ST-Link).
   - **Low Speed Clock (LSE):** `Disable`.
2. Na aba superior **Clock Configuration**:
   - Selecione a fonte **`HSI`** direta (8 MHz).
   - Prescalers AHB e APB1 configurados em `/1`, garantindo `HCLK = 8 MHz` e `PCLK1 = 8 MHz`.

---

### 3.3 Configuração da Porta Serial (USART2)
1. No menu lateral, vá em **Connectivity $\rightarrow$ USART2**:
2. Na aba **Mode**:
   - **Mode:** Selecione **`Asynchronous`**.
3. Na aba **Configuration $\rightarrow$ Parameter Settings**:
   - **Baud Rate:** Digite **`460800`** Bits/s *(ou `115200` para depuração lenta)*.
   - **Word Length:** `8 Bits (including Parity)`.
   - **Parity:** `None`.
   - **Stop Bits:** `1`.
   - **Over Sampling:** `16 Samples`.

---

### 3.4 Configuração de DMA da USART2 (Alta Performance sem Sobrecarga de CPU)
A recepção e o envio com Direct Memory Access (DMA) evitam que o Cortex-M0 perca ciclos de clock tratando interrupções byte a byte:

1. Na mesma janela da USART2, clique na aba **DMA Settings**:
2. Clique no botão **Add**:
   - Selecione **`USART2_RX`**:
     - **DMA Request:** `USART2_RX`
     - **Channel:** `DMA1 Channel 5` (atribuído automaticamente)
     - **Direction:** `Peripheral to Memory`
     - **Priority:** `High`
     - **Mode:** `Normal` (ou `Circular` para ring-buffer contínuo)
     - **Increment Address:** Marque **`Memory`** (Peripheral desmarcado)
     - **Data Width:** Peripheral = `Byte`, Memory = `Byte`.
3. Clique em **Add** novamente:
   - Selecione **`USART2_TX`**:
     - **DMA Request:** `USART2_TX`
     - **Channel:** `DMA1 Channel 4`
     - **Direction:** `Memory to Peripheral`
     - **Priority:** `Medium`
     - **Mode:** `Normal`
     - **Increment Address:** Marque **`Memory`**
     - **Data Width:** Peripheral = `Byte`, Memory = `Byte`.

---

### 3.5 Configuração de Interrupções (NVIC)
1. Na aba **NVIC Settings** da USART2:
   - Marque a caixa **`USART2 global interrupt`** $\rightarrow$ `Enabled`.
2. Em **System Core $\rightarrow$ NVIC**:
   - Certifique-se de que `DMA1 channel 4 and 5 interrupts` estejam ativadas.

---

### 3.6 Pino do LED de Status (`PA5`)
1. No diagrama de pinos do microcontrolador à direita, localize o pino **`PA5`**:
   - Clique nele com o botão esquerdo e selecione **`GPIO_Output`**.
2. Em **System Core $\rightarrow$ GPIO $\rightarrow$ PA5**:
   - **GPIO Output Level:** `Low` (inicia apagado).
   - **GPIO Mode:** `Output Push Pull`.
   - **GPIO Pull-up/Pull-down:** `No pull-up and no pull-down`.
   - **Maximum output speed:** `Low`.
   - **User Label:** Digite **`LD2`** (ou `LED_STATUS`).

---

### 3.7 Gerar o Código do HAL
Pressione **Ctrl + S** (ou vá em **Project $\rightarrow$ Generate Code**). O CubeMX gerará as inicializações em `Core/Src/main.c`, `stm32f0xx_hal_msp.c` e `Core/Inc/main.h`.

---

## 4. Configuração do Linker Script (`STM32F030R8TX_FLASH.ld`)

Como a matriz resultante $C$ não é mais gravada permanentemente na Flash física, **não é mais necessário reservar os 8 KB finais (`FLASH_C`)**. O espaço total de 64 KB fica 100% liberado para código:

```ld
/* Memories definition */
MEMORY
{
  RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 8K
  FLASH  (rx)     : ORIGIN = 0x08000000,   LENGTH = 64K
}

_Min_Heap_Size = 0x0;    /* Sem alocação dinâmica malloc */
_Min_Stack_Size = 0x400; /* 1024 bytes reservados para a pilha */
```

---

## 5. Estrutura Completa da Árvore de Diretórios (*Worktree*)

A arquitetura organiza de forma desacoplada os dois lados do sistema: o **Firmware Embarcado** e o **Host no PC**:

```text
matrix-tiling/
│
├── arquitetura.md                      # Especificação formal da arquitetura serial HIL
├── README.md                           # Documentação central do repositório
│
├── src_t1/                             # Pipeline da Tarefa 1 (Host x86_64)
│   ├── input_matrix/                   # 30 pares de matrizes estatísticas (A_1..A_30, B_1..B_30)
│   ├── bin_c/                          # Matrizes de referência calculadas pelo motor C do Host
│   └── bin_py/                         # Matrizes de referência analítica calculadas pelo NumPy
│
└── src_t2/                             # Pipeline da Tarefa 2 (Embarcado STM32 + Runner Serial)
    │
    ├── runner_serial.py                # Script Host Python: orquestra envio dos 30 testes e validação
    ├── protocolo_serial.py             # Parser e gerador de pacotes binários com CRC-16 no Python
    ├── framework_testes.ipynb          # Notebook de auditoria gráfica e análise estatística dos 30 testes
    ├── tutorial.md                     # Este guia completo de pinagem, configuração e execução
    │
    ├── audit/                          # Relatórios formais gerados pela auditoria serial
    │   ├── auditoria_serial_30_testes.csv
    │   ├── auditoria_serial_30_testes.json
    │   └── painel_auditoria_serial.png
    │
    └── firmware_stm32/                 # Projeto STM32CubeIDE (NUCLEO-F030R8)
        ├── matrix_multi.ioc            # Arquivo gráfico do CubeMX com USART2 DMA e pinos
        ├── STM32F030R8TX_FLASH.ld      # Linker script com 8 KB RAM e 64 KB Flash
        │
        ├── Core/
        │   ├── Inc/
        │   │   ├── main.h              # Definições de pinos (LD2_Pin, USART_TX_Pin, etc.)
        │   │   ├── protocolo.h         # Definição dos pacotes binários, comandos e CRC-16 em C
        │   │   └── multi_tiling.h      # Protótipo do motor de Tiling streaming por faixas
        │   │
        │   └── Src/
        │       ├── main.c              # Inicialização de HAL, DMA, USART2 e loop da máquina de estados
        │       ├── protocolo.c         # FSM do receptor serial, cálculo de CRC-16 e empacotador TX
        │       ├── multi_tiling.c      # Motor particionado de cálculo de bloco C[8][8] em 256 B de RAM
        │       ├── stm32f0xx_it.c      # Handlers de interrupção USART2 e DMA1
        │       └── stm32f0xx_hal_msp.c # Inicialização física de hardware e barramentos
        │
        └── Drivers/                    # Drivers oficiais STM32F0xx HAL e CMSIS
```

---

## 6. Como os Dados Trafegam: Estrutura do Pacote Serial Binário

Para que o Host e o STM32 troquem matrizes sem corrupção de bytes, o protocolo implementa uma estrutura padronizada com **soma de verificação CRC-16**:

```text
 0      1      2      3      4      5                  (5 + LEN)   (7 + LEN)
+------+------+------+------+------+------+------------+----------+-----------+
| SOF1 | SOF2 | CMD  | SEQ  | LEN_L| LEN_H|  PAYLOAD   |  CRC_L   |   CRC_H   |
| 0xAA | 0x55 | 1 B  | 1 B  |    2 Bytes  | (LEN bytes)|      2 Bytes        |
+------+------+------+------+------+------+------------+----------+-----------+
```

### Comandos Utilizados no Pipeline:
1. **`CMD_PING` (`0x01`):** Host pergunta *"STM32 está online?"*. STM32 responde com `CMD_ACK`.
2. **`CMD_SEND_STRIP_A` (`0x03`):** Host envia uma faixa de 8 linhas da matriz $A$ ($8 \times 45 \times 4 = 1.440\text{ bytes}$).
3. **`CMD_SEND_BLOCK_B` (`0x04`):** Host envia um sub-bloco de $B$ ($8 \times 8 \times 4 = 256\text{ bytes}$).
4. **`CMD_RESULT_TILE` (`0x05`):** STM32 devolve o sub-bloco $8 \times 8$ calculado de $C$ ($256\text{ bytes}$) com as coordenadas `(ii, jj)`.
5. **`CMD_ACK` (`0x06`):** Confirmação de recebimento íntegro do pacote.
6. **`CMD_NACK` (`0x07`):** Falha no CRC-16; solicita retransmissão imediata.

---

## 7. Roteiro de Execução na Prática

### Passo 1: Compilar e Gravar o Firmware no STM32
1. Conecte a placa **NUCLEO-F030R8** à porta USB do computador.
2. No **STM32CubeIDE**, selecione o projeto do firmware.
3. Pressione **Ctrl + B** para compilar (verifique: `0 errors, 0 warnings`).
4. Clique no ícone da **Baratinha (F11)** ou **Run (Ctrl + F11)** para gravar o binário no microcontrolador via ST-LINK.
5. O LED verde LD2 dará 3 piscadas rápidas e permanecerá aceso em repouso, indicando que a porta serial `USART2` está pronta para receber dados.

---

### Passo 2: Executar a Bateria Completa dos 30 Testes pelo VS Code
1. No VS Code, abra a pasta raiz do repositório (`matrix-tiling`).
2. Abra o terminal integrado (**Ctrl + `**).
3. Execute o runner serial:
   ```powershell
   python src_t2/runner_serial.py
   ```
4. **O que você observará:**
   - O script localiza automaticamente a porta `COM` do ST-LINK (ex: `COM7`).
   - O LED verde LD2 da placa começa a piscar em alta frequência, refletindo o fluxo de dados em streaming.
   - O terminal exibe o processamento em tempo real de cada teste:
     ```text
     ========================================================================
     EXECUÇÃO SERIAL EM TEMPO REAL: HARDWARE-IN-THE-LOOP (NUCLEO-F030R8)
     ========================================================================
     [OK] Placa conectada na porta COM7 a 460800 bps
     
     Processando Bateria de Testes (30 conjuntos estatísticos):
       -> Teste 01/30: Transmitindo A1 e B1 ... Recebido C1 (8100 B) | Erro Máx: 1.83e-04 [APROVADO]
       -> Teste 02/30: Transmitindo A2 e B2 ... Recebido C2 (8100 B) | Erro Máx: 1.76e-04 [APROVADO]
       -> Teste 03/30: Transmitindo A3 e B3 ... Recebido C3 (8100 B) | Erro Máx: 1.91e-04 [APROVADO]
       ...
       -> Teste 30/30: Transmitindo A30 e B30 ... Recebido C30 (8100 B) | Erro Máx: 1.85e-04 [APROVADO]
     
     ========================================================================
     STATUS FINAL: 100% DOS TESTES APROVADOS (30/30)
     Maior Erro Absoluto Global: 1.95e-04 <= 1.00e-03 (Tolerância de Higham)
     Tempo Médio por Matriz: 195 ms
     Laudos gravados em src_t2/audit/
     ========================================================================
     ```

---

### Passo 3: Visualizar os Resultados no Jupyter Notebook
Abra [`src_t2/framework_testes.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) no VS Code e clique em **Run All**:
- Os 30 casos de teste serão auditados e consolidados.
- O painel gráfico exibirá a curva de dispersão temporal, o histograma de ULPs e a matriz de resíduos de todos os 30 testes combinados.
