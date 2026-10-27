# ⚡ Tarefa 2 (T2): Acelerador Co-Processador Serial em Tempo Real (Hardware-in-the-Loop)

> **Diretório:** `src_t2/`  
> **Plataforma Alvo:** NUCLEO-F030R8 / STM32F030R8T6 (ARM Cortex-M0 @ **48 MHz com PLL**, **8 KB SRAM nativa**, 64 KB Flash)  
> **Comunicação:** USART2 (PA2=TX, PA3=RX) a **460.800 bps** com transferência em massa acelerada por **DMA1** (Canais 4 e 5) via ST-LINK Virtual COM Port (VCP)  
> **Estratégia de Memória:** **Tile-Strip Streaming** ($B=8$) mantendo a alocação estática sob controle rigoroso e consumo compatível com a SRAM de 8 KB  
> **Automação de Testes:** Execução contínua dos **30 conjuntos de matrizes** ($45 \times 45$, `float32`) com validação NumPy em tempo real e emissão de laudo metrológico.

---

## 1. Visão Geral da Arquitetura

O firmware transforma a placa **NUCLEO-F030R8** em um co-processador matemático autônomo conectado via cabo USB:

```text
+-----------------------------------------------------------------------------------------+
|                                HOST PC (Python 3 / NumPy)                               |
|                                                                                         |
|  - Lê matrizes A_k e B_k de src_t1/input_matrix/ (30 conjuntos estatísticos + unitários)|
|  - Transmite faixas horizontais de A (até 1.440 B) e aguarda confirmação 'K'            |
|  - Transmite blocos verticais de B (até 1.440 B) e recebe sub-blocos C em tempo real   |
|  - Compara C_stm32 vs C_py (NumPy float32) e emite laudos automáticos (CSV)             |
+-----------------------------------------------------------------------------------------+
                                           |   ^
                    Streaming Serial USB   |   |  Sub-bloco Resultante C[8][8] (até 256 B)
                    (Baud: 460.800 bps)    v   |  (via UART TX acelerado por DMA1 Canal 4)
+-----------------------------------------------------------------------------------------+
|                              NUCLEO-F030R8 (ARM Cortex-M0)                              |
|                                                                                         |
|  - Clock do Barramento: HSI com PLL a 48 MHz (máximo estável com FLASH_LATENCY_1)        |
|  - Interface Física: USART2 (PA2/PA3) via ST-LINK VCP (460.800 bps, 8N1)                |
|  - Movimentação de Dados: DMA1 Canal 5 (USART2_RX) e DMA1 Canal 4 (USART2_TX)           |
|  - Loop de escuta: Comandos 'P' (Ping), 'A' (Faixa A via DMA) e 'B' (Bloco B via DMA)  |
|  - Motor de Tiling Streaming: acumula sub-bloco C[8][8] em apenas 256 B de SRAM local   |
|  - Sinalização por LED LD2 (PA5): pisca 3x no boot, pulsa em tráfego, fixo em repouso   |
+-----------------------------------------------------------------------------------------+
```

---

## 2. Mecanismo de Transferência: UART Física vs. Movimentação por DMA

> [!IMPORTANT]
> **Como os dados trafegam entre PC e Microcontrolador?**  
> A transferência combina **UART** como interface física de comunicação externa e **DMA** como motor de movimentação direta de memória dentro do microcontrolador.

### 2.1 Interface Física Externa: **UART (USART2)**
* **Canal e Pinos:** O fluxo serial trafega pelos pinos `PA2` (USART2_TX) e `PA3` (USART2_RX) da placa NUCLEO-F030R8.
* **Transmissão USB:** Esses pinos conectam-se internamente ao circuito integrado do programador ST-LINK v2.1, que encapsula a UART em uma porta serial virtual USB (*Virtual COM Port - VCP*, ex: `COM7`), eliminando a necessidade de adaptadores USB-Serial externos.
* **Velocidade:** Configurada a **460.800 bps** com formatação 8N1 (8 bits de dados, sem paridade, 1 stop bit).

