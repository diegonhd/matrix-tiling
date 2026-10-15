/**
 * @file           : protocolo.h
 * @brief          : Definição do protocolo de comunicação simplificado (Ping-Pong por blocos).
 *                   Comunicação direta sem FSM complexa e sem CRC (garantido pelo canal USB CDC).
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>

/* Comandos de 1 byte enviados pelo Host PC */
#define CMD_PING      'P'  /* 0x50: Verifica se a placa está online */
#define CMD_STRIP_A   'A'  /* 0x41: Envia faixa horizontal de 8 linhas de A */
#define CMD_BLOCK_B   'B'  /* 0x42: Envia bloco de 8 colunas de B e solicita cálculo */

/* Respostas de 1 byte enviadas pelo STM32 */
#define RESP_OK       'O'  /* 0x4F: Resposta ao PING */
#define RESP_ACK      'K'  /* 0x4B: Confirmação de recebimento da faixa de A */

#endif /* PROTOCOLO_H */
