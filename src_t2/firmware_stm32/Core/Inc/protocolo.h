/**
 * ============================================================================
 * @file        protocolo.h
 * @brief       Definição do protocolo de comunicação serial direta (Request-Response).
 *
 * @details
 * APRESENTAÇÃO GERAL DA APLICAÇÃO:
 * Header que define os estados do protocolo serial simplificado de comunicação direta entre o
 * Host PC e a placa NUCLEO-F030R8 (MCU ARM Cortex-M0 STM32F030R8T6).
 *

 * ENTRADA E SAÍDA DE DADOS:
 * - Tipo de Dado de Controle: Caracteres escalares ASCII de 1 byte ('P', 'A', 'B', 'O', 'K').
 * - Tipo de Dado Numérico: Ponto flutuante IEEE 754 de precisão simples (float, 32 bits / 4 bytes).
 * - Entrada: Comandos de 1 byte e parâmetros escalares de dimensão de bloco (bi, bj).
 * - Saída: Respostas de confirmação ('O', 'K') e sub-blocos calculados de C via DMA.
 *
 * COMO USAR (HELP):
 * 1. Inclua este cabeçalho nos módulos de comunicação: #include "protocolo.h"
 * 2. Utilize os opcodes nomeados (CMD_PING, CMD_STRIP_A, CMD_BLOCK_B) para decodificação.
 * 3. Utilize as constantes nomeadas de timeout nas chamadas de recepção e transmissão HAL.
 *
 * PLATAFORMA ALVO:
 * - Microcontrolador: STMicroelectronics STM32F030R8T6 (ARM Cortex-M0 @ 48 MHz).
 * - Placa: STM32 NUCLEO-F030R8.
 * - Interface: USART2 (PA2=TX, PA3=RX) conectada ao ST-LINK a 460.800 bps.
 *
 * AUTORES / ESTUDANTES:
 * Diego Amorim Vidal & André Queiroz Medeiros
 *
 * DATA:
 * 31 de Outubro de 2026
 *
 * INDICAÇÃO DE ORIGEM DO CÓDIGO:
 * - Código Próprio: Definição dos opcodes de sincronismo ('P', 'A', 'B'), respostas ('O', 'K'),
 *                   constantes de temporização (timeouts), estados de DMA e protótipos de validação.
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
--  Motivo: Definição dos opcodes de comando e resposta do protocolo
--          serial simplificado de comunicação direta STM32 <-> Host PC.
-------------------------------------------------------------
--  #2.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Definição de constantes de timeout e status de DMA para
--          eliminar números mágicos na comunicação serial a 460.800 bps.
-------------------------------------------------------------
**/

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>

// By Diego & André: #1. Comandos e Respostas do Protocolo Serial
// Opcodes de 1 byte transmitidos via USART2 (ST-LINK VCP)
#define CMD_PING      'P'  /* 0x50: Verifica prontidão e presença da placa */
#define CMD_STRIP_A   'A'  /* 0x41: Envia faixa horizontal de até 8 linhas de A */
#define CMD_BLOCK_B   'B'  /* 0x42: Envia bloco vertical de 8 colunas de B e solicita cálculo */

#define RESP_OK       'O'  /* 0x4F: Resposta afirmativa ao CMD_PING */
#define RESP_ACK      'K'  /* 0x4B: Confirmação de recebimento da faixa de A */
// End by Diego & André

// By Diego & André: #2. Constantes de Comunicação e Timeout
// Eliminação de números mágicos para temporização e controle de DMA
#define BAUD_RATE_PADRAO    460800  /* Taxa da USART2 conectada ao VCP */
#define TIMEOUT_CMD_MS      10      /* Timeout curto de polling para escuta de comandos */
#define TIMEOUT_PARAM_MS    1000    /* Timeout para recepção de dimensão escalar (bi/bj) */
#define TIMEOUT_DMA_MS      3000    /* Timeout defensivo para término de transferência DMA */
#define TIMEOUT_RESP_MS     100     /* Timeout de envio de byte de resposta ('O'/'K') */
#define QTD_TESTES_PADRAO   30      /* Bateria estatística dos testes matriciais */

/* Estados de sincronização das transações DMA */
#define DMA_STATUS_IDLE     0  /* Aguardando término de transferência */
#define DMA_STATUS_OK       1  /* Transferência concluída com sucesso */
#define DMA_STATUS_ERROR    2  /* Erro de comunicação ou timeout */
// End by Diego & André

// By Diego & André: #1. Funções de Validação e Dimensionamento do Protocolo
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
uint8_t protocolo_is_valid_cmd(uint8_t cmd);

/**
 * @brief Calcula a quantidade exata de bytes para transferência via DMA da faixa horizontal de A.
 * 
 * @param[in] bi Quantidade de linhas ativas da faixa de A (máximo BLOCK = 8).
 * 
 * @return uint16_t Total de bytes correspondente (bi * N * sizeof(float), até 1.440 B).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Nenhuma (função pura sem efeitos colaterais).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
uint16_t protocolo_calcular_bytes_faixa_A(uint8_t bi);

/**
 * @brief Calcula a quantidade exata de bytes para transferência via DMA do bloco vertical de B.
 * 
 * @param[in] bj Quantidade de colunas ativas do bloco de B (máximo BLOCK = 8).
 * 
 * @return uint16_t Total de bytes correspondente (N * bj * sizeof(float), até 1.440 B).
 * 
 * @note Variáveis Globais Afetadas:
 *       - Nenhuma (função pura sem efeitos colaterais).
 *       Origem: Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
 */
uint16_t protocolo_calcular_bytes_bloco_B(uint8_t bj);
// End by Diego & André

#endif /* PROTOCOLO_H */
