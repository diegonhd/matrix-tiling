/**
 * ============================================================================
 * @file        main.h
 * @brief       Cabeçalho principal com mapeamento de hardware da placa NUCLEO-F030R8.
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Header que centraliza as definições de pinagem física da placa NUCLEO-F030R8,
 * configurações de GPIO, periféricos de comunicação (USART2 via ST-LINK VCP) e
 * estados lógicos do LED verde de telemetria LD2.
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Entradas Físicas: USART2 RX (PA3) e Botão de Usuário B1 (PC13).
 * - Saídas Físicas: USART2 TX (PA2) e LED Verde de Usuário LD2 (PA5).
 *
 * COMO USAR (HELP):
 * 1. Inclua este cabeçalho nos módulos fonte: #include "main.h"
 * 2. Utilize os identificadores simbólicos USART_TX_Pin, LD2_Pin etc.
 *
 * PLATAFORMA ALVO:
 * - Microcontrolador: STMicroelectronics STM32F030R8T6 (ARM Cortex-M0 @ 48 MHz).
 * - Placa: NUCLEO-F030R8 (MB1136).
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Gerado:  Mapeamento de pinagem física e macros de GPIO da placa NUCLEO-F030R8
 *                   gerados pelo STM32CubeMX / STM32CubeIDE.
 * - Código Próprio: Constantes adicionais de controle de LED de telemetria e documentação Doxygen.
 * - Código Legado:  Inclusão de bibliotecas de periféricos da STMicroelectronics (stm32f0xx_hal.h).
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
--  Motivo: Mapeamento de hardware da placa NUCLEO-F030R8 (MCU: STM32F030R8T6).
--          Define pinos de GPIO, periféricos (USART2) e estados do LED LD2.
-------------------------------------------------------------
**/

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STM32F030x8) || defined(STM32F0xx)
  #include "stm32f0xx_hal.h"
#elif defined(STM32F103xB) || defined(STM32F1xx)
  #include "stm32f1xx_hal.h"
#endif

/**
 * @brief Tratador de exceções para falhas críticas de hardware e inicialização de periféricos.
 *        Desativa todas as interrupções mascaráveis e trava a execução em laço infinito preventivo.
 * 
 * @param None (void).
 * 
 * @return void (Função divergente; não retorna).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Modifica o registrador de controle de interrupções da CPU ARM Cortex-M0 (PRIMASK),
 *         desabilitando globalmente as interrupções via instrução __disable_irq().
 *       Origem: Código gerado (STM32CubeMX / HAL).
 */
void Error_Handler(void);

// By Diego & André: #1. Mapeamento físico de pinos da NUCLEO-F030R8
#define B1_Pin                 GPIO_PIN_13
#define B1_GPIO_Port           GPIOC

#define USART_TX_Pin           GPIO_PIN_2
#define USART_TX_GPIO_Port     GPIOA

#define USART_RX_Pin           GPIO_PIN_3
#define USART_RX_GPIO_Port     GPIOA

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

#ifndef LED_ON_STATE
  #define LED_ON_STATE         GPIO_PIN_SET
  #define LED_OFF_STATE        GPIO_PIN_RESET
#endif
// End by Diego & André

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
