"""

APRESENTAÇÃO GERAL DA APLICAÇÃO:
Módulo de constantes e parâmetros de comunicação serial direta entre o Host PC
e o microcontrolador STM32F030R8T6. A comunicação opera por streaming de blocos
sem FSM complexa e sem CRC, pois o canal USB CDC do ST-LINK garante integridade física.

ENTRADA E SAÍDA DE DADOS:
- Comandos enviados pelo Host: CMD_PING (b'P'), CMD_STRIP_A (b'A'), CMD_BLOCK_B (b'B').
- Respostas recebidas do STM32: RESP_OK (b'O'), RESP_ACK (b'K').

COMO USAR (HELP):
Importar no runner:
    from protocolo_serial import N, BLOCK, CMD_PING, CMD_STRIP_A, CMD_BLOCK_B, RESP_OK, RESP_ACK

PLATAFORMA ALVO:
- Host PC (Python 3) conectado à placa STM32 NUCLEO-F030R8 via USB VCP a 460.800 bps.

AUTORES / ESTUDANTES:
Diego Amorim Vidal & André Queiroz Medeiros

DATA:
31 de Outubro de 2026

INDICAÇÃO DE ORIGEM DO CÓDIGO:
- Código Próprio: Definição dos opcodes de sincronismo ('P', 'A', 'B'), respostas ('O', 'K')
                  e funções auxiliares de dimensionamento de buffers serial.
- Código Legado:  Dimensão da matriz (N=45) e sub-bloco (BLOCK=8) herdados da Tarefa 1.
- Código Gerado:  Não aplicável.

PERMISSÕES DE USO E COPYRIGHT:
Copyright (c) 2026 Diego Amorim Vidal & André Queiroz Medeiros.
Uso educacional e acadêmico livre mediante citação da fonte.
===============================================================================
--------------------------------------------------------------
--  #1.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Módulo de Protocolo Serial Simplificado para a Tarefa 2 (STM32 HIL).
--          Comunicação direta em streaming por blocos (Request-Response).
--------------------------------------------------------------
"""

# By Diego & André: #1. Constantes de Dimensão e Memória
N = 45      # Dimensão fixa da matriz quadrada (45 x 45)
BLOCK = 8   # Dimensão do sub-bloco de particionamento (8 x 8)

TAM_BLOCO_C_ELEMS = BLOCK * BLOCK      # 64 floats
TAM_BLOCO_C_BYTES = TAM_BLOCO_C_ELEMS * 4  # 256 bytes

# Caracteres de Comando (1 byte cada) enviados pelo Host PC
CMD_PING    = b'P'  # Verifica prontidão da placa
CMD_STRIP_A = b'A'  # Envia faixa horizontal de A
CMD_BLOCK_B = b'B'  # Envia bloco vertical de B e solicita cálculo de C

# Caracteres de Resposta (1 byte cada) enviados pelo STM32
RESP_OK     = b'O'  # Resposta afirmativa do PING
RESP_ACK    = b'K'  # Confirmação de recebimento da faixa de A


def calcular_bytes_faixa_A(bi: int) -> int:
    """
    Calcula a quantidade de bytes a transmitir para a faixa horizontal de A.
    
    Comportamento:
        Multiplica a quantidade de linhas ativas bi pela largura total N (45)
        e pelo tamanho de 4 bytes de cada float32 IEEE 754.
        
    Parâmetros:
        bi (int): Quantidade de linhas ativas da faixa de A (máximo BLOCK = 8).
        
    Retorno:
        int: Total de bytes correspondente (até 1.440 bytes).
        
    Variáveis Globais Afetadas:
        Nenhuma (função pura sem efeitos colaterais).
        
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    return int(min(bi, BLOCK) * N * 4)


def calcular_bytes_bloco_B(bj: int) -> int:
    """
    Calcula a quantidade de bytes a transmitir para o bloco vertical de B.
    
    Comportamento:
        Multiplica a altura total N (45) pela quantidade de colunas ativas bj
        e pelo tamanho de 4 bytes de cada float32 IEEE 754.
        
    Parâmetros:
        bj (int): Quantidade de colunas ativas do bloco de B (máximo BLOCK = 8).
        
    Retorno:
        int: Total de bytes correspondente (até 1.440 bytes).
        
    Variáveis Globais Afetadas:
        Nenhuma (função pura sem efeitos colaterais).
        
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    return int(N * min(bj, BLOCK) * 4)
# End by Diego & André