### 2.2 Movimentação de Dados em Massa: **DMA (Direct Memory Access)**
* **Por que DMA?** A uma taxa de 460.800 bps, um byte chega a cada ~21,7 µs. Se o núcleo ARM Cortex-M0 (sem FPU) tratasse cada byte por interrupção simples de software, o overhead de troca de contexto geraria gargalos severos de CPU e risco iminente de perda de dados (*RX Overrun Error - ORE*).
* **Operação no Firmware:**
  * **Recepção de Faixas de $A$ e Blocos de $B$:** Assim que o comando é decodificado, o firmware arma o canal de recepção com [`HAL_UART_Receive_DMA()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L106). O controlador **DMA1 (Canal 5)** desvia os bytes de ponto flutuante diretamente do registrador `USART2->RDR` para o buffer na SRAM, sem intervenção da CPU.
  * **Transmissão do Bloco $C$:** Ao término do produto particionado, o sub-bloco resultante (até 256 bytes) é despachado via [`HAL_UART_Transmit_DMA()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L144) pelo **DMA1 (Canal 4)** diretamente da SRAM para `USART2->TDR`.
  * **Notificação por Callbacks:** Ao concluir o número programado de bytes, o subsistema DMA dispara as interrupções de hardware `HAL_UART_RxCpltCallback` ou `HAL_UART_TxCpltCallback`, liberando os laços de sincronismo do firmware.
  * **Configuração de Baixo Nível:** Os canais de DMA e os pinos da USART2 são associados e configurados em [`stm32f0xx_hal_msp.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_hal_msp.c#L89-L135), e as rotinas de tratamento de interrupção residem em [`stm32f0xx_it.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_it.c#L81-L85).

### 2.3 Bytes de Controle e Sinalização: **UART Direta (Polling)**
* Bytes únicos de sinalização e handshake que exigem baixa latência de decisão imediata (como `'P'` de Ping, `'A'`, `'B'` e os caracteres de confirmação `'O'` e `'K'`) utilizam `HAL_UART_Receive()` e `HAL_UART_Transmit()` por polling simples na USART2.

| Etapa do Fluxo | Dado Trafegado | Interface de Comunicação | Mecanismo de Movimentação | Função da CPU |
| :--- | :---: | :---: | :---: | :--- |
| **Ping / Handshake** | `'P'` e `'O'` (1 byte) | USART2 (VCP) | UART Polling direto | Decodifica e responde imediatamente |
| **Envio da Faixa $A$** | $bi \times 45$ floats (até 1.440 B) | USART2 (VCP) | **DMA1 Canal 5 (RX)** | Totalmente desonerada durante o tráfego |
| **Confirmação Faixa $A$** | `'K'` (1 byte) | USART2 (VCP) | UART Polling direto | Confirma integridade do recebimento |
| **Envio do Bloco $B$** | $45 \times bj$ floats (até 1.440 B) | USART2 (VCP) | **DMA1 Canal 5 (RX)** | Totalmente desonerada durante o tráfego |
| **Cálculo de $C$** | Sub-bloco $bi \times bj$ | Local (SRAM) | - | Executa laços de Tiling na SRAM (256 B) |
| **Retorno do Bloco $C$** | $bi \times bj$ floats (até 256 B) | USART2 (VCP) | **DMA1 Canal 4 (TX)** | CPU liberada enquanto o DMA transmite |

---

## 3. Estrutura Modular da Árvore (*Worktree*) de `src_t2/`

```text
src_t2/
├── README.md                           # Especificação técnica, etapas e arquitetura da T2
├── arquitetura.md                      # Especificação formal da arquitetura serial HIL
├── tutorial.md                         # Guia prático passo a passo de pinagem, CubeMX e execução
├── dificuldades_e_solucoes.md          # Resumo de todos os desafios de engenharia, soluções e locais no código
├── runner_serial.py                    # Orquestrador Host Python: gerencia porta serial e os 30 testes
├── protocolo_serial.py                 # Módulo de protocolo serial simplificado (comandos 'P', 'A', 'B')
├── framework_testes.ipynb              # Caderno Jupyter de auditoria gráfica e análise estatística
├── STM32F030R8TX_FLASH.ld              # Linker script oficial de referência (8 KB RAM, 64 KB Flash)
│
├── audit/                              # Laudos e evidências gerados automaticamente
│   └── auditoria_serial_30_testes.csv  # Tabela com métricas metrológicas dos 30 testes
│
├── gerados/                            # Código-base exportado pelo STM32CubeMX (referência de pinagem e HAL)
│   ├── main.c                          # Template de inicialização gerado
│   ├── main.h                          # Definições de hardware geradas
│   └── stm32f0xx_hal_msp.c             # Configuração MSP de periféricos gerada
│
└── firmware_stm32/                     # Código-Fonte do Firmware Embarcado Operacional (C / HAL)
    ├── STM32F030R8TX_FLASH.ld          # Linker script do projeto (8 KB RAM, 64 KB Flash)
    │
    ├── Core/Inc/
    │   ├── main.h                      # Definições de pinos, barramentos e protótipos da Nucleo
    │   ├── protocolo.h                 # Definição dos comandos diretos ('P', 'A', 'B', 'O', 'K')
    │   ├── multi_tiling.h              # Protótipo do motor de Tiling streaming por faixas
    │   └── stm32f0xx_it.h              # Protótipos das rotinas de interrupção (SysTick, DMA, USART)
    │
    └── Core/Src/
        ├── main.c                      # Ponto de entrada: clock 48 MHz, DMA1, USART2 e loop principal
        ├── protocolo.c                 # Apoio ao módulo de protocolo
        ├── multi_tiling.c              # Multiplicação particionada em blocos 8x8 (256 B RAM)
        ├── stm32f0xx_hal_msp.c         # Configuração de baixo nível de GPIOs, USART2 e canais DMA
        └── stm32f0xx_it.c              # Tratadores de interrupção (DMA1 Channel 4/5 e USART2)
```

---

## 4. Fluxo de Execução do Pipeline HIL (Abstração por Etapas)

O ciclo de execução contínua entre o **Host PC** e o **STM32** opera de forma síncrona (*Hardware-in-the-Loop*), dividido em 8 etapas conceituais:

```text
[1. Handshake] ──> [2. Carga Matrizes] ──> [3. Fatiamento A] ──> [4. Streaming B]
                                                                        │
[8. Relatórios] <── [7. Auditoria NumPy] <── [6. Reconstrução C] <── [5. Retorno C]
```

---

### Etapa 1: Handshake e Sincronismo Inicial
* **Conceito:** Antes de trafegar matrizes, o Host testa a conectividade da porta serial USB enviando um *ping* (`'P'`). O STM32 responde com confirmação imediata (`'O'`) e o LED de status `LD2` sinaliza a operação.
* 📍 **Onde se aprofundar:**
  - Host: [`auto_detectar_porta_stlink()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L33-L43) e conexão em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L120-L135) em `runner_serial.py`.
  - Firmware: Resposta ao `CMD_PING` em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L90-L93) em `main.c`.

---

### Etapa 2: Carregamento dos Casos de Teste
* **Conceito:** O Host lê os arquivos binários brutos `A_k.bin` e `B_k.bin` de `src_t1/input_matrix/` em ponto flutuante de precisão simples (`float32`), moldando-os no formato matricial $45 \times 45$.
* 📍 **Onde se aprofundar:**
  - Host: Carga via `np.fromfile()` no laço de testes em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L151-L153) em `runner_serial.py`.

---

### Etapa 3: Fatiamento Horizontal (*Tile-Strip*) e Envio de $A$ via DMA
* **Conceito:** Como a matriz $A$ completa ($8{,}1\text{ KB}$) esgotaria a memória do microcontrolador ($8\text{ KB}$), ela é dividida em faixas horizontais de no máximo 8 linhas ($bi \le 8$). Cada faixa ($1.440\text{ bytes}$) é transmitida ao STM32, que a transfere diretamente para a SRAM estática via **DMA1 Canal 5** (`HAL_UART_Receive_DMA`), confirmando ao final com `ACK` (`'K'`).
* 📍 **Onde se aprofundar:**
  - Host: Laço externo de fatiamento em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L71-L84) em `runner_serial.py`.
  - Firmware: Recepção do comando via polling e payload via DMA em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L96-L121) em `main.c`, e gravação na SRAM em [`multi_tiling_set_faixa_A()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L34-L46) em `multi_tiling.c`.

---

### Etapa 4: Streaming de Sub-blocos Verticais de $B$ via DMA e Multiplicação
* **Conceito:** Mantendo a faixa atual de $A$ em memória, o Host envia sucessivos blocos verticais de $B$ (de até 8 colunas, $bj \le 8$). A transferência da USART2 para o `buffer_B` é realizada via **DMA1 Canal 5** (`HAL_UART_Receive_DMA`). O STM32 computa o produto matricial particionado do sub-bloco resultante $C[bi][bj]$ acumulando em um buffer de apenas **256 bytes**.
* 📍 **Onde se aprofundar:**
  - Host: Transmissão dos blocos verticais em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L86-L94) em `runner_serial.py`.
  - Firmware: Recepção DMA em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L124-L140) e algoritmo de Tiling em [`multi_tiling_computar_bloco()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L74-L120) em `multi_tiling.c`.

---

### Etapa 5: Devolução Imediata do Bloco $C$ Calculado via DMA
* **Conceito:** O microcontrolador não armazena a matriz $C$ inteira. Assim que cada sub-bloco de até $8 \times 8$ floats termina de ser calculado, ele é devolvido imediatamente via **DMA1 Canal 4** (`HAL_UART_Transmit_DMA`) pela serial para o computador, liberando a CPU para a próxima iteração.
* 📍 **Onde se aprofundar:**
  - Firmware: Despacho não-bloqueante por DMA em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L141-L151) em `main.c`.

---

### Etapa 6: Reconstrução da Matriz $C$ no Host
* **Conceito:** O Host lê os bytes brutos recebidos da serial, decodifica para `float32` e posiciona o sub-bloco na fatia correspondente da matriz global $C$ na memória do PC.
* 📍 **Onde se aprofundar:**
  - Host: Leitura precisa com [`ler_exato()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L46-L59) e encaixe em `C_resultado` em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L96-L101) em `runner_serial.py`.

