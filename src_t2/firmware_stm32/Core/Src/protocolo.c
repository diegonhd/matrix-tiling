/**
 * ============================================================================
 * @file        protocolo.c
 * @brief       Implementação de apoio ao protocolo serial simplificado.
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Este módulo provê a camada de apoio ao protocolo serial de comunicação direta
 * entre o Host PC e o firmware embarcado. O protocolo opera diretamente por chamadas
 * de HAL_UART_Receive e HAL_UART_Transmit no main.c, sem necessidade de parser FSM complexo.
 *
 *
 * ENTRADA E SAÍDA DE DADOS:
 * - Entrada: Bytes de controle recebidos pela USART2.
 * - Saída: Bytes de confirmação transmitidos pela USART2.
 *
 * COMO USAR (HELP):
 * O módulo atua como retaguarda declarativa para o protocolo.h no firmware.
 *
 * PLATAFORMA ALVO:
 * - Placa STM32 NUCLEO-F030R8 (MCU ARM Cortex-M0 @ 48 MHz).
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Implementação das rotinas defensivas de validação de opcodes
 *                   e dimensionamento de payloads para transações DMA.
 * - Código Legado:  Não aplicável.
 * - Código Gerado:  Não aplicável.
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
--  Motivo: Implementação de apoio ao protocolo simplificado.
--          O protocolo opera diretamente por chamadas de HAL_UART_Receive
--          e HAL_UART_Transmit no main.c, sem necessidade de parser FSM complexo.
-------------------------------------------------------------
**/

#include "protocolo.h"
#include "multi_tiling.h"

// By Diego & André: #1. Apoio ao Protocolo e Rotinas de Validação
// A integridade e entrega confiável são garantidas fisicamente pelo
// canal USB do ST-LINK (Virtual COM Port), dispensando sobrecarga de CRC.

/**
 * @brief Valida se o byte recebido pela USART2 corresponde a um opcode reconhecido pelo protocolo.
 * 
 * @param[in] cmd Byte de comando a validar.
 * 
 * @return uint8_t Retorna 1 se for um opcode válido (CMD_PING, CMD_STRIP_A ou CMD_BLOCK_B); 
 *                 retorna 0 caso contrário.
 * 
 * @note Variáveis Globais Afetadas:
 *       - Nenhuma (função pura sem efeitos colaterais).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
uint8_t protocolo_is_valid_cmd(uint8_t cmd)
{
    return (cmd == CMD_PING || cmd == CMD_STRIP_A || cmd == CMD_BLOCK_B) ? 1 : 0;
}

/**
 * @brief Calcula a quantidade exata de bytes para transferência via DMA da faixa horizontal de A.
 *        Aplica saturação defensiva para evitar estouro de buffer caso bi > BLOCK.
 * 
 * @param[in] bi Quantidade de linhas ativas da faixa de A (máximo BLOCK = 8).
 * 
 * @return uint16_t Total de bytes correspondente (bi * N * sizeof(float), até 1.440 B).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Nenhuma (função pura sem efeitos colaterais).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
uint16_t protocolo_calcular_bytes_faixa_A(uint8_t bi)
{
    if (bi > BLOCK) {
        bi = BLOCK;
    }
    return (uint16_t)(bi * N * sizeof(float));
}

/**
 * @brief Calcula a quantidade exata de bytes para transferência via DMA do bloco vertical de B.
 *        Aplica saturação defensiva para evitar estouro de buffer caso bj > BLOCK.
 * 
 * @param[in] bj Quantidade de colunas ativas do bloco de B (máximo BLOCK = 8).
 * 
 * @return uint16_t Total de bytes correspondente (N * bj * sizeof(float), até 1.440 B).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Nenhuma (função pura sem efeitos colaterais).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
uint16_t protocolo_calcular_bytes_bloco_B(uint8_t bj)
{
    if (bj > BLOCK) {
        bj = BLOCK;
    }
    return (uint16_t)(N * bj * sizeof(float));
}
// End by Diego & André
