# 🚀 Multiplicação de Matrizes com Tiling: Da Computação em Host à Otimização Extrema em Sistemas Embarcados

Este repositório documenta a trajetória completa de desenvolvimento, validação estatística e portabilidade de um motor de multiplicação de matrizes quadradas ($N = 45$, 2025 elementos em ponto flutuante IEEE 754 de precisão simples `float32`) utilizando a técnica de particionamento em blocos (**Tiling / Blocking**, $B = 8$).

O projeto é dividido em duas etapas complementares de engenharia organizadas na forma de um **pipeline de execução desacoplado e contínuo**, onde a primeira etapa estabelece a base de dados e a referência analítica para a validação da segunda:

```text
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                               PIPELINE GERAL DE EXECUÇÃO DO PROJETO                             │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘

     ┌───────────────────────────────────────────────────────────────────────────────────────┐
     │  🔬 TAREFA 1 (T1): COMPUTADOR HOST (x86_64) — GERAÇÃO, TILING & ORÁCULO MATEMÁTICO    │
     │                                                                                       │
     │  [1. Geração de Entradas] ──> src_t1/input_matrix/ (30 pares + 3 unitários em float32)│
     │             │                                                                         │
     │             ├───> [2. Motor C Tiling (B=8)]  ──> src_t1/bin_c/ (C_1..30.bin)          │
     │             │                                                                         │
     │             ├───> [3. Gabarito NumPy]        ──> src_t1/bin_py/ (C_1..30.bin)         │
     │             │                                                                         │
     │             └───> [4. Auditoria Metrológica] ──> src_t1/audit/ (Relatórios CSV/JSON)  │
     └──────────────────────────────────────────┬────────────────────────────────────────────┘
                                                │ Matrizes A_k e B_k compartilhadas
                                                ▼
     ┌───────────────────────────────────────────────────────────────────────────────────────┐
     │  ⚡ TAREFA 2 (T2): CO-PROCESSADOR EMBARCADO (STM32 HIL) — STREAMING SERIAL (8 KB RAM) │
     │                                                                                       │
     │  [1. Firmware Bare-Metal] ──> NUCLEO-F030R8 (Cortex-M0, 8 KB SRAM, USART2 460.800 bps)│
     │                                    ▲                                                  │
     │                         Handshake  │  Tile-Strip Streaming (Faixas A & Blocos B)      │
     │                         ('P'/'O')  │  Retorno Sub-blocos C[8][8] (256 B)              │
     │                                    ▼                                                  │
     │  [2. Orquestrador Host]   <──> [3. Processamento no Silício (~3,1 KB SRAM)]           │
     │             │                                                                         │
     │             ├───> [4. Reconstrução Matricial] ──> Grava C.bin na raiz do projeto      │
     │             ├───> [5. Validação Tripla HIL]   ──> STM32 vs Motor C vs NumPy           │
     │             └───> [6. Painel de Auditoria]    ──> src_t2/audit/ (CSV, JSON e Gráficos)│
     └───────────────────────────────────────────────────────────────────────────────────────┘
```

---

## ⚙️ Detalhamento dos Pipelines de Execução

### 🔬 Pipeline da Tarefa 1 (T1) — Computação em Host x86_64 e Auditoria

O pipeline da Tarefa 1 opera integralmente na máquina hospedeira e visa produzir os dados de teste, executar a computação particionada clássica em C e auditar rigorosamente os resultados contra um oráculo em Python:

1. **Fase 1: Síntese e Geração das Matrizes de Entrada:**
   - O gerador [`matrix_gen.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_gen.c) sintetiza 30 pares de matrizes $45 \times 45$ com distribuição uniforme bipolar $[-10{,}0, +9{,}9]$ sob sementes determinísticas, além de 3 casos limítrofes (Identidade, Zeros e Matriz Forçada).
   - As matrizes são gravadas como binários puros (*raw float32*, 8.100 bytes cada) no diretório [`src_t1/input_matrix/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/input_matrix/).
