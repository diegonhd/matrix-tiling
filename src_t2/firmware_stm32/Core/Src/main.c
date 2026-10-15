/**
 * @file           : main.c
 * @brief          : Firmware Bare-Metal Simplificado para NUCLEO-F030R8 (Tarefa 2)
 *                   Co-processador Serial de Multiplicação Matricial (HIL)
 * 
 * Arquitetura de Comunicação Simplificada:
 * 1. PING: Recebe 'P' -> Responde 'O'.
 * 2. FAIXA A: Recebe 'A' + [bi: 1B] + [bi * 45 floats] -> Guarda na RAM e responde 'K'.
 * 3. BLOCO B: Recebe 'B' + [bj: 1B] + [45 * bj floats] -> Calcula bloco C[8][8] em RAM
 *             e devolve diretamente os [bi * bj floats] de volta para o PC.
 */

#include "main.h"
#include "protocolo.h"
#include "multi_tiling.h"
#include <string.h>

/* Manipuladores de periféricos HAL */
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* Buffers de Trabalho na SRAM (Total: ~3,1 KB, folga total nos 8 KB da placa) */
static float buffer_A[BLOCK * N];       /* Até 1.440 bytes */
static float buffer_B[N * BLOCK];       /* Até 1.440 bytes */
static float bloco_C_res[BLOCK * BLOCK];/* Apenas 256 bytes */
static uint8_t bi_atual = 0;

/* Protótipos de inicialização de hardware */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    /* 1. Inicializa subsistemas HAL e Clock HSI (8 MHz) */
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

    /* 3. Loop Principal: Escuta de comandos simplificada */
    while (1)
    {
        uint8_t cmd = 0;

        /* Aguarda 1 byte de comando pela USART2 */
        if (HAL_UART_Receive(&huart2, &cmd, 1, 10) == HAL_OK) 
        // huart2 são as especificações da porta UART
        // &cmd é o endereço onde sera armazenado os bytes recebidos
        // 1 é o tamanho (size) do dado (1 byte - HAL_OK)
        // 10 é o timeout 

        {
            /* Alterna LED para dar feedback visual de atividade serial */
            HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

            /* CASO 1: PING ('P') - Host testa conexão */
            if (cmd == CMD_PING) {
                uint8_t resp = RESP_OK;
                HAL_UART_Transmit(&huart2, &resp, 1, 100);
                // huart2 é o ponteiro para a porta
                // resp é o ponteiro pro vetor que vai ser transmitido
                // 1 é o tamanho do vetor (1 porque é um RESP_OK == 'P')
                // 100 é o timeout   
            }

            /* CASO 2: FAIXA DE A ('A') - Host envia fatia horizontal */
            else if (cmd == CMD_STRIP_A) {
                uint8_t bi = 0;
                /* Lê a quantidade de linhas da faixa (bi = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bi, 1, 1000) == HAL_OK && bi <= BLOCK) 
                // verificação adicional de bi <= BLOCK pois bi pode assumir de 1 a 255 bytes 
                {
                    // bi atual é a atual posição de memoria 
                    bi_atual = bi; 
                    uint16_t bytes_A = bi * N * sizeof(float);
                    // bytes A pega a quantidade de bytes da matriz A a partir da posição atual

                    /* Lê diretamente todos os floats da faixa para a RAM */
                    if (HAL_UART_Receive(&huart2, (uint8_t *)buffer_A, bytes_A, 2000) == HAL_OK) {
                    // Ponteiro para o buffer_A = 1440 bytes
                        multi_tiling_set_faixa_A(bi, buffer_A); 
                        // Seta os numeros que a faixa atual de A no buffer
                        /* Confirma com 'K' */
                        uint8_t ack = RESP_ACK; // Transmite uma check validation pro STM
                        HAL_UART_Transmit(&huart2, &ack, 1, 100);
                    }
                }
            }

            /* CASO 3: BLOCO DE B ('B') - Host envia bloco vertical e solicita C */
            // mesma logica mas usando buffer de B    
            else if (cmd == CMD_BLOCK_B) {
                uint8_t bj = 0;
                /* Lê a quantidade de colunas do bloco (bj = 8 ou 5) */
                if (HAL_UART_Receive(&huart2, &bj, 1, 1000) == HAL_OK && bj <= BLOCK) {
                    uint16_t bytes_B = N * bj * sizeof(float);
                    /* Lê diretamente todos os floats do bloco de B */
                    if (HAL_UART_Receive(&huart2, (uint8_t *)buffer_B, bytes_B, 2000) == HAL_OK) {
                        
                        /* Computa a multiplicação do bloco C[bi][bj] em apenas 256 B de SRAM */
                        multi_tiling_computar_bloco(bi_atual, bj, buffer_B, bloco_C_res);

                        /* Devolve imediatamente os floats calculados para o Host PC */
                        uint16_t bytes_C = bi_atual * bj * sizeof(float);
                        HAL_UART_Transmit(&huart2, (uint8_t *)bloco_C_res, bytes_C, 2000);
                    }
                }
            }
        }
    }
}

/* Configuração do Clock: HSI 8 MHz interno (estável sem cristal externo) */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

/* Configuração da USART2 (460.800 bps, 8N1 via ST-LINK VCP) */
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
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, LED_OFF_STATE);

    GPIO_InitStruct.Pin = LED_STATUS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_STATUS_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
