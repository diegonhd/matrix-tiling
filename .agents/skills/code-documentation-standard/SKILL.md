---
name: code-documentation-standard
description: >-
  Diretrizes e padrões para documentação completa de código-fonte (C/C++, Python) e estruturação de README para entregas acadêmicas e projetos de sistemas embarcados. Use sempre que o usuário solicitar documentar código, adicionar cabeçalhos, documentar funções em padrão Doxygen, classificar autoria (próprio, gerado, legado) ou estruturar o README do projeto.
---

# Padrão de Documentação de Código e README

Este guia estabelece os requisitos e padrões para comentários, documentação de código-fonte e elaboração de READMEs em projetos de software acadêmicos e embarcados (como o projeto Matrix Tiling).

---

## 1. Estrutura do Cabeçalho de Arquivo

Todo arquivo-fonte (`.c`, `.h`, `.cpp`, `.py`) deve conter no topo um bloco de cabeçalho padronizado contendo:

1. **Apresentação Geral da Aplicação / Módulo**: Descrição concisa do papel do arquivo e sua finalidade no sistema.
2. **Autores / Estudantes**: Nomes completos dos integrantes da equipe (ex.: Diego e André Queiroz Medeiros).
3. **Data**: Data de criação ou da última atualização/entrega.
4. **Plataforma Alvo**: Microcontrolador/arquitetura (ex.: STM32F411RE / ARM Cortex-M4) ou ambiente host (ex.: Python 3 no PC).
5. **Permissões de Uso / Copyright**: Declaração explícita de licença e permissões (ex.: uso exclusivamente acadêmico).
6. **Como Usar (Help)**: Instruções práticas de compilação, execução e integração do módulo.
7. **Entrada e Saída (I/O)**: Especificação precisa dos tipos e estruturas de dados manipulados (ex.: matrizes lineares em float32, dimensões, pacotes binários).
8. **Indicação de Autoria do Código**:
   * **Código Próprio**: Funções e algoritmos desenvolvidos diretamente pelo grupo.
   * **Código Gerado**: Código gerado automaticamente por IDE/ferramenta (ex.: STM32CubeMX, HAL).
   * **Código Legado / Adaptado**: Códigos adaptados de literatura, bibliotecas ou semestres anteriores, com citação explícita da fonte.

### Template de Cabeçalho em C / C++

```c
/**
 * @file        multi_tiling.h
 * @brief       Implementação do algoritmo de multiplicação de matrizes com técnica de Tiling em blocos.
 *
 * @details     AUTORES:
 *              - Diego
 *              - André Queiroz Medeiros
 *
 *              DATA DE ENTREGA / REVISÃO:
 *              31 de Outubro de 2026
 *
 *              PLATAFORMA ALVO:
 *              STM32F411RE-Nucleo (ARM Cortex-M4 com FPU)
 *
 *              PERMISSÕES DE USO / COPYRIGHT:
 *              Uso exclusivamente didático e acadêmico. Proibida a comercialização.
 *
 *              COMO USAR (HELP):
 *              Inclua este arquivo no projeto C e chame a função matrix_multiply_tiled(...)
 *              fornecendo ponteiros válidos e alocados para as matrizes A, B e C.
 *
 *              ENTRADA E SAÍDA:
 *              - Entrada: Matrizes A (M x K) e B (K x N) com elementos float32 contíguos em memória.
 *              - Saída:   Matriz C (M x N) em float32 resultante de A x B.
 *
 *              CLASSIFICAÇÃO DE AUTORIA:
 *              - Código Próprio: Lógica de tiling em múltiplos laços e tratamento de resto.
 *              - Código Legado/Adaptado: Base matemática fundamentada em Higham (2002).
 */
```

### Template de Cabeçalho em Python

```python
"""
runner_serial.py
================
Apresentação Geral:
    Script de orquestração no Host PC para execução de testes em bancada via Hardware-in-the-Loop (HIL).
    Realiza o envio de matrizes para o microcontrolador via UART e a coleta do resultado para validação.

Autores:
    - Diego
    - André Queiroz Medeiros

Data:
    31 de Outubro de 2026

Plataforma Alvo:
    Host PC (Python 3.10+) conectado via porta COM / USB-UART ao STM32F411RE.

Permissões de Uso:
    Uso estritamente acadêmico.

Como Usar (Help):
    python runner_serial.py --port COM3 --baudrate 115200 --tests 30

Entrada e Saída:
    - Entrada: Matrizes de teste geradas em memória (float32, 45x45).
    - Saída:   Logs de teste, comparação com ground truth do NumPy e métricas de desvio (ULPs / erro relativo).

Classificação de Autoria:
    - Código Próprio: Orquestração de teste e protocolo serial.
    - Código Legado: Estrutura de cálculo de ULPs baseada no IEEE-754.
"""
```

