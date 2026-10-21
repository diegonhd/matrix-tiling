/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Cabeçalho principal com mapeamento de hardware da placa
 *                   NUCLEO-F030R8 (MCU: STM32F030R8T6, ARM Cortex-M0).
 *                   Define pinos de GPIO, periféricos de comunicação (USART2)
 *                   e estados lógicos do LED de telemetria.
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Inclusão condicional da camada HAL (Hardware Abstraction Layer) da ST.
 * Permite portabilidade entre as famílias STM32F0 (alvo deste projeto)
 * e STM32F1 caso haja migração de placa no futuro.
 */
#if defined(STM32F030x8) || defined(STM32F0xx)
  #include "stm32f0xx_hal.h"
#elif defined(STM32F103xB) || defined(STM32F1xx)
  #include "stm32f1xx_hal.h"
#endif

/**
 * @brief Protótipo da rotina de captura de erros críticos de hardware.
 * Trava a CPU em loop infinito e desabilita interrupções se uma falha de clock
 * ou periférico for detectada durante a inicialização.
 */
void Error_Handler(void);

/* =========================================================================
 * MAPEAMENTO FÍSICO DE PINOS DA PLACA NUCLEO-F030R8
 * ========================================================================= */

/* 
 * Botão de Usuário B1 (Botão Azul):
 * Conectado ao pino PC13 com resistor de pull-up externo na placa.
 * Pressionar o botão gera nível lógico LOW (0).
 */
#define B1_Pin                 GPIO_PIN_13
#define B1_GPIO_Port           GPIOC

/* 
 * Linha de Transmissão Serial USART2 TX:
 * Conectada ao pino PA2. Na placa NUCLEO-F030R8, esse pino é roteado
 * internamente para o microcontrolador do ST-LINK (circuito de depuração on-board),
 * gerando a porta Virtual COM Port (VCP) vista no PC pelo cabo USB.
 */
#define USART_TX_Pin           GPIO_PIN_2
#define USART_TX_GPIO_Port     GPIOA

/* 
 * Linha de Recepção Serial USART2 RX:
 * Conectada ao pino PA3. Roteada do ST-LINK VCP diretamente para o PA3,
 * recebendo comandos e blocos de dados enviados pelo Python (Host PC).
 */
#define USART_RX_Pin           GPIO_PIN_3
#define USART_RX_GPIO_Port     GPIOA

/* 
 * LED de Usuário LD2 (LED Verde):
 * Conectado ao pino PA5.
 * Usado como indicador de status visual de bancada:
 * - 3 piscadas rápidas no boot (indica que a CPU reiniciou com sucesso).
 * - Aceso contínuo em repouso (indica prontidão para receber matrizes).
 * - Toggling (inversão rápida) durante o cálculo de matrizes para telemetria visual.
 */
#define LD2_Pin                GPIO_PIN_5
#define LD2_GPIO_Port          GPIOA

#define TMS_Pin                GPIO_PIN_13
#define TMS_GPIO_Port          GPIOA

#define TCK_Pin                GPIO_PIN_14
#define TCK_GPIO_Port          GPIOA

#ifndef LED_STATUS_Pin
  #define LED_STATUS_Pin       GPIO_PIN_5
  #define LED_STATUS_GPIO_Port GPIOA
#endif

/* 
 * Níveis lógicos de acionamento do LED LD2:
 * Na NUCLEO-F030R8, o catodo do LED é aterrado através de resistor,
 * logo nível ALTO (GPIO_PIN_SET) acende o LED e BAIXO (GPIO_PIN_RESET) apaga.
 */
#ifndef LED_ON_STATE
  #define LED_ON_STATE         GPIO_PIN_SET
  #define LED_OFF_STATE        GPIO_PIN_RESET
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