2. **Fase 2: Multiplicação com Tiling Clássico em C:**
   - O executável [`matrix_multi.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_multi.c) lê os pares $A$ e $B$ do disco e executa o algoritmo particionado com 6 laços aninhados (tamanho de bloco $B = 8$).
   - Os resultados são salvos em formato binário bruto em [`src_t1/bin_c/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/bin_c/).
3. **Fase 3: Computação do Gabarito Analítico em Python:**
   - O script [`matrix_multi.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_multi.py) processa os mesmos arquivos via rotinas vetorizadas BLAS/NumPy em precisão simples (`np.float32`), gerando as matrizes de referência absoluta em [`src_t1/bin_py/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/bin_py/).
4. **Fase 4: Auditoria Metrológica Independente:**
   - O notebook [`framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/framework_testes.ipynb) é executado para ler unicamente os binários em disco, calculando o maior erro absoluto ($L_\infty$), desvios em ULPs, distância de Frobenius e verificação criptográfica SHA-256, gerando os laudos em [`src_t1/audit/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/audit/).

```powershell
# Execução sequencial completa do pipeline da T1:
gcc -O2 -Wall -Wextra src_t1/matrix_gen.c -o matrix_gen; ./matrix_gen
gcc -O3 -Wall -Wextra src_t1/matrix_multi.c -o matrix_multi; ./matrix_multi
python src_t1/matrix_multi.py
jupyter execute src_t1/framework_testes.ipynb
```

---

### ⚡ Pipeline da Tarefa 2 (T2) — Acelerador Embarcado STM32 HIL (*Hardware-in-the-Loop*)

O pipeline da Tarefa 2 transporta a carga de trabalho para um microcontrolador com apenas **8 KB de SRAM**, utilizando o protocolo serial de *Tile-Strip Streaming* para nunca exceder a memória física da placa:

> [!NOTE]
> **Canal de Comunicação e Mecanismo de Transferência:**  
> A transmissão física ocorre via **UART (USART2 a 460.800 bps)** pelo cabo USB (ST-LINK VCP). Internamente no microcontrolador, todo o transporte em massa de dados (faixas de $A$, blocos de $B$ e blocos de $C$) é gerenciado pelo controlador **DMA (Direct Memory Access - Canais 4 e 5 do DMA1)**, garantindo que o núcleo Cortex-M0 não sofra sobrecarga de interrupção nem perda de dados (*overrun*).

1. **Fase 1: Inicialização e Handshake Serial:**
   - O microcontrolador NUCLEO-F030R8 roda o firmware bare-metal [`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c) e aguarda comandos na USART2 a 460.800 bps.
   - O orquestrador host [`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py) detecta a porta COM via [`auto_detectar_porta_stlink()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L33-L43) e envia o *ping* `'P'`, aguardando a resposta `'O'` para validar a integridade do canal.
2. **Fase 2: Streaming de Matrizes por Faixas e Blocos via DMA:**
   - Para cada um dos 30 testes em [`src_t1/input_matrix/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/input_matrix/), o Host envia a matriz $A$ fatiada em faixas horizontais de 8 linhas (`CMD_STRIP_A` + 1.440 bytes). O STM32 recebe o payload via DMA1 Canal 5 diretamente na SRAM e confirma o recebimento com `'K'`.
   - Em seguida, o Host envia sucessivos blocos verticais de $B$ (`CMD_BLOCK_B` + 1.440 bytes), também recebidos via DMA.