---

## 2. Documentação de Funções (Padrão Doxygen)

> [!IMPORTANT]
> **Regra Obrigatória:** Todas as funções devem ter documentação. Isso inclui descrição do comportamento e dos parâmetros de entrada e de retorno (quando for o caso). Indicar as variáveis globais afetadas pela função.

Toda função deve ser acompanhada de bloco documental descrevendo:
1. **Comportamento geral** da função (o que ela faz e como opera).
2. **Parâmetros de entrada (`@param[in]`)**: Tipos, significados e restrições obrigatórias de limites/tamanho de buffers.
3. **Parâmetros de saída (`@param[out]`)**: Onde a saída é gravada e **aviso explícito se há sobrescrita de buffer**.
4. **Valor de Retorno (`@return`)**: Significado de cada código ou status retornado (quando for o caso).
5. **Variáveis Globais Afetadas**: Lista explícita das variáveis globais lidas ou modificadas (ou declaração formal de nenhuma / função reentrante).

### Exemplo em C

```c
/**
 * @brief  Multiplica duas matrizes bidimensionais utilizando particionamento em blocos (tiling).
 *
 * @details Executa o produto matricial C = A x B dividindo o espaço de iteração em sub-blocos
 *          de dimensão fixa BLOCK_SIZE para otimizar o reúso de dados no pipeline/cache.
 *          ATENÇÃO: A matriz C é completamente sobrescrita durante a execução.
 *
 * @param[in]  A           Ponteiro para a matriz A (dimensão M x K, contígua em float32). Não nulo.
 * @param[in]  B           Ponteiro para a matriz B (dimensão K x N, contígua em float32). Não nulo.
 * @param[out] C           Ponteiro para a matriz de saída C (dimensão M x N). Buffer pré-alocado.
 * @param[in]  dim_m       Número de linhas de A e C (restrição: 1 <= dim_m <= MAX_MATRIX_DIM).
 * @param[in]  dim_k       Número de colunas de A / linhas de B (restrição: 1 <= dim_k <= MAX_MATRIX_DIM).
 * @param[in]  dim_n       Número de colunas de B e C (restrição: 1 <= dim_n <= MAX_MATRIX_DIM).
 *
 * @return     int         0 em caso de sucesso;
 *                         -1 caso algum ponteiro seja nulo ou dimensão inválida.
 *
 * @note       Variáveis Globais Afetadas: Nenhuma (função reentrante e autocontida).
 *             Sobrescrita: O conteúdo anterior do buffer apontado por C é descartado.
 */
int matrix_multiply_tiled(const float *A, const float *B, float *C,
                          int dim_m, int dim_k, int dim_n);
```

---

## 3. Diretrizes de Código Interno e Constantes

Para evitar penalizações comuns em avaliações de código:

1. **Evitar Comentários Óbvios**:
   * ❌ Incorreto: `i++; // incrementa i`
   * ✔️ Correto: `// Avança para a próxima coluna considerando o resto da borda (N % BLOCK_SIZE)`
2. **Constantes Nomeadas em vez de Números Mágicos**:
   * Substitua literais como `256`, `64`, `30`, `45` por `#define` ou `const` autoexplicativas:
     ```c
     #define BUFFER_UART_MAX_BYTES    256
     #define TILE_BLOCK_SIZE           8
     #define NUMERO_RODADAS_TESTE      30
     #define MATRIZ_DIM_PADRAO         45
     ```
3. **Nomenclatura Semântica**:
   * Evite nomes abreviados como `na`, `nb`, `nc`, `buf`, `tmp`. Use `linhas_matriz_a`, `colunas_matriz_b`, `buffer_recepcao_dma`.
4. **Fontes Externas e Referências Bibliográficas**:
   * Sempre que utilizar algoritmos de terceiros ou formulas analíticas, inclua a citação bibliográfica completa:
     * Exemplo: `Higham, N. J. (2002). Accuracy and Stability of Numerical Algorithms. SIAM, p. 75.`

