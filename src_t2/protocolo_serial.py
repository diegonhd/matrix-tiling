"""
Módulo de Protocolo Serial Simplificado para a Tarefa 2 (STM32 HIL).
Comunicação direta em streaming por blocos (Request-Response).

Sem FSM complexa e sem CRC: a integridade e entrega confiável são
garantidas pelo próprio canal USB (Virtual COM Port do ST-LINK).
"""

import numpy as np

# Constantes de Dimensão
N = 45
BLOCK = 8

# Caracteres de Comando (1 byte cada)
CMD_PING    = b'P'  # Verifica prontidão da placa
CMD_STRIP_A = b'A'  # Envia faixa horizontal de A
CMD_BLOCK_B = b'B'  # Envia bloco vertical de B e solicita cálculo de C

# Caracteres de Resposta (1 byte cada)
RESP_OK     = b'O'  # Resposta afirmativa do PING
RESP_ACK    = b'K'  # Confirmação de recebimento da faixa de A
