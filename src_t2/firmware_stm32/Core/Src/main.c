/**
 * @file           : main.c
 * @brief          : Firmware Bare-Metal Acelerador para NUCLEO-F030R8 (Tarefa 2)
 *                   Co-processador Serial de Multiplicação Matricial (HIL)
 *                   com Clock de 48 MHz (PLL) e USART2 acelerada via DMA.
 * 
 * Arquitetura de Comunicação:
 * 1. PING: Recebe 'P' -> Responde 'O'.
 * 2. FAIXA A: Recebe 'A' + [bi: 1B] + [bi * 45 floats via DMA] -> Guarda na RAM e responde 'K'.
 * 3. BLOCO B: Recebe 'B' + [bj: 1B] + [45 * bj floats via DMA] -> Computa bloco C[bi][bj]
 *             em 256 B de SRAM e devolve os floats via DMA de volta para o PC.
 */

#include "main.h"
#include "protocolo.h"
#include "multi_tiling.h"
#include <string.h>

/* Manipuladores de periféricos HAL e DMA */
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* Flags voláteis de sincronização para interrupções de DMA */
static volatile uint8_t dma_rx_status = 0; /* 0: aguardando, 1: sucesso, 2: erro */
static volatile uint8_t dma_tx_status = 0; /* 0: aguardando, 1: sucesso, 2: erro */

/* Buffers de Trabalho na SRAM (Total: ~3,1 KB, ampla folga nos 8 KB da placa) */
static float buffer_A[BLOCK * N];        /* Até 1.440 bytes */
static float buffer_B[N * BLOCK];        /* Até 1.440 bytes */
static float bloco_C_res[BLOCK * BLOCK]; /* Apenas 256 bytes */
static uint8_t bi_atual = 0;

/* Protótipos de inicialização de hardware */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);

/* Callbacks disparados pelo subsistema HAL ao término das transferências DMA */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_rx_status = 1;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_tx_status = 1;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        dma_rx_status = 2;
        dma_tx_status = 2;
    }
}

int main(void)
{
    /* 1. Inicializa subsistemas HAL e Clock HSI + PLL (48 MHz) */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART2_UART_Init();

    /* 2. Confirmação visual de boot: pisca LED LD2 (PA5) 3 vezes */
    for (int i = 0; i < 6; i++) {
        HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
        HAL_Delay(80);
    }
    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, LED_ON_STATE);

    /* 3. Loop Principal: Escuta de comandos com recepção acelerada por DMA */
    while (1)
    {
        uint8_t cmd = 0;

        /* Aguarda 1 byte de comando pela USART2 */
        if (HAL_UART_Receive(&huart2, &cmd, 1, 10) == HAL_OK)
        {
            HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

            /* CASO 1: PING ('P') - Host testa conexão */
            if (cmd == CMD_PING) {
                uint8_t resp = RESP_OK;
                HAL_UART_Transmit(&huart2, &resp, 1, 100);
            }

            /* CASO 2: FAIXA DE A ('A') - Host envia fatia horizontal */
            else if (cmd == CMD_STRIP_A) {
                uint8_t bi = 0;
                /* Lê a quantidade de linhas da faixa (bi = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bi, 1, 1000) == HAL_OK && bi <= BLOCK)
                {
                    bi_atual = bi;
                    uint16_t bytes_A = bi * N * sizeof(float);

                    /* Recebe toda a faixa diretamente na RAM usando DMA sem overhead de CPU */
                    dma_rx_status = 0;
                    if (HAL_UART_Receive_DMA(&huart2, (uint8_t *)buffer_A, bytes_A) == HAL_OK) {
                        uint32_t t_inicio = HAL_GetTick();
                        while (dma_rx_status == 0 && (HAL_GetTick() - t_inicio < 3000)) {
                        }

                        if (dma_rx_status == 1) {
                            multi_tiling_set_faixa_A(bi, buffer_A);
                            /* Confirma com 'K' */
                            uint8_t ack = RESP_ACK;
                            HAL_UART_Transmit(&huart2, &ack, 1, 100);
                        } else {
                            HAL_UART_AbortReceive(&huart2);
                        }
                    }
                }
            }

            /* CASO 3: BLOCO DE B ('B') - Host envia bloco vertical e solicita C */
            else if (cmd == CMD_BLOCK_B) {
                uint8_t bj = 0;
                /* Lê a quantidade de colunas do bloco (bj = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bj, 1, 1000) == HAL_OK && bj <= BLOCK) {
                    uint16_t bytes_B = N * bj * sizeof(float);

                    /* Recebe o bloco de B via DMA */
                    dma_rx_status = 0;
                    if (HAL_UART_Receive_DMA(&huart2, (uint8_t *)buffer_B, bytes_B) == HAL_OK) {
                        uint32_t t_inicio = HAL_GetTick();
                        while (dma_rx_status == 0 && (HAL_GetTick() - t_inicio < 3000)) {
                        }

                        if (dma_rx_status == 1) {
                            /* Computa o produto matricial particionado em apenas 256 B de SRAM */
                            multi_tiling_computar_bloco(bi_atual, bj, buffer_B, bloco_C_res);

                            /* Devolve imediatamente os floats calculados para o Host PC via DMA */
                            uint16_t bytes_C = bi_atual * bj * sizeof(float);
                            dma_tx_status = 0;
                            if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)bloco_C_res, bytes_C) == HAL_OK) {
                                uint32_t tx_inicio = HAL_GetTick();
                                while (dma_tx_status == 0 && (HAL_GetTick() - tx_inicio < 3000)) {
                                }
                            } else {
                                HAL_UART_Transmit(&huart2, (uint8_t *)bloco_C_res, bytes_C, 1000);
                            }
                        } else {
                            HAL_UART_AbortReceive(&huart2);
                        }
                    }
                }
            }
        }
    }
}

/**
  * @brief Configuração de Clock: HSI (8 MHz) com PLL ativado para 48 MHz (máximo estável).
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
  * @brief Inicialização da USART2 (460.800 bps, 8N1 via ST-LINK VCP).
  */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 460800;
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
  * @brief Inicialização do Controlador DMA1 e prioridade de IRQ.
  */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* Configuração de interrupção compartilhada DMA1 Canal 4/5 */
    HAL_NVIC_SetPriority(DMA1_Channel4_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_5_IRQn);
}

/**
  * @brief Inicialização das portas GPIO (LED LD2 PA5 e Botão B1 PC13).
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

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
