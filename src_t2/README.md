# ⚡ Tarefa 2 (T2): Acelerador Co-Processador Serial em Tempo Real (Hardware-in-the-Loop)

> **Diretório:** `src_t2/`  
> **Plataforma Alvo:** NUCLEO-F030R8 / STM32F030R8T6 (ARM Cortex-M0, **8 KB SRAM nativa**, 64 KB Flash)  
> **Comunicação:** USART2 (PA2=TX, PA3=RX) a **460.800 bps** via ST-LINK Virtual COM Port (VCP)  
> **Estratégia de Memória:** **Tile-Strip Streaming** ($B=8$) mantendo o consumo de SRAM em apenas **~3,1 KB**  
> **Automação de Testes:** Execução contínua dos **30 conjuntos de matrizes** ($45 \times 45$, `float32`) com validação NumPy em tempo real.

---

## 1. Visão Geral da Arquitetura

O firmware transforma a placa **NUCLEO-F030R8** em um co-processador matemático autônomo conectado via cabo USB:

```text
+-----------------------------------------------------------------------------------------+
|                                HOST PC (Python 3 / NumPy)                               |
|                                                                                         |
|  - Lê matrizes A_k e B_k de src_t1/input_matrix/ (30 conjuntos estatísticos + unitários)|
|  - Transmite faixas horizontais de A e recebe confirmação 'K'                           |
|  - Transmite blocos de B e recebe sub-blocos calculados de C em tempo real pela serial  |
|  - Compara C_stm32 vs C_py (NumPy float32) e emite laudos automáticos (CSV, JSON)       |
+-----------------------------------------------------------------------------------------+
                                           |   ^
                    Streaming Serial USB   |   |  Sub-bloco Resultante C[8][8]
                    (Baud: 460.800 bps)    v   |  (apenas 256 bytes)
+-----------------------------------------------------------------------------------------+
|                              NUCLEO-F030R8 (ARM Cortex-M0)                              |
|                                                                                         |
|  - ST-LINK VCP (PA2/PA3 USART2)                                                         |
|  - Loop de escuta direto: comandos 'P' (Ping), 'A' (Faixa A) e 'B' (Bloco B)            |
|  - Motor de Tiling Streaming: acumula sub-bloco de C[8][8] em apenas 256 B de SRAM      |
|  - Consumo total de RAM: ~3,1 KB (quase 5 KB livres na SRAM nativa de 8 KB)             |
|  - Sinalização por LED LD2 (PA5): pulsa em atividade serial, fixo em repouso            |
+-----------------------------------------------------------------------------------------+
```

---

## 2. Estrutura Modular da Árvore (*Worktree*) de `src_t2/`

```text
src_t2/
├── README.md                           # Especificação técnica, etapas e papéis de cada arquivo
├── arquitetura.md                      # Especificação formal da arquitetura serial HIL
├── tutorial.md                         # Guia prático passo a passo de pinagem, CubeMX e execução
├── runner_serial.py                    # Orquestrador Host Python: gerencia porta serial e os 30 testes
├── protocolo_serial.py                 # Módulo de protocolo serial simplificado (comandos 'P', 'A', 'B')
├── framework_testes.ipynb              # Caderno Jupyter de auditoria gráfica e análise estatística
│
├── audit/                              # Laudos e evidências gerados automaticamente
│   ├── auditoria_serial_30_testes.csv  # Tabela com métricas metrológicas dos 30 testes
│   ├── auditoria_serial_30_testes.json # Relatório estruturado em JSON com taxas de aprovação
│   └── painel_auditoria_serial.png     # Painel visual com controle de processo, histograma e latência
│
└── firmware_stm32/                     # Código-Fonte do Firmware Embarcado (C / HAL)
    ├── STM32F030R8TX_FLASH.ld          # Linker script oficial (8 KB RAM, 64 KB Flash)
    │
    ├── Core/Inc/
    │   ├── main.h                      # Definições de pinos e barramentos da Nucleo
    │   ├── protocolo.h                 # Definição dos comandos diretos ('P', 'A', 'B', 'O', 'K')
    │   └── multi_tiling.h              # Protótipo do motor de Tiling streaming por faixas
    │
    └── Core/Src/
        ├── main.c                  # Inicialização de HAL, USART2 e loop principal simplificado
        ├── protocolo.c             # Apoio ao módulo de protocolo
        └── multi_tiling.c          # Multiplicação particionada em blocos 8x8 (256 B RAM)
```