---

### Etapa 7: Auditoria Numérica em Tempo Real (vs. NumPy)
* **Conceito:** O Host calcula o gabarito de referência com `np.matmul(A, B)` e audita a matriz devolvida pelo microcontrolador através de métricas formais: erro absoluto máximo ($L_\infty$), desvio em ULPs (*Units in the Last Place*) e tolerância de conformidade.
* 📍 **Onde se aprofundar:**
  - Host: Avaliação de conformidade e tolerância em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L160-L180) em `runner_serial.py`.

---

### Etapa 8: Consolidação Estatística e Emissão de Laudos
* **Conceito:** Ao término dos testes, o Host compila as métricas de erro em relatório tabular legível (`auditoria_serial_30_testes.csv`) para controle de qualidade e auditoria metrológica.
* 📍 **Onde se aprofundar:**
  - Host: Exportação em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L182-L192) em `runner_serial.py`.
  - Análise Gráfica: Caderno [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) para geração de cartas de controle estatístico e dispersão de resíduos.

---

## 5. Função e Responsabilidade de Cada Arquivo no Processo

Cada arquivo em `src_t2/` possui uma responsabilidade estrita dentro da arquitetura de co-processador:

| Arquivo | Camada | Papel e Função no Pipeline |
| :--- | :---: | :--- |
| [**`runner_serial.py`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py) | **Host (PC)** | **Orquestrador Mestre dos Testes:** Gerencia a porta serial USB, carrega os 30 pares de matrizes de `src_t1/input_matrix/`, fatia em faixas de 8 linhas, transmite para a placa via serial, recebe $C_{stm32}$, calcula o gabarito no NumPy, audita os erros e grava o laudo em `src_t2/audit/auditoria_serial_30_testes.csv`. |
| [**`protocolo_serial.py`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/protocolo_serial.py) | **Host (PC)** | **Camada de Transporte Simplificada:** Define os comandos de 1 byte (`'P'`, `'A'`, `'B'`) e respostas (`'O'`, `'K'`) para comunicação direta sem overhead de protocolo. |
| [**`framework_testes.ipynb`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) | **Host (PC)** | **Caderno de Auditoria Gráfica (Jupyter):** Consome os dados consolidados da auditoria dos 30 testes e plota os gráficos de controle de processo ($L_\infty$), histogramas de desvio em ULPs e emite o laudo visual de conformidade. |
| [**`main.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c) | **Firmware (STM32)** | **Ponto de Entrada do Microcontrolador:** Inicializa o clock HSI com PLL a 48 MHz, configura os GPIOs, inicializa o DMA1 e a USART2 a 460.800 bps, sinaliza boot no LED LD2 e executa o loop de escuta de comandos com recepção/transmissão via DMA. |
| [**`stm32f0xx_hal_msp.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_hal_msp.c) | **Firmware (STM32)** | **Inicialização de Baixo Nível (MSP):** Ativa os clocks de barramento, configura os pinos PA2 (TX) e PA3 (RX), mapeia os canais DMA1 Canal 5 (RX) e DMA1 Canal 4 (TX) e vincula aos handles da HAL. |
| [**`stm32f0xx_it.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/stm32f0xx_it.c) / [**`stm32f0xx_it.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/stm32f0xx_it.h) | **Firmware (STM32)** | **Tratadores de Interrupção (ISR):** Gerencia as interrupções de hardware essenciais, incluindo `SysTick_Handler`, `DMA1_Channel4_5_IRQHandler` e `USART2_IRQHandler`. |
| [**`multi_tiling.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c) / [**`multi_tiling.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/multi_tiling.h) | **Firmware (STM32)** | **Motor de Multiplicação por Tiling:** Armazena a faixa ativa de $A$ ($1.440\text{ bytes}$) e calcula o produto matricial particionado do sub-bloco em apenas **256 bytes de SRAM**, permitindo que o chip de 8 KB opere com folga total sem estouro de memória. |
| [**`protocolo.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/protocolo.h) / [**`protocolo.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/protocolo.c) | **Firmware (STM32)** | **Definições do Protocolo:** Mapeia os caracteres de comando (`'P'`, `'A'`, `'B'`, `'O'`, `'K'`) compartilhados entre o firmware C e os scripts Python. |
| [**`main.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/main.h) | **Firmware (STM32)** | **Mapeamento de Hardware da Placa:** Define os pinos físicos da NUCLEO-F030R8 (LED LD2 em `PA5`, USART2 TX em `PA2`, RX em `PA3`, Botão B1 em `PC13`). |
| [**`STM32F030R8TX_FLASH.ld`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/STM32F030R8TX_FLASH.ld) | **Firmware (STM32)** | **Linker Script de Memória:** Define as regiões físicas de hardware do microcontrolador: 64 KB de Flash ROM para código executável e 8 KB de SRAM estática com proteção de pilha (*Stack* de 1.024 bytes). |
| [**`auditoria_serial_30_testes.csv`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/auditoria_serial_30_testes.csv) | **Auditoria (Dados)** | **Tabela de Conformidade Formal:** Registro em formato tabular legível contendo `teste_id`, maior erro absoluto ($L_\infty$), desvio em ULPs e status de aprovação de cada um dos 30 testes. |
| [**`gerados/`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/gerados/) | **Referência** | **Arquivos Base do CubeMX:** Contém as saídas originais do gerador de código para referência de inicialização periférica do microcontrolador. |
| [**`arquitetura.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/arquitetura.md) | **Documentação** | **Especificação Teórica e Arquitetural:** Fundamenta matematicamente a inviabilidade de alocar 24,3 KB em 8 KB de RAM, deduz o orçamento de memória do *Tile-Strip Streaming*, detalha a pinagem física da USART2 e apresenta os diagramas de sequência. |
| [**`tutorial.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md) | **Documentação** | **Guia Prático de Bancada:** Tutorial com o passo a passo de configuração gráfica dos periféricos no STM32CubeMX (`.ioc`), configuração de clock e compilação no CubeIDE. |
| [**`dificuldades_e_solucoes.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/dificuldades_e_solucoes.md) | **Documentação** | **Desafios e Soluções de Engenharia:** Resumo minucioso de todas as dificuldades técnicas (estouro de 8 KB de RAM, DMA vs UART a 460.800 bps, falta de FPU, bordas de $N=45$, sincronismo serial USB) com suas soluções e referências exatas no código. |

