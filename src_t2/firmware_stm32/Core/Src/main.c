/**
 * ============================================================================
 * @file        main.c
 * @brief       Firmware Bare-Metal Acelerador para NUCLEO-F030R8 (Tarefa 2).
 *              Co-processador Serial de Multiplicação Matricial (HIL)
 *              com Clock de 48 MHz (PLL) e USART2 acelerada via DMA.
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este firmware transforma o microcontrolador ARM Cortex-M0 STM32F030R8T6 em um
 * co-processador matemático autônomo conectado ao Host PC via porta serial USB
 * a 460.800 bps acelerada por DMA.
 * O microcontrolador processa matrizes quadradas de 45 x 45 elementos em ponto
 * flutuante (float32) empregando a técnica de Tile-Strip Streaming para operar
 * sob a severa restrição de 8 KB de SRAM física do chip.
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Tipo de Dado: Ponto flutuante IEEE 754 de precisão simples (float, 32 bits / 4 bytes).
 * - Entrada: Faixas horizontais de A (até 1.440 B) e blocos verticais de B (até 1.440 B)
 *            recebidos via USART2 acelerada por DMA1 Canal 5.
 * - Saída: Sub-blocos C resultantes (até 256 B) transmitidos de volta via DMA1 Canal 4.
 *
 * COMO USAR (HELP):
 * 1. Compile e grave o firmware na placa NUCLEO-F030R8 via STM32CubeIDE.
 * 2. Conecte a placa via cabo micro-USB (o LED LD2 piscará 3 vezes confirmando o boot).
 * 3. Execute o orquestrador no Host PC: python runner_serial.py
 *
 * PLATAFORMA ALVO:
 * - Microcontrolador: STMicroelectronics STM32F030R8T6 (ARM Cortex-M0 @ 48 MHz).
 * - Placa: STM32 NUCLEO-F030R8 (8 KB SRAM, 64 KB Flash).
 * - Comunicação: USART2 (PA2/PA3 via ST-LINK VCP) a 460.800 bps com DMA1 Canais 4 e 5.
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Máquina de estados cooperativa HIL, super-loop de escuta e despacho serial,
 *                   callbacks de interrupção DMA (Rx, Tx, Error), alocação estática dos buffers
 *                   de trabalho na SRAM (buffer_A, buffer_B, bloco_C_res) e sinalização no LED LD2.
 * - Código Gerado:  Funções de inicialização de hardware geradas pelo STM32CubeMX / CubeIDE:
 *                   SystemClock_Config (48 MHz), MX_USART2_UART_Init (460.800 bps),
 *                   MX_DMA_Init (DMA1 Canais 4 e 5), MX_GPIO_Init (PA5, PC13) e Error_Handler.
 * - Código Legado:  Estruturas e drivers da biblioteca STM32CubeF0 HAL (STMicroelectronics).
 *
 * PERMISSÕES DE USO E COPYRIGHT:
 * Copyright (c) 2026 Diego Amorim Vidal & André Queiroz Medeiros.
 * Uso educacional e acadêmico livre mediante citação da fonte.
 * ============================================================================
 */
/*--------------------------------------------------------------
--  #1.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Firmware Bare-Metal Acelerador para NUCLEO-F030R8 (Tarefa 2).
--          Co-processador Serial de Multiplicação Matricial (HIL)
--          com Clock de 48 MHz (PLL) e USART2 acelerada via DMA.
-------------------------------------------------------------
--  #2.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Recepção e transmissão em massa via DMA1 (Canais 4 e 5)
--          para evitar estouro de buffer (ORE) a 460.800 bps.
-------------------------------------------------------------
--  #3.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Sincronismo por flags voláteis e timeouts defensivos com HAL_GetTick().
-------------------------------------------------------------
**/

#include "main.h"
#include "protocolo.h"
#include "multi_tiling.h"
#include <string.h>

/* Manipuladores de periféricos HAL e DMA */
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

// By Diego & André: #3. Flags voláteis de sincronização para interrupções de DMA
static volatile uint8_t dma_rx_status = DMA_STATUS_IDLE; /* 0: aguardando, 1: sucesso, 2: erro */
static volatile uint8_t dma_tx_status = DMA_STATUS_IDLE; /* 0: aguardando, 1: sucesso, 2: erro */
// End by Diego & André

