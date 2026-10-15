/**
 * @file           : protocolo.c
 * @brief          : Implementação de apoio ao protocolo simplificado.
 */

#include "protocolo.h"

/* O protocolo simplificado opera diretamente por chamadas de HAL_UART_Receive
 * e HAL_UART_Transmit no main.c, sem necessidade de parser FSM complexo. */