---

## 6. Protocolo Serial Simplificado de Alta Performance

A comunicação opera de forma direta em regime de **Request-Response** síncrono:

| Comando | Origem $\rightarrow$ Destino | Payload Enviado | Resposta da Placa | Descrição da Operação |
| :---: | :---: | :---: | :---: | :--- |
| **`'P'`** (`0x50`) | Host $\rightarrow$ STM32 | *(vazio)* | **`'O'`** (`0x4F`) | **CMD_PING:** Handshake inicial de conectividade e sincronismo da porta serial. |
| **`'A'`** (`0x41`) | Host $\rightarrow$ STM32 | `[bi: 1B]` + `[bi * 45 floats]` (até 1.440 B) | **`'K'`** (`0x4B`) | **CMD_STRIP_A:** Transmite a faixa horizontal de $A$ via DMA. A placa armazena e confirma com `'K'`. |
| **`'B'`** (`0x42`) | Host $\rightarrow$ STM32 | `[bj: 1B]` + `[45 * bj floats]` (até 1.440 B) | `[bi * bj floats]` (até 256 B) | **CMD_BLOCK_B:** Transmite bloco de colunas de $B$ via DMA. STM32 calcula e devolve o bloco de $C$ via DMA. |

---

## 7. Orçamento de Memória SRAM (NUCLEO-F030R8: 8 KB)