---

### 3. Fluxo de Execução do Pipeline HIL (Abstração por Etapas)

O ciclo de execução contínua entre o **Host PC** e o **STM32** opera de forma síncrona (*Hardware-in-the-Loop*), dividido em 8 etapas conceituais:

```text
[1. Handshake] ──> [2. Carga Matrizes] ──> [3. Fatiamento A] ──> [4. Streaming B]
                                                                        │
[8. Relatórios] <── [7. Auditoria NumPy] <── [6. Reconstrução C] <── [5. Retorno C]
```

---

### Etapa 1: Handshake e Sincronismo Inicial
* **Conceito:** Antes de trafegar matrizes, o Host testa a conectividade da porta serial USB enviando um *ping* (`'P'`). O STM32 responde com confirmação imediata (`'O'`) e pisca o LED de status `LD2`.
* 📍 **Onde se aprofundar:**
  - Host: [`auto_detectar_porta_stlink()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L33-L43) e handshake em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L116-L126) em `runner_serial.py`.
  - Firmware: Resposta ao `CMD_PING` em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L70-L74) em `main.c`.

---

### Etapa 2: Carregamento dos Casos de Teste
* **Conceito:** O Host lê os arquivos binários brutos `A_k.bin` e `B_k.bin` do disco em ponto flutuante de precisão simples (`float32`), moldando-os no formato matricial $45 \times 45$.
* 📍 **Onde se aprofundar:**
  - Host: Carga via `np.fromfile()` no laço de testes em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L145-L146) em `runner_serial.py`.

---

### Etapa 3: Fatiamento Horizontal (*Tile-Strip*) e Envio de $A$
* **Conceito:** Como a matriz $A$ completa ($8{,}1\text{ KB}$) esgotaria a memória do microcontrolador ($8\text{ KB}$), ela é dividida em faixas horizontais de no máximo 8 linhas ($bi \le 8$). Cada faixa ($1.440\text{ bytes}$) é transmitida ao STM32, que a guarda na SRAM estática e responde com `ACK` (`'K'`).
* 📍 **Onde se aprofundar:**
  - Host: Laço externo de fatiamento em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L71-L81) em `runner_serial.py`.
  - Firmware: Recepção UART e validação de `bi` em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L76-L91) em `main.c`, e gravação na SRAM em [`multi_tiling_set_faixa_A()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L34-L46) em `multi_tiling.c`.

---

### Etapa 4: Streaming de Sub-blocos Verticais de $B$ e Multiplicação
* **Conceito:** Mantendo a faixa atual de $A$ em memória, o Host envia sucessivos blocos verticais de $B$ (de até 8 colunas, $bj \le 8$). O STM32 computa o produto matricial particionado do sub-bloco resultante $C[bi][bj]$ acumulando em um buffer de apenas **256 bytes**.
* 📍 **Onde se aprofundar:**
  - Host: Transmissão dos blocos verticais em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L83-L90) em `runner_serial.py`.
  - Firmware: Algoritmo de Tiling com particionamento na dimensão interna $K$ em [`multi_tiling_computar_bloco()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L74-L120) em `multi_tiling.c`.

---

### Etapa 5: Devolução Imediata do Bloco $C$ Calculado
* **Conceito:** O microcontrolador não armazena a matriz $C$ inteira. Assim que cada sub-bloco de até $8 \times 8$ floats termina de ser calculado, ele é devolvido imediatamente via UART serial para o computador.
* 📍 **Onde se aprofundar:**
  - Firmware: Chamada `HAL_UART_Transmit` em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L106-L107) em `main.c`.

---

### Etapa 6: Reconstrução da Matriz $C$ no Host
* **Conceito:** O Host lê os bytes brutos recebidos da serial, decodifica para `float32` e posiciona o sub-bloco na fatia correspondente da matriz global $C$ na memória do PC.
* 📍 **Onde se aprofundar:**
  - Host: Leitura precisa com [`ler_exato()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L46-L59) e encaixe em `C_resultado` em [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L91-L97) em `runner_serial.py`.

---

### Etapa 7: Auditoria Numérica em Tempo Real (vs. NumPy)
* **Conceito:** O Host calcula o gabarito de referência com `np.matmul(A, B)` e audita a matriz devolvida pelo microcontrolador através de métricas metrológicas formais: erro absoluto máximo ($L_\infty$), desvio em ULPs (*Units in the Last Place*) e limite de erro de Higham.
* 📍 **Onde se aprofundar:**
  - Host: Avaliação de conformidade e tolerância em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L150-L165) em `runner_serial.py`.