// By Diego & André: #1. Buffers de Trabalho na SRAM (~3,1 KB total, folga nos 8 KB da placa)
// buffer_A e buffer_B são sobrescritos a cada nova recepção de faixa ou bloco.
static float buffer_A[BLOCK * N];        /* Até 1.440 bytes: recepção DMA da faixa de A */
static float buffer_B[N * BLOCK];        /* Até 1.440 bytes: recepção DMA do bloco de B */
static float bloco_C_res[BLOCK * BLOCK]; /* Apenas 256 bytes: sub-bloco C calculado */
static uint8_t bi_atual = 0;
// End by Diego & André

/* Protótipos de inicialização de hardware */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);

// By Diego & André: #2. Callbacks HAL disparados pelas interrupções de DMA

/**
 * @brief Callback disparado pelo subsistema HAL ao concluir recepção via DMA1 Canal 5.
 * 
 * @param[in] huart Ponteiro para a estrutura de controle da UART que gerou a interrupção.
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica a variável global volátil 'dma_rx_status' para DMA_STATUS_OK
 *         quando o periférico disparador for a USART2.
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_rx_status = DMA_STATUS_OK;
    }
}

/**
 * @brief Callback disparado pelo subsistema HAL ao concluir transmissão via DMA1 Canal 4.
 * 
 * @param[in] huart Ponteiro para a estrutura de controle da UART que gerou a interrupção.
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica a variável global volátil 'dma_tx_status' para DMA_STATUS_OK
 *         quando o periférico disparador for a USART2.
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_tx_status = DMA_STATUS_OK;
    }
}

/**
 * @brief Callback disparado pelo subsistema HAL em caso de erro na comunicação USART2.
 *        Trata erros de hardware como Overrun (ORE), ruído (NE) ou enquadramento (FE).
 * 
 * @param[in] huart Ponteiro para a estrutura de controle da UART que sofreu o erro.
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica as variáveis globais voláteis 'dma_rx_status' e 'dma_tx_status'
 *         para DMA_STATUS_ERROR quando o periférico afetado for a USART2.
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_rx_status = DMA_STATUS_ERROR;
        dma_tx_status = DMA_STATUS_ERROR;
    }
}
// End by Diego & André

/**
 * @brief Ponto de entrada principal do firmware co-processador matricial bare-metal.
 * 
 * @details Inicializa subsistemas HAL, clocks (48 MHz), periféricos GPIO, DMA1 e USART2
 *          a 460.800 bps. Realiza rotina de diagnóstico visual piscando o LED LD2 3 vezes.
 *          Em seguida, executa o laço infinito cooperativo de escuta de comandos:
 *          - CMD_PING ('P'): Responde com RESP_OK ('O') confirmando conexão ativa;
 *          - CMD_STRIP_A ('A'): Recebe dimensão bi e a faixa horizontal de A via DMA1 Canal 5,
 *            armazenando-a em buffer estático e respondendo com RESP_ACK ('K');
 *          - CMD_BLOCK_B ('B'): Recebe dimensão bj e o bloco vertical de B via DMA1 Canal 5,
 *            aciona o particionamento matricial multi_tiling_computar_bloco e devolve
 *            o sub-bloco C resultante de volta ao Host PC via DMA1 Canal 4.
 * 
 * @param None (void).
 * 
 * @return int Código de terminação (em regime bare-metal, a função opera em loop infinito).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica 'huart2', 'hdma_usart2_rx', 'hdma_usart2_tx' durante a inicialização.
 *       - Modifica 'dma_rx_status' e 'dma_tx_status' para sincronismo com as interrupções de DMA.
 *       - Sobrescreve 'buffer_A' com a faixa de A recebida via DMA.
 *       - Sobrescreve 'buffer_B' com o bloco de B recebido via DMA.
 *       - Sobrescreve 'bloco_C_res' com o sub-bloco de C computado.
 *       - Atualiza 'bi_atual' com a quantidade de linhas da fatia de A em processamento.
 *       - Altera o estado do pino do LED LD2 (PA5) via registradores de saída do GPIOA.
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros) orquestrando chamadas da HAL.
 */
