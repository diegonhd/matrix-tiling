# Explicação da Transição e Desafios da Tarefa T2

> **Documento Vivo de Engenharia & Acompanhamento da T2**  
> Este documento consolida todo o entendimento do projeto, restrições arquiteturais, incompatibilidades, desafios práticos e o plano de evolução contínua da atividade T2.

---

## 1. Objetivos da T2 e Critérios de Avaliação

### 1.1 Objetivos Formais
1. **Portar a aplicação** desenvolvida na T1 para uma plataforma de microcontrolador com muito mais restrições de recursos.
2. **Lidar estritamente com as restrições de memória** (RAM e Flash) da plataforma de destino.
3. **Desenvolver mecanismos práticos** para diagnóstico, instrumentação e validação de problemas em sistemas embarcados.

### 1.2 Atividades Obrigatórias
- Identificar incompatibilidades e restrições da nova plataforma alvo.
- Implementar e validar na plataforma final **Nucleo STM32F030R8T6** (usando a **STM32F103C8T6** como degrau preliminar de testes, já com RAM travada para 8 KB).
- Fazer demonstração do funcionamento **direto na placa física**, evidenciando os testes e aferição.
- Preparar apresentação de resultados (máximo de 8 minutos) destacando domínio dos conceitos de sistemas embarcados.

### 1.3 Critérios e Pesos de Avaliação
| Aspecto | Peso | Foco de Atenção |
| :--- | :---: | :--- |
| **Funcionamento correto na plataforma indicada** | 25% | Manter os requisitos funcionais e integridade matemática da T1. |
| **Clareza do código para fins de reuso** | 10% | Código modular, legível, desacoplado de hardware específico. |
| **Ajustes para migração de plataforma** | 20% | Grau de dificuldade vencido, análise de tradeoffs e aprendizado. |
| **Procedimentos de teste e validação** | 15% | Métodos confiáveis de verificação (mesmo sem UART). |
| **Conhecimento conceitual em embarcados** | 30% | Domínio sobre arquitetura ARM Cortex-M, mapa de memória, linker script, ciclos, cache/buffers. |

---

## 2. Plataformas: Comparativo de Hardware e Restrições

| Recurso | Host (T1 - PC x86_64) | Placa de Testes Atual (Blue Pill / STM32F103C8T6) | Placa Alvo Final (Nucleo STM32F030R8T6) |
| :--- | :--- | :--- | :--- |
| **Núcleo / Arquitetura** | x86_64 multi-core | ARM Cortex-M3 (32 bits, instrução thumb-2) | ARM Cortex-M0 (32 bits, ARMv6-M simplificado) |
| **Frequência Máxima** | ~3-5 GHz | 72 MHz | 48 MHz |
| **Unidade de Ponto Flutuante (FPU)** | Sim (Hardware, SSE/AVX) | **Não** (emulação por software via software-float) | **Não** (emulação por software via software-float) |
| **Memória Flash (ROM)** | GBs / TBs de disco | 64 KB (ou 128 KB não oficial) | 64 KB |
| **Memória SRAM** | 8 a 32+ GB | 20 KB físicos (*travado em 8 KB no linker script*) | **8 KB** |
| **Instruções de Hardware Div/Mult** | Mult e Div em 1 ciclo | Multiplicação por HW (32b), Divisão por HW | Multiplicação por HW limitada (opção 32b lenta/32 ciclos ou 1 ciclo), **SEM divisão por HW** |

---

## 3. O Desafio Crítico de Memória (N = 45 em float32)

No algoritmo original da T1, trabalhamos com matrizes quadradas de dimensão $N = 45$:
$$\text{Elementos por matriz} = 45 \times 45 = 2025 \text{ elementos}$$
$$\text{Tamanho de cada matriz (float32, 4 bytes)} = 2025 \times 4 = 8.100 \text{ bytes} \approx 7{,}91 \text{ KB}$$

### A Conta Impossível na SRAM:
- Matriz $A$: 8.100 bytes
- Matriz $B$: 8.100 bytes
- Matriz $C$: 8.100 bytes
- **Total se tudo ficasse na SRAM:** $3 \times 8.100 = 24.300 \text{ bytes} \approx 23{,}73 \text{ KB}$

> [!CAUTION]
> A SRAM total da STM32F030R8T6 é de **8.192 bytes (8 KB)**.  
> Uma única matriz de 8.100 bytes ocupa **98,8%** de toda a SRAM disponível! Se colocarmos $A$, $B$ e $C$ na RAM, o programa estoura a memória antes mesmo de inicializar o stack ou variáveis do HAL.