---

## 4. Requisitos Específicos para o README de Entrega

O README de entrega deve ser direto, técnico e focado estritamente em:

1. **Hierarquia dos Arquivos Complementares**:
   * Exibição visual da árvore de diretórios e arquivos presentes no `.zip` entregue.
2. **Função de Cada Arquivo no Projeto**:
   * Tabela ou lista pontual indicando claramente o papel de cada arquivo na solução.
3. **Motivação Arquitetural do Transporte Serial / UART**:
   * Justificar por que a abordagem adotada foi o transporte UART com *Hardware-in-the-Loop* (HIL):
     * **Limitação de Memória Flash/SRAM**: Evita gravação estática de múltiplas matrizes de teste na memória Flash do MCU, preservando ciclos de escrita e economizando SRAM.
     * **Escalabilidade e Automação**: Permite testes contínuos automatizados disparados pelo Host PC em tempo real.

### Template de README

````markdown
# Projeto de Multiplicação de Matrizes com Tiling (src_t2)

## 1. Estrutura e Hierarquia de Arquivos

```text
src_t2/
├── Core/
│   ├── Inc/
│   │   ├── main.h               # Definições globais, pinagens e mapeamentos HAL
│   │   ├── multi_tiling.h       # Protótipos e constantes do algoritmo de Tiling
│   │   └── protocolo.h          # Definições do protocolo serial e estados da máquina
│   └── Src/
│       ├── main.c               # Ponto de entrada do firmware, inicializações e laço HIL
│       ├── multi_tiling.c       # Implementação do cálculo matricial particionado
│       └── protocolo.c          # Parser de pacotes UART, framing e checksum
├── scripts/
│   ├── runner_serial.py         # Orquestrador HIL em Python: envio, recepção e validação
│   └── protocolo_serial.py      # Codificador/Decodificador binário no Host
└── README.md                    # Descrição da hierarquia e funções dos arquivos
```

## 2. Função de Cada Arquivo no Projeto

| Arquivo | Camada | Função Principal |
| :--- | :--- | :--- |
| `main.c` | Firmware (Core) | Inicializa periféricos (UART/DMA), relógio e gerencia a recepção e despacho de comandos. |
| `main.h` | Firmware (Core) | Exporta pinos, manipuladores de periféricos e configurações gerais do hardware. |
| `multi_tiling.c / .h` | Algoritmo | Implementa a multiplicação particionada em blocos para reúso de registradores/SRAM. |
| `protocolo.c / .h` | Comunicação | Monta e desmonta frames binários recebidos via UART, verificando integridade. |
| `runner_serial.py` | Host / HIL | Lê dados de teste, envia blocos via serial, recebe o resultado e valida via NumPy/ULPs. |
| `protocolo_serial.py` | Host / HIL | Serializa tipos `float32` em pacotes estruturados compatíveis com o firmware. |

## 3. Motivação da Arquitetura Serial (UART / HIL)

A transmissão contínua das matrizes via UART foi adotada devido a:
* **Preservação de Memória Flash/SRAM**: O microcontrolador possui SRAM limitada (128 KB) e Flash (512 KB). Gravar previamente centenas de matrizes de teste ocuparia a memória de programa desnecessariamente.
* **Automação de Bancada (Hardware-in-the-Loop)**: O script Host pode injetar dinamicamente dezenas de matrizes de teste com diferentes distribuições numéricas e verificar a precisão do ponto flutuante em tempo real.
````

---

## 5. Checklist de Validação Pré-Entrega

Antes de finalizar qualquer entrega de documentação:
- [ ] Todos os arquivos contêm o cabeçalho completo com os dois autores (Diego e André Queiroz Medeiros).
- [ ] Todas as funções têm documentação (descrição do comportamento, parâmetros de entrada com restrições e limites, parâmetros de retorno quando for o caso, indicação explícita de sobrescrita e variáveis globais afetadas).
- [ ] Não há números mágicos sem `#define` ou `const` nomeada.
- [ ] Nomes de variáveis são claros e descritivos (sem abreviações opacas).
- [ ] O README foca na hierarquia, papel dos arquivos e motivação da comunicação serial.
- [ ] Código foi classificado entre próprio, gerado e legado.