int main(void)
{
    /* 1. Inicializa subsistemas HAL e Clock HSI + PLL (48 MHz) */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();

    // By Diego & André: #1. Confirmação visual de boot: pisca LED LD2 (PA5) 3 vezes
    for (int i = 0; i < 6; i++) {
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        HAL_Delay(80);
    }
    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, LED_ON_STATE);
    // End by Diego & André

    /* 3. Loop Principal: Escuta de comandos com recepção acelerada por DMA */
    while (1)
    {
        uint8_t cmd = 0;

        /* Aguarda 1 byte de comando pela USART2 */
        if (HAL_UART_Receive(&huart2, &cmd, 1, TIMEOUT_CMD_MS) == HAL_OK)
        {
            HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

            // By Diego & André: #1. CASO 1: PING ('P') - Host testa conexão
            if (cmd == CMD_PING) {
                uint8_t resp = RESP_OK;
                HAL_UART_Transmit(&huart2, &resp, 1, TIMEOUT_RESP_MS);
            }
            // End by Diego & André

            // By Diego & André: #1. CASO 2: FAIXA DE A ('A') - Host envia fatia horizontal
            else if (cmd == CMD_STRIP_A) {
                uint8_t bi = 0;
                /* Lê a quantidade de linhas da faixa (bi = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bi, 1, TIMEOUT_PARAM_MS) == HAL_OK && bi <= BLOCK)
                {
                    bi_atual = bi;
                    uint16_t bytes_A = bi * N * sizeof(float);

                    /* Recebe a faixa diretamente na SRAM via DMA1 Canal 5 sem overhead de CPU */
                    dma_rx_status = DMA_STATUS_IDLE;
                    if (HAL_UART_Receive_DMA(&huart2, (uint8_t *)buffer_A, bytes_A) == HAL_OK) {
                        uint32_t t_inicio = HAL_GetTick();
                        while (dma_rx_status == DMA_STATUS_IDLE && (HAL_GetTick() - t_inicio < TIMEOUT_DMA_MS)) {
                        }

                        if (dma_rx_status == DMA_STATUS_OK) {
                            /* Armazena no buffer estático e confirma com 'K' */
                            multi_tiling_set_faixa_A(bi, buffer_A);
                            uint8_t ack = RESP_ACK;
                            HAL_UART_Transmit(&huart2, &ack, 1, TIMEOUT_RESP_MS);
                        } else {
                            HAL_UART_AbortReceive(&huart2);
                        }
                    }
                }
            }
            // End by Diego & André

            // By Diego & André: #1. CASO 3: BLOCO DE B ('B') - Host envia bloco vertical e solicita C
            else if (cmd == CMD_BLOCK_B) {
                uint8_t bj = 0;
                /* Lê a quantidade de colunas do bloco (bj = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bj, 1, TIMEOUT_PARAM_MS) == HAL_OK && bj <= BLOCK) {
                    uint16_t bytes_B = N * bj * sizeof(float);

                    /* Recebe o bloco de B via DMA1 Canal 5 */
                    dma_rx_status = DMA_STATUS_IDLE;
                    if (HAL_UART_Receive_DMA(&huart2, (uint8_t *)buffer_B, bytes_B) == HAL_OK) {
                        uint32_t t_inicio = HAL_GetTick();
                        while (dma_rx_status == DMA_STATUS_IDLE && (HAL_GetTick() - t_inicio < TIMEOUT_DMA_MS)) {
                        }

                        if (dma_rx_status == DMA_STATUS_OK) {
                            /* Computa o produto matricial particionado em apenas 256 B de SRAM */
                            multi_tiling_computar_bloco(bi_atual, bj, buffer_B, bloco_C_res);

                            /* Devolve os floats calculados para o Host PC via DMA1 Canal 4 */
                            uint16_t bytes_C = bi_atual * bj * sizeof(float);
                            dma_tx_status = DMA_STATUS_IDLE;
                            if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)bloco_C_res, bytes_C) == HAL_OK) {
                                uint32_t tx_inicio = HAL_GetTick();
                                while (dma_tx_status == DMA_STATUS_IDLE && (HAL_GetTick() - tx_inicio < TIMEOUT_DMA_MS)) {
                                }
                            } else {
                                HAL_UART_Transmit(&huart2, (uint8_t *)bloco_C_res, bytes_C, TIMEOUT_PARAM_MS);
                            }
                        } else {
                            HAL_UART_AbortReceive(&huart2);
                        }
                    }
                }
            }
            // End by Diego & André
        }
    }
}

