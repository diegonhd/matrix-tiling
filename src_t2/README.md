# ⚡ Tarefa 2 (T2): Portabilidade para Microcontrolador com Severa Restrição de Memória (STM32)

> **Diretório:** `src_t2/`  
> **Plataforma de Desenvolvimento:** STM32F103C8T6 (Blue Pill, ARM Cortex-M3, **SRAM travada em 8 KB**)  
> **Plataforma Alvo Final:** Nucleo STM32F030R8T6 (ARM Cortex-M0, **8 KB SRAM nativa**, 64 KB Flash)  
> **Ambiente de Desenvolvimento:** STM32CubeIDE / STM32CubeMX com drivers HAL

---

## 1. O que é pedido na Tarefa 2 (T2)

### 1.1 Objetivo Geral
Portar a aplicação de multiplicação de matrizes da T1 para uma plataforma de microcontrolador com recursos drasticamente inferiores aos de um PC, superando limitações severas de memória (RAM de apenas 8 KB) e desenvolvendo mecanismos embarcados para diagnóstico, instrumentação e validação.

### 1.2 Atividades Obrigatórias
- **Identificar Incompatibilidades e Restrições:** Lidar com ausência de sistema de arquivos (sem `fopen`/disco), ausência de FPU em hardware (emulação por software via libgcc) e, crucialmente, o limite de **8.192 bytes de SRAM**.
- **Preservação Estrita dos Requisitos:** Manter a mesma dimensão de entrada da T1 ($N = 45$ em `float32`).
- **Validação com Zero UART:** Desenvolver procedimentos de teste autônomos sem auxílio de conversor USB-TTL ou terminal serial.
- **Demonstração Física na Placa:** Apresentação funcional (8 minutos) exibindo a validação dos cálculos tanto no hardware físico (LED) quanto via depuração não-intrusiva (SWD via Live Expressions).

### 1.3 Critérios e Pesos de Avaliação
| Aspecto Avaliado | Peso |
| :--- | :---: |
| Funcionamento correto na plataforma indicada (preservação de requisitos) | **25%** |
| Clareza do código para fins de reuso (modularidade e desacoplamento) | **10%** |
| Ajustes para migração de plataforma (grau de dificuldade e aprendizado) | **20%** |
| Procedimentos de teste e validação | **15%** |
| Conhecimento dos conceitos de embarcados durante a apresentação | **30%** |

---

## 2. A Solução Arquitetural de Memória: Tile Streaming

Uma matriz $45 \times 45$ em `float32` consome $8.100\text{ bytes}$.
- Se $A$, $B$ e $C$ fossem alocadas em RAM: consumiriam $24.300\text{ bytes}$ (estouro imediato da SRAM de 8 KB).
- Mesmo se apenas $C$ ficasse em RAM: consumiria $8.100$ dos $8.192\text{ bytes}$ (98,8%), sufocando a pilha (*stack*) e gerando *HardFault*.

### Estratégia de Engenharia Adotada:
1. **Entradas $A$ e $B$ na Flash (`.rodata`):**
   Armazenadas como `static const float` dentro de [`test_data.h`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/test_data.h). Consomem $\approx 16{,}2\text{ KB}$ da Flash (de 64 KB disponíveis), com custo zero de SRAM.
2. **Saída via Tile Streaming (Buffer de 256 Bytes em SRAM):**
   O motor em [`multi_tiling.c`](file:///C:/Users/diego/Desktop/repositorios/matrix-tiling/src_t2/multi_tiling.c) calcula um sub-bloco de $8 \times 8$ por vez em um buffer local estático de **256 bytes**. Ao terminar o bloco, ele despacha os valores para um callback de consumo (BIST) e reutiliza os 256 bytes para o próximo bloco.
3. **Economia:** Redução de **8.100 bytes** para **256 bytes** em SRAM (>96% de economia), liberando >7,5 KB de folga para a pilha de execução.

---

## 3. Estrutura Modular de Arquivos de `src_t2/`

O código foi rigorosamente desacoplado em camadas para maximizar a nota de reuso:

```text
src_t2/
├── Core/Inc/
│   ├── multi_tiling.h          # Motor puro: protótipos de computação em blocos e callbacks
│   ├── bist_suite.h            # Suíte de testes: cenários (Zeros, Identidade, Teste 1) e struct g_report
│   ├── test_data.h             # Entradas constantes em Flash e checksums de referência
│   └── main.h                  # Definições de hardware e pinos do STM32Cube
│
├── Core/Src/
│   ├── multi_tiling.c          # Motor matemático puro: Tiling com Tile Streaming (independente de teste)
│   ├── bist_suite.c            # Implementação do BIST: auditoria online, checagem de erros e tolerância
│   └── main.c                  # Ponto de entrada: inicializa HAL/GPIO e invoca bist_run_suite()
│
├── generate_test_data.py       # Script Python que extrai binários da T1 e compila test_data.h com checksums
├── tutorial.md                 # Guia passo a passo de configuração no STM32CubeIDE e roteiro de demonstração
├── explicacao_t2.md            # Documento vivo da T2 (requisitos, limites de hardware e decisões de projeto)
└── arquitetura_stm32cube.md    # Especificação arquitetural de hardware e software no ecossistema STM32
```

---

## 4. Procedimento de Teste e Demonstração (Sem UART)

Sem dispor de conversores USB-TTL, a validação ocorre em duas frentes complementares:

1. **Demonstração Física por Hardware (LED de Status):**
   - Pino `PC13` (Blue Pill) ou `PA5` (Nucleo).
   - O LED pisca rápido 3 vezes na inicialização.
   - Ao término, se todos os 3 testes passarem com sucesso, o LED fica **aceso fixo contínuo**.
2. **Inspeção em Tempo Real via STM32CubeIDE (*Live Expressions*):**
   - Com a placa conectada via ST-Link (SWD), a struct global `g_report` é monitorada em tempo real:
     - `g_report.tests_passed_count`: Sobe progressivamente de 0 até **3**.
     - `g_report.current_scenario`: Exibe o teste ativo (0: Zeros, 1: Identidade, 2: Geral).
     - `g_report.checksum_calculated`: Iguala os valores exatos de referência do Python.
     - `g_report.status`: Converge para `TEST_PASSED (2)`.