### A Solução de Engenharia Adotada:
1. **Entradas $A$ e $B$ fixadas em FLASH (Constantes):**
   - Na T1, as entradas eram lidas de arquivos `.bin`.
   - Na T2, para manter a mesma entrada de dados sem sistema de arquivos, as matrizes de entrada $A$ e $B$ são convertidas pelo script `generate_test_data.py` em vetores estáticos constantes (`static const float A[N][N]` e `static const float B[N][N]`).
   - Sendo `const`, o compilador GCC as aloca na seção `.rodata` da memória **Flash** (ROM), que tem **64 KB**. Duas matrizes em Flash ocupam $16.200 \text{ bytes} \approx 15{,}8 \text{ KB}$, cabendo perfeitamente dentro dos 64 KB.
2. **Matriz de Saída $C$ na SRAM:**
   - Apenas a matriz $C$ (ou blocos de $C$) precisa estar em RAM para ser escrita.
   - Mesmo assim, $C$ completa na RAM consome 8.100 bytes, deixando apenas 92 bytes para Stack, Heap e registradores do HAL.
   - **Trade-off arquitetural:** Na STM32F103 (com 8 KB travados) e na STM32F030, devemos verificar se $C$ inteira cabe ou se o cálculo deve ser emitido via acumulação/streaming em blocos (Tile Streaming).

---

## 4. Limitação Crítica de Instrumentação: Sem UART / Sem Conversor USB-TTL

Você informou que **não possui conversores USB-TTL** no momento e que não é possível debugar via UART serial.

### Como contornar essa restrição mantendo a validação e demonstração exigidas?
1. **Sinalização Física por GPIO (LEDs e Pinos de Diagnóstico):**
   - **LED do Usuário (PC13 na Blue Pill / PA5 na Nucleo):**
     - Piscando rápido: sistema inicializou e está calculando.
     - Aceso contínuo: cálculo finalizado com sucesso e validação (checksum) aprovada.
     - Padrão de erro (SOS / piscadas contínuas): erro de integridade nos dados calculados.
   - **Pino de Profiling (GPIO Toggle para Osciloscópio / Analisador Lógico / Frequencímetro):**
     - Colocar um pino em HIGH no início de `mult_tiling` e LOW ao término. O tempo de cálculo em microssegundos/milissegundos pode ser medido externamente com precisão absoluta.
2. **Depuração via SWD / ST-Link (GDB / OpenOCD / STM32CubeIDE Live Watch):**
   - Como você conecta a placa para gravar via ST-Link (SWD: pinos SWDIO e SWCLK), o depurador permite:
     - **Live Expressions / Memory Watch:** Ler posições de memória de $C$ em tempo real no STM32CubeIDE sem parar o processador.
     - **Breakpoints:** Parar na linha após o cálculo e inspecionar a matriz $C$, ou exportar a memória para conferir no PC.
3. **Mecanismo Embarcado de Auto-Teste (Built-In Self-Test / Checksum):**
   - O microcontrolador computa uma assinatura matemática de $C$ ao término (soma dos elementos, hash FNV-1a ou CRC32 por hardware se disponível).
   - Compara o valor obtido com o checksum esperado já pré-calculado na Flash.
   - Se os valores baterem exatamente, acende o LED de sucesso.

---

## 5. Decisões Técnicas Consolidadas para a T2
- **Configuração no .ioc:**
  - LED em `PC13` (Blue Pill) / `PA5` (Nucleo) no modo `GPIO_Output` Push-Pull.
  - Debug configurado como `Serial Wire` (SWD via `PA13` e `PA14`).
  - Clock configurado em HSI interno (dispensa cristal externo nas duas placas).
- **Algoritmo & Tile Streaming:**
  - Descarte de `multi_tiling_buffers`; foco exclusivo na implementação canônica de `mult_tiling`.
  - Implementação de **Tile Streaming**: cálculo bloco a bloco ($8 \times 8 = 256\text{ bytes}$ em SRAM), eliminando a matriz $C$ completa da RAM e deixando $>7{,}5\text{ KB}$ livres para stack.
- **Validação com Referência Python:**
  - Python pré-calcula as matrizes $A$ e $B$ e um **checksum condensado** da matriz de referência em `test_data.h`.
  - BIST (Built-In Self-Test) acumula o checksum durante o streaming e valida automaticamente.
- **Estratégia de Demonstração (Sem UART):**
  - **Estratégia 1:** LED físico (Piscando = rodando; Aceso fixo = PASS; Piscando lento = FAIL).
  - **Estratégia 3:** STM32CubeIDE Live Expressions (inspeção de `test_status`, `checksum_calculated` e `checksum_expected` em tempo real via ST-Link).