/**
 * @brief Configura a árvore de relógio do sistema para operar a 48 MHz (máximo estável).
 * 
 * @details Ativa o oscilador interno HSI (8 MHz) e configura o multiplicador de frequência
 *          PLL (x6) para alimentar o barramento SYSCLK a 48 MHz. Configura os prescalers
 *          AHB e APB1 com divisão por 1 e ajusta a latência de acesso à memória Flash
 *          para 1 ciclo de espera (FLASH_LATENCY_1).
 * 
 * @param None (void).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica registradores de controle e configuração de clock do RCC (RCC->CR, RCC->CFGR).
 *       - Modifica o registrador de controle de latência de leitura da Flash (FLASH->ACR).
 *       - Atualiza a variável global do sistema HAL 'SystemCoreClock' para 48000000.
 *       - Em caso de falha crítica na inicialização dos osciladores, invoca Error_Handler().
 *       Origem: Código gerado (STM32CubeMX / CubeIDE HAL).
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
    RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Configuração e inicialização da interface serial assíncrona USART2 (ST-LINK VCP).
 * 
 * @details Configura a USART2 nos pinos PA2 (TX) e PA3 (RX) a 460.800 bps, 8 bits de dados,
 *          1 bit de parada, sem paridade (8N1), modo bidirecional pleno (TX/RX) e sem controle
 *          de fluxo por hardware.
 * 
 * @param None (void).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica a variável global de controle 'huart2' (estrutura UART_HandleTypeDef).
 *       - Configura registradores internos de controle e baud rate da USART2 (CR1, CR2, CR3, BRR).
 *       - Em caso de falha na inicialização HAL, invoca Error_Handler().
 *       Origem: Código gerado (STM32CubeMX / CubeIDE HAL).
 */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = BAUD_RATE_PADRAO;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Inicialização do controlador de Acesso Direto à Memória (DMA1) e interrupções NVIC.
 * 
 * @details Habilita o clock do barramento para o controlador DMA1 e configura a prioridade
 *          e habilitação da interrupção compartilhada dos Canais 4 e 5 no controlador NVIC
 *          (DMA1_Channel4_5_IRQn).
 * 
 * @param None (void).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica registradores de clock de periféricos no RCC (RCC->AHBENR).
 *       - Modifica os registradores de prioridade e habilitação de IRQ no NVIC (NVIC->IPR, NVIC->ISER).
 *       Origem: Código gerado (STM32CubeMX / CubeIDE HAL).
 */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* Configuração de interrupção compartilhada DMA1 Canal 4/5 */
    HAL_NVIC_SetPriority(DMA1_Channel4_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_5_IRQn);
}

/**
 * @brief Inicialização das portas de entrada e saída de uso geral (GPIO).
 * 
 * @details Habilita os clocks para as portas GPIOA, GPIOC e GPIOF. Configura o pino PC13
 *          (Botão de Usuário B1) como entrada digital com detecção de borda de descida,
 *          e o pino PA5 (LED de Telemetria LD2) como saída digital push-pull inicializada em nível baixo.
 * 
 * @param None (void).
 * 
 * @return void (Nenhum valor de retorno).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica registradores de habilitação de clock GPIO no RCC (RCC->AHBENR).
 *       - Modifica registradores de modo, tipo de saída, velocidade e pull-up/down dos bancos GPIOA e GPIOC.
 *       Origem: Código gerado (STM32CubeMX / CubeIDE HAL).
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

/**
 * @brief Tratador de exceção para falhas críticas de hardware e inicialização de periféricos.
 * 
 * @details Desativa todas as interrupções mascaráveis do núcleo ARM Cortex-M0 via instrução
 *          __disable_irq() e prende a CPU em laço infinito para manter o sistema em estado seguro.
 * 
 * @param None (void).
 * 
 * @return void (Função divergente; não retorna).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica o registrador de controle da CPU ARM Cortex-M0 (PRIMASK), desabilitando interrupções.
 *       Origem: Código gerado (STM32CubeMX / CubeIDE HAL).
 */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
