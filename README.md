# 🚀 Multiplicação de Matrizes com Tiling: Da Computação em Host à Otimização Extrema em Sistemas Embarcados

Este repositório documenta a trajetória completa de desenvolvimento, validação estatística e portabilidade de um motor de multiplicação de matrizes quadradas ($N = 45$, 2025 elementos em ponto flutuante IEEE 754 de precisão simples `float32`) utilizando a técnica de particionamento em blocos (**Tiling / Blocking**, $B = 8$).

O projeto é dividido em duas etapas complementares de engenharia:
1. **Tarefa 1 (T1):** Computação em arquitetura Host x86_64, desacoplamento em arquivos binários brutos (*raw*) e auditoria metrológica independente via Jupyter Notebook.
2. **Tarefa 2 (T2):** Portabilidade para microcontrolador ARM Cortex-M com restrição extrema de memória (**SRAM de apenas 8 KB**), processamento com **Tile Streaming** (buffer de apenas 256 B em RAM), exportação direta de **`C.bin`** na *worktree* do projeto e validação numérica desacoplada em host.

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
    ├── runner_serial.py                # Script Host Python: orquestra envio dos 30 testes e validação
    ├── protocolo_serial.py             # Módulo de frames binários e cálculo de CRC-16 no Python
    ├── framework_testes.ipynb          # Auditoria gráfica e análise estatística dos 30 testes
    ├── audit/                          # Relatórios formais gerados (CSV, JSON, gráficos)
    │   ├── auditoria_serial_30_testes.csv
    │   ├── auditoria_serial_30_testes.json
    │   └── painel_auditoria_serial.png
    └── firmware_stm32/                 # Firmware Bare-Metal para NUCLEO-F030R8 (C / HAL)
        ├── STM32F030R8TX_FLASH.ld      # Linker script oficial (8 KB RAM, 64 KB Flash)
        └── Core/
            ├── Inc/ (main.h, protocolo.h, multi_tiling.h)
            └── Src/ (main.c, protocolo.c, multi_tiling.c)
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
  - [`main()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/main.c#L35-L113) — Inicialização de hardware, USART2 e máquina de estados para `'P'`, `'A'` e `'B'`.
* **Motor de Tiling Embarcado:** [`multi_tiling.c`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c)
  - [`multi_tiling_set_faixa_A()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L34-L46) — Grava a fatia ativa de $A$ na SRAM com proteção de limites.
  - [`multi_tiling_computar_bloco()`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/firmware_stm32/Core/Src/multi_tiling.c#L74-L120) — Multiplica e acumula o bloco $8 \times 8$ em apenas 256 bytes de memória.
* **Painel Gráfico de Auditoria:** [`src_t2/framework_testes.ipynb`](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/framework_testes.ipynb) — Cartas de controle estatístico (SPC) e dispersão de resíduos.

---

## 🎯 Links Diretos para Documentação Detalhada

- Para entender os detalhes da geração de matrizes, cálculo de ULPs e auditoria de 100% de aprovação na T1: consulte [**`src_t1/README.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/README.md).
- Para entender as restrições de 8 KB de SRAM, a solução do Tile Streaming e a geração de `C.bin` na T2: consulte [**`src_t2/README.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/README.md).
- Para o passo a passo de configuração e demonstração prática no **STM32CubeIDE**: consulte [**`src_t2/tutorial.md`**](file:///c:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md).