3. **Fase 3: Computação no Silício e Retorno via DMA:**
   - O módulo [`multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c) processa e acumula o produto dos blocos em um buffer de saída de apenas $8 \times 8$ floats (**256 bytes em SRAM**).
   - O resultado de cada bloco é transmitido de volta para o Host via DMA1 Canal 4 imediatamente pela serial.
4. **Fase 4: Reconstrução Matricial e Gravação de `C.bin`:**
   - O Host recebe os blocos calculados e monta a matriz completa $45 \times 45$ em memória, gravando o arquivo binário consolidado [`C.bin`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/C.bin) na raiz do projeto.
5. **Fase 5: Validação Metrológica Tripla em Tempo Real:**
   - O Host confronta instantaneamente a matriz do STM32 contra o gabarito NumPy em precisão simples, validando a tolerância e os desvios em ULPs para cada caso de teste.
6. **Fase 6: Relatórios Consolidados e Painel Gráfico:**
   - Ao término dos 30 conjuntos, o script emite os relatórios formais em [`src_t2/audit/`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/audit/) (`.csv` e `.json`) e permite a visualização das cartas de controle estatístico no notebook [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb).

```powershell
# Execução do pipeline da T2 (com a placa conectada via USB):
python src_t2/runner_serial.py
jupyter execute src_t2/framework_testes.ipynb
```

---

## 📁 Estrutura Geral do Repositório

```text
matrix-tiling/
├── .venv/                              # Ambiente virtual Python (numpy, matplotlib, jupyter)
├── requirements.txt                    # Dependências Python do projeto
├── README.md                           # Documentação central do repositório (este arquivo)
│
├── src_t1/                             # 🔬 TAREFA 1: Host x86_64, Validação Estatística e Auditoria
│   ├── README.md                       # Detalhamento completo da T1 e guia de execução
│   ├── matrix_gen.c                    # Gerador de matrizes de teste (30 estatísticos + 3 unitários)
│   ├── matrix_multi.c                  # Motor em C com Tiling clássico (salva em bin_c/)
│   ├── matrix_multi.py                 # Motor de referência em Python NumPy (salva em bin_py/)
│   ├── framework_testes.ipynb          # Auditoria metrológica independente (ULPs, Frobenius, Hashes)
│   ├── fundamentacao_teorica.ipynb     # Fundamentação: IEEE 754, limite de Higham e estabilidade numérica
│   ├── input_matrix/                   # Matrizes binárias de entrada (8.100 bytes cada, float32)
│   ├── bin_c/                          # Matrizes C resultantes (float32) geradas pelo motor em C
│   ├── bin_py/                         # Matrizes C de referência (float32) geradas pelo NumPy
│   └── audit/                          # Logs formais em CSV e JSON com registros da auditoria
│
└── src_t2/                             # ⚡ TAREFA 2: Co-Processador Serial em Tempo Real (STM32 HIL, 8 KB SRAM)
    ├── README.md                       # Especificação da T2 e arquitetura de co-processador serial
    ├── arquitetura.md                  # Especificação formal da arquitetura serial HIL
    ├── tutorial.md                     # Guia passo a passo de pinagem, USART2 DMA e execução
    ├── dificuldades_e_solucoes.md      # Resumo dos desafios de engenharia, soluções e locais no código
    ├── runner_serial.py                # Script Host Python: orquestra envio dos 30 testes e validação
    ├── protocolo_serial.py             # Módulo de protocolo serial simplificado (comandos 'P', 'A', 'B')
    ├── framework_testes.ipynb          # Auditoria gráfica e análise estatística dos 30 testes
    ├── audit/                          # Relatórios formais gerados (CSV com métricas dos 30 testes)
    │   └── auditoria_serial_30_testes.csv
    └── firmware_stm32/                 # Firmware Bare-Metal para NUCLEO-F030R8 (C / HAL)
        ├── STM32F030R8TX_FLASH.ld      # Linker script oficial (8 KB RAM, 64 KB Flash)
        └── Core/
            ├── Inc/ (main.h, protocolo.h, multi_tiling.h, stm32f0xx_it.h)
            └── Src/ (main.c, protocolo.c, multi_tiling.c, stm32f0xx_hal_msp.c, stm32f0xx_it.c)
```

---

## 🧭 Comparativo Entre as Etapas T1 e T2

| Aspecto | Tarefa 1 (T1) — Host x86_64 | Tarefa 2 (T2) — Embarcado STM32 |
| :--- | :--- | :--- |
| **Plataforma** | Computador PC (Intel/AMD x86_64) | Microcontrolador (NUCLEO-F030R8 / STM32F103) |
| **Arquitetura da CPU** | x86_64 multi-core com FPU hardware (SSE/AVX) | ARM Cortex-M0 / Cortex-M3 (**Sem FPU em hardware**) |
| **Memória RAM** | Gigabytes de memória disponível | **8.192 bytes (8 KB)** rigorosamente limitados |
| **Alocação de Matrizes** | $A$, $B$ e $C$ completas na RAM ($24{,}3\text{ KB}$) | **Tile Streaming:** apenas **256 bytes** de buffer em SRAM |
| **Geração de Saída** | Gravação de `C.bin` via chamadas POSIX no PC | Gravação direta na **Flash interna** (`0x0800E000`) sem Semihosting |
| **Verificação Matemática** | Comparação em disco contra NumPy em Jupyter | **Framework Metrológico Triplo (`framework_testes.ipynb`):** STM32 vs C vs Python |

---

## 🗺️ Mapa de Navegação Rápida (Onde se Aprofundar no Código)

Para explorar diretamente as implementações sem se perder nos detalhes:

### 🔬 Tarefa 1 (Host x86_64)
* **Geração de Entradas:** [`matrix_gen.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_gen.c) — Cria os 30 pares de matrizes aleatórias bipolares e casos unitários (Identidade, Zeros, Matriz Forçada).
* **Multiplicação com Tiling Clássico:** [`matrix_multi.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_multi.c) — 6 laços aninhados dividindo a multiplicação em blocos $8 \times 8$ em disco.
* **Gabarito em Python:** [`matrix_multi.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/matrix_multi.py) — Multiplicação vetorizada com NumPy para referência analítica.
* **Auditoria Metrológica:** [`src_t1/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/framework_testes.ipynb) — Verificação de conformidade numérica ($L_\infty$, ULPs, Frobenius e SHA-256).

### ⚡ Tarefa 2 (STM32 HIL & Streaming Serial)
* **Orquestrador Host (PC):** [`runner_serial.py`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py)
  - [`auto_detectar_porta_stlink()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L33-L43) — Localiza automaticamente a placa conectada na USB.
  - [`processar_matriz_remota()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/runner_serial.py#L62-L99) — Fatia a matriz $A$ em faixas, envia blocos de $B$ e reconstrói $C$.
* **Loop de Comandos no Silício:** [`main.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c)
  - [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L63-L159) — Inicialização de hardware (clock 48 MHz, DMA1, USART2) e loop de escuta para `'P'`, `'A'` e `'B'`.
* **Motor de Tiling Embarcado:** [`multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c)
  - [`multi_tiling_set_faixa_A()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L34-L46) — Grava a fatia ativa de $A$ na SRAM com proteção de limites.
  - [`multi_tiling_computar_bloco()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L74-L120) — Multiplica e acumula o bloco $8 \times 8$ em apenas 256 bytes de memória.
* **Painel Gráfico de Auditoria:** [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) — Cartas de controle estatístico (SPC) e dispersão de resíduos.

---

## 🎯 Links Diretos para Documentação Detalhada

- Para entender os detalhes da geração de matrizes, cálculo de ULPs e auditoria de 100% de aprovação na T1: consulte [**`src_t1/README.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/README.md).
- Para entender as restrições de 8 KB de SRAM, a solução do Tile Streaming e a geração de `C.bin` na T2: consulte [**`src_t2/README.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/README.md).
- Para o passo a passo de configuração e demonstração prática no **STM32CubeIDE**: consulte [**`src_t2/tutorial.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md).
