# 🚀 Multiplicação de Matrizes com Tiling: Da Computação em Host à Otimização Extrema em Sistemas Embarcados

Este repositório documenta a trajetória completa de desenvolvimento, validação estatística e portabilidade de um motor de multiplicação de matrizes quadradas ($N = 45$, 2025 elementos em ponto flutuante IEEE 754 de precisão simples `float32`) utilizando a técnica de particionamento em blocos (**Tiling / Blocking**, $B = 8$).

O projeto é dividido em duas etapas complementares de engenharia:
1. **Tarefa 1 (T1):** Computação em arquitetura Host x86_64, desacoplamento em arquivos binários brutos (*raw*) e auditoria metrológica independente via Jupyter Notebook.
2. **Tarefa 2 (T2):** Portabilidade para microcontrolador ARM Cortex-M com restrição extrema de memória (**SRAM de apenas 8 KB**), contorno da ausência de UART via BIST (*Built-In Self-Test*) condensado e demonstração física em bancada.

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
└── src_t2/                             # ⚡ TAREFA 2: Portabilidade para Microcontrolador (STM32, 8 KB SRAM)
    ├── README.md                       # Detalhamento completo da T2, restrições e critérios da banca
    ├── Core/Inc/
    │   ├── multi_tiling.h              # Motor puro: protótipos de computação em blocos e streaming
    │   ├── bist_suite.h                # Suíte BIST: orquestração dos 3 cenários e struct g_report
    │   ├── test_data.h                 # Entradas estáticas em Flash e checksums de referência
    │   └── main.h                      # Cabeçalhos do STM32Cube / HAL
    ├── Core/Src/
    │   ├── multi_tiling.c              # Motor puro de Tiling com Tile Streaming (buffer local de 256B)
    │   ├── bist_suite.c                # Implementação da suíte de testes BIST (Zeros, Identidade, Teste 1)
    │   └── main.c                      # Ponto de entrada: inicialização HAL, controle de LED e BIST
    ├── generate_test_data.py           # Conversor de binários da T1 para cabeçalho C em Flash
    ├── tutorial.md                     # Guia passo a passo no STM32CubeIDE e roteiro de demonstração
    ├── explicacao_t2.md                # Documento vivo da T2 (desafios de memória, hardware e decisões)
    └── arquitetura_stm32cube.md        # Especificação da arquitetura de hardware e software
```

---

## 🧭 Comparativo Entre as Etapas T1 e T2

| Aspecto | Tarefa 1 (T1) — Host x86_64 | Tarefa 2 (T2) — Embarcado STM32 |
| :--- | :--- | :--- |
| **Plataforma** | Computador PC (Intel/AMD x86_64) | Microcontrolador (STM32F103 $\rightarrow$ STM32F030) |
| **Arquitetura da CPU** | x86_64 multi-core com FPU hardware (SSE/AVX) | ARM Cortex-M3 / Cortex-M0 (**Sem FPU em hardware**) |
| **Memória RAM** | Gigabytes de memória disponível | **8.192 bytes (8 KB)** rigorosamente limitados |
| **I/O e Armazenamento** | Sistema de arquivos do SO (arquivos `.bin`) | Memória **Flash** (`.rodata`, 64 KB) + **Zero Disco** |
| **Alocação de Matrizes** | $A$, $B$ e $C$ completas na RAM ($24{,}3\text{ KB}$) | **Tile Streaming:** apenas **256 bytes** de buffer em SRAM |
| **Interface de Comunicação** | Terminal CLI / Jupyter Notebook | **Zero UART** (Sinalização via LED + SWD Live Expressions) |
| **Verificação Matemática** | Comparação célula a célula contra NumPy em disco | **BIST condensado** online com tolerância de float32 |

---

## 🎯 Links Diretos para Documentação Detalhada

- Para entender os detalhes da geração de matrizes, cálculo de ULPs e auditoria de 100% de aprovação na T1: consulte [**`src_t1/README.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t1/README.md).
- Para entender as restrições de 8 KB de SRAM, a solução do Tile Streaming e a suíte multi-testes na T2: consulte [**`src_t2/README.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/README.md).
- Para o passo a passo de configuração e demonstração prática no **STM32CubeIDE**: consulte [**`src_t2/tutorial.md`**](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/tutorial.md).