O microcontrolador STM32F030R8T6 possui exatamente **8.192 bytes (8 KB)** de SRAM física. Alocar as matrizes completas ($45 \times 45 \times 4\text{ B} \times 3 \approx 24{,}3\text{ KB}$) causaria estouro imediato de memória. A arquitetura particionada garante que todo o footprint estático caiba com folga segura:

| Estrutura de Memória | Arquivo / Módulo | Tamanho Máximo | Função e Alocação |
| :--- | :---: | :---: | :--- |
| `buffer_A[8 * 45]` | `main.c` | 1.440 bytes | Buffer de recepção DMA para a faixa horizontal de $A$ |
| `buffer_B[45 * 8]` | `main.c` | 1.440 bytes | Buffer de recepção DMA para o bloco vertical de $B$ |
| `bloco_C_res[8 * 8]` | `main.c` | 256 bytes | Sub-bloco resultante transmitido via DMA de volta ao Host |
| `faixa_A[8][45]` | `multi_tiling.c` | 1.440 bytes | Faixa ativa de $A$ na SRAM preservada para reuso com múltiplos blocos de $B$ |
| Pilha do Sistema (*Stack*) | Linker Script | 1.024 bytes | Pilha de chamadas de funções e variáveis locais da CPU |
| **Total Estático + Pilha** | — | **~5.600 bytes** | **Mais de 2.500 bytes de SRAM livre (> 31% de margem de folga!)** |