---

### Etapa 8: Consolidação Estatística e Emissão de Laudos
* **Conceito:** Ao término dos 30 testes, o Host compila as métricas de tempo e erro em relatórios estruturados para controle de qualidade e auditoria gráfica.
* 📍 **Onde se aprofundar:**
  - Host: Exportação em [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L176-L186) em `runner_serial.py`.
  - Análise Gráfica: Caderno [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) que plota as cartas de controle estatístico e persistência em [`painel_auditoria_serial.png`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/painel_auditoria_serial.png).

---

## 4. Função e Responsabilidade de Cada Arquivo no Processo

Cada arquivo em `src_t2/` possui uma responsabilidade estrita dentro da arquitetura de co-processador:

| Arquivo | Camada | Papel e Função no Pipeline |
| :--- | :---: | :--- |
| [**`runner_serial.py`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py) | **Host (PC)** | **Orquestrador Mestre dos Testes:** Gerencia a porta serial USB, carrega os 30 pares de matrizes de `src_t1/input_matrix/`, fatia em faixas de 8 linhas, envia para a placa, recebe $C_{stm32}$, calcula o gabarito no NumPy, audita os erros e grava os laudos em `src_t2/audit/`. |
| [**`protocolo_serial.py`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/protocolo_serial.py) | **Host (PC)** | **Camada de Transporte Simplificada:** Define os comandos de 1 byte (`'P'`, `'A'`, `'B'`) e respostas (`'O'`, `'K'`) para comunicação direta e rápida sem overhead. |
| [**`framework_testes.ipynb`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) | **Host (PC)** | **Caderno de Auditoria Gráfica (Jupyter):** Consome os dados consolidados da auditoria dos 30 testes e plota os gráficos de controle de processo ($L_\infty$ vs tempo), histogramas de desvio em ULPs e dispersão temporal, emitindo o laudo formal de conformidade de 100%. |
| [**`main.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c) | **Firmware (STM32)** | **Ponto de Entrada do Microcontrolador:** Inicializa o clock HSI (8 MHz), configura os GPIOs e a USART2 a 460.800 bps, controla o LED LD2 de status e executa o loop principal de escuta serial direta. |
| [**`protocolo.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/protocolo.h) | **Firmware (STM32)** | **Definições do Protocolo:** Mapeia os caracteres de comando (`'P'`, `'A'`, `'B'`, `'O'`, `'K'`) compartilhados entre o C e o Python. |
| [**`multi_tiling.c`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c) / [**`multi_tiling.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/multi_tiling.h) | **Firmware (STM32)** | **Motor de Multiplicação por Tiling:** Armazena a faixa ativa de $A$ ($1.440\text{ bytes}$) e calcula o produto matricial do bloco $8 \times 8$ em apenas **256 bytes de SRAM**, permitindo que o chip de 8 KB opere com folga total sem estouro de memória. |
| [**`main.h`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Inc/main.h) | **Firmware (STM32)** | **Mapeamento de Hardware da Placa:** Define os pinos físicos da NUCLEO-F030R8 (LED LD2 em `PA5`, USART2 TX em `PA2`, RX em `PA3`). |
| [**`STM32F030R8TX_FLASH.ld`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/STM32F030R8TX_FLASH.ld) | **Firmware (STM32)** | **Linker Script de Memória:** Define as regiões físicas de hardware do microcontrolador: 64 KB de Flash ROM para código executável e 8 KB de SRAM estática com proteção de pilha (*Stack* de 1.024 bytes). |
| [**`auditoria_serial_30_testes.csv`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/auditoria_serial_30_testes.csv) | **Auditoria (Dados)** | **Tabela de Conformidade Formal:** Registro em formato tabular legível contendo o tempo de execução (ms), maior erro absoluto ($L_\infty$), erro médio (MAE), erro relativo de Frobenius (ppm), desvio em ULPs e status de aprovação de cada um dos 30 testes. |
| [**`auditoria_serial_30_testes.json`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/auditoria_serial_30_testes.json) | **Auditoria (Dados)** | **Registro Estruturado para CI/CD:** Contém as mesmas métricas em formato JSON estruturado, incluindo taxa de aprovação percentual (100.0%) e parâmetros da máquina para auditoria automatizada. |
| [**`painel_auditoria_serial.png`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/painel_auditoria_serial.png) | **Auditoria (Imagem)** | **Painel Gráfico Consolidado:** Imagem gerada com Matplotlib apresentando o gráfico de controle de erro máximo ao longo dos 30 testes contra a linha de tolerância de Higham, o histograma de ULPs e o gráfico de latência por matriz. |
| [**`arquitetura.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/arquitetura.md) | **Documentação** | **Especificação Teórica e Arquitetural:** Fundamenta matematicamente a inviabilidade de alocar 24,3 KB em 8 KB de RAM, deduz o orçamento de memória do *Tile-Strip Streaming*, detalha a pinagem física da USART2 e apresenta os diagramas de sequência. |
| [**`tutorial.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md) | **Documentação** | **Guia Prático de Bancada:** Tutorial com o passo a passo de configuração gráfica dos periféricos no STM32CubeMX (`.ioc`), configuração de clock, compilação no CubeIDE e comandos de disparo no VS Code. |

---

## 5. Protocolo Serial Simplificado de Alta Performance

A comunicação opera de forma direta em regime de **Request-Response** síncrono:

| Comando | Origem $\rightarrow$ Destino | Payload Enviado | Resposta da Placa | Descrição da Operação |
| :---: | :---: | :---: | :---: | :--- |
| **`'P'`** (`0x50`) | Host $\rightarrow$ STM32 | *(vazio)* | **`'O'`** (`0x4F`) | **CMD_PING:** Handshake inicial de conectividade e sincronismo da porta serial. |
| **`'A'`** (`0x41`) | Host $\rightarrow$ STM32 | `[bi: 1B]` + `[bi * 45 floats]` (até 1.440 B) | **`'K'`** (`0x4B`) | **CMD_STRIP_A:** Transmite a faixa horizontal de $A$. A placa armazena e confirma com `'K'`. |
| **`'B'`** (`0x42`) | Host $\rightarrow$ STM32 | `[bj: 1B]` + `[45 * bj floats]` (até 1.440 B) | `[bi * bj floats]` (até 256 B) | **CMD_BLOCK_B:** Transmite bloco de colunas de $B$. STM32 calcula e devolve o bloco de $C$. |

---

## 6. Orçamento de Memória SRAM (NUCLEO-F030R8: 8 KB)

| Estrutura de Memória | Tamanho Máximo | Função e Alocação |
| :--- | :---: | :--- |
| `buffer_A[8 * 45]` | 1.440 bytes | Faixa horizontal ativa da matriz $A$ na SRAM |
| `buffer_B[45 * 8]` | 1.440 bytes | Sub-bloco vertical recebido da matriz $B$ |
| `bloco_C_res[8 * 8]` | 256 bytes | Sub-bloco resultante sendo acumulado e transmitido |
| Pilha do Sistema (*Stack*) | 1.024 bytes | Pilha de chamadas de funções da CPU ARM |
| **Total Alocado** | **~4.160 bytes** | **Mais de 4.000 bytes livres (50% de margem de folga!)** |

---

## 7. Como Executar a Bateria dos 30 Testes

### No Terminal do VS Code:
```powershell
# Modo de simulação offline (sem necessidade de placa física):
python src_t2/runner_serial.py --simular

# Modo com placa NUCLEO-F030R8 conectada na porta USB:
python src_t2/runner_serial.py
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
  -> Teste 01/30 | Tempo:    0.6 ms | Erro Máx: 1.525879e-04 | ULPs: 2.50 | [APROVADO]
  -> Teste 02/30 | Tempo:    0.4 ms | Erro Máx: 1.220703e-04 | ULPs: 2.00 | [APROVADO]
  ...
  -> Teste 30/30 | Tempo:    0.4 ms | Erro Máx: 1.220703e-04 | ULPs: 2.00 | [APROVADO]

================================================================================
RELATÓRIO CONSOLIDADO: 30/30 TESTES APROVADOS (100.0%)
Tempo Total: 0.03 s (Média: 1.1 ms por matriz)
Relatório CSV salvo em:  src_t2/audit/auditoria_serial_30_testes.csv
Relatório JSON salvo em: src_t2/audit/auditoria_serial_30_testes.json
================================================================================
```

---

## 8. Auditoria Gráfica no Jupyter Notebook

Abra [`src_t2/framework_testes.ipynb`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) no VS Code e execute para visualizar os mapas de controle estatístico, dispersão de resíduos e histogramas de ULPs de toda a bateria de testes.