---

## 8. Como Executar a Bateria dos 30 Testes

### Pré-requisitos:
1. Conectar a placa **NUCLEO-F030R8** via cabo USB ao computador (porta ST-LINK VCP detectada automaticamente).
2. Gravar o firmware localizado em [`src_t2/firmware_stm32/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/) utilizando o STM32CubeIDE.
3. Garantir que as matrizes de entrada da Tarefa 1 estejam geradas em `src_t1/input_matrix/`.

### No Terminal (PowerShell / Bash):
```powershell
# Execução com auto-detecção da porta ST-LINK (padrão: 460.800 bps, 30 testes):
python src_t2/runner_serial.py

# Ou especificando manualmente a porta COM e parâmetros desejados:
python src_t2/runner_serial.py --port COM7 --baud 460800 --testes 30
```

### Exemplo de Saída em Tempo Real:
```text
================================================================================
CO-PROCESSADOR SERIAL SIMPLIFICADO: STM32 HIL (STREAMING POR BLOCOS)
================================================================================
[*] Conectando à placa física na porta COM7 a 460800 bps...
[OK] Conexão com NUCLEO-F030R8 estabelecida com sucesso!

Iniciando execução contínua de 30 conjuntos de matrizes (45x45 float32):
--------------------------------------------------------------------------------
  -> Teste 01/30 | Erro Máx: 1.525879e-04 | ULPs: 2.50 | [APROVADO]
  -> Teste 02/30 | Erro Máx: 1.220703e-04 | ULPs: 2.00 | [APROVADO]
  -> Teste 03/30 | Erro Máx: 1.373291e-04 | ULPs: 2.25 | [APROVADO]
  ...
  -> Teste 30/30 | Erro Máx: 1.220703e-04 | ULPs: 2.00 | [APROVADO]

================================================================================
RELATÓRIO CONSOLIDADO: 30/30 TESTES APROVADOS (100.0%)
Relatório CSV salvo em:  C:\Users\diego\Desktop\repositorios\matrix-tiling\src_t2\audit\auditoria_serial_30_testes.csv
================================================================================
```

---

## 9. Auditoria Gráfica no Jupyter Notebook

Abra [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) no VS Code e execute para visualizar:
- As cartas de controle estatístico de processo (SPC) do erro máximo ($L_\infty$).
- Os histogramas de desvio em ULPs (*Units in the Last Place*).
- A correlação entre o gabarito NumPy e as predições geradas no silício do STM32.

---

## 10. Documentos Complementares

Para um mergulho aprofundado em cada faceta do projeto:
- [**`dificuldades_e_solucoes.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/dificuldades_e_solucoes.md) — Relato detalhado de todos os desafios de engenharia (RAM, DMA, falta de FPU, bordas de matrizes e sincronismo USB) com as soluções implementadas.
- [**`arquitetura.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/arquitetura.md) — Fundamentação teórica formal, deduções matemáticas de memória e diagramas de sequência da USART2.
- [**`tutorial.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md) — Guia prático de configuração de periféricos e pinagem no STM32CubeMX e compilação no CubeIDE.
