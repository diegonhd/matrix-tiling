#!/usr/bin/env python3
"""
===============================================================================

APRESENTAÇÃO GERAL DA APLICAÇÃO:
Script do Host PC responsável por coordenar a bateria dos 30 testes das matrizes
45x45 (float32) com o microcontrolador STM32F030R8T6. Envia faixas de A e blocos
de B, recebe sub-blocos C de volta, calcula o gabarito no NumPy, audita erros
(L_inf e ULPs) e exporta relatório consolidado em CSV.

ENTRADA E SAÍDA DE DADOS:
- Entrada em disco: 'src_t1/input_matrix/A_{id}.bin' e 'B_{id}.bin' (45x45 float32).
- Saída serial: Comandos e blocos binários via USART2 (460.800 bps).
- Entrada serial: Sub-blocos C (float32) devolvidos pelo STM32 via DMA.
- Saída em disco: 'src_t2/audit/auditoria_serial_30_testes.csv'.

COMO USAR (HELP):
python runner_serial.py [--port PORT] [--baud BAUD] [--testes TESTES]

PLATAFORMA ALVO:
- Host PC: Windows/Linux com Python 3.8+ e bibliotecas pyserial e numpy.
- Microcontrolador: STM32F030R8T6 (NUCLEO-F030R8) via ST-LINK VCP.

AUTORES / ESTUDANTES:
Diego Amorim Vidal & André Queiroz Medeiros

DATA:
31 de Outubro de 2026

INDICAÇÃO DE ORIGEM DO CÓDIGO:
- Código Próprio: Orquestração do pipeline de testes HIL, auto-detecção da porta ST-LINK,
                  transmissão particionada em streaming e cálculo de métricas de erro (L_inf, ULPs).
- Código Legado:  Bateria de 30 pares de matrizes binárias de teste herdada da Tarefa 1.
- Código Gerado:  Não aplicável.

PERMISSÕES DE USO E COPYRIGHT:
Copyright (c) 2026 Diego Amorim Vidal & André Queiroz Medeiros.
Uso educacional e acadêmico livre mediante citação da fonte.
===============================================================================
--------------------------------------------------------------
--  #1.
--  Date: Out, 2026
--  Authors: Diego Amorim Vidal & André Queiroz Medeiros
--  Motivo: Orquestrador Serial Host Simplificado
--          Transmite lotes de matrizes (A e B, 45x45, float32) para o STM32 NUCLEO-F030R8,
--          recebe a matriz resultante C calculada por blocos e audita contra o NumPy.
--------------------------------------------------------------
"""

import time
import argparse
import csv
from pathlib import Path
import numpy as np
import serial
import serial.tools.list_ports

from protocolo_serial import (
    N, BLOCK,
    CMD_PING, CMD_STRIP_A, CMD_BLOCK_B,
    RESP_OK, RESP_ACK
)

# By Diego & André: #1. Parâmetros de Comunicação e Auditoria
ATOL_TOLERANCIA = 1.0e-3
BAUD_RATE_PADRAO = 460800
TIMEOUT_PADRAO_S = 2.0

ROOT_DIR = Path(__file__).resolve().parent.parent
DIR_INPUT = ROOT_DIR / "src_t1" / "input_matrix"
DIR_AUDIT = ROOT_DIR / "src_t2" / "audit"
DIR_AUDIT.mkdir(parents=True, exist_ok=True)
# End by Diego & André


def auto_detectar_porta_stlink():
    """
    Varre as portas seriais disponíveis no sistema operacional e identifica a porta COM do ST-Link.
    
    Comportamento:
        Consulta todas as interfaces seriais conectadas utilizando a biblioteca pySerial.
        Filtra os dispositivos comparando as strings descritivas e do fabricante por termos
        como 'stlink' ou 'stm32'. Retorna a porta encontrada ou a única porta disponível no host.
    
    Parâmetros:
        Nenhum.
        
    Retorno:
        str ou None: Nome identificador do dispositivo da porta serial (ex: 'COM7' no Windows,
                     '/dev/ttyACM0' no Linux) ou None caso nenhuma porta compatível seja encontrada.
                     
    Variáveis Globais Afetadas:
        Nenhuma (função de consulta sem efeitos colaterais).
        
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    portas = serial.tools.list_ports.comports()
    for p in portas:
        desc = (p.description or "").lower()
        mfg = (p.manufacturer or "").lower()
        if "stlink" in desc or "stlink" in mfg or "stm32" in desc:
            return p.device
    if len(portas) == 1:
        return portas[0].device
    return None


def ler_exato(porta, n_bytes: int, timeout_s: float = TIMEOUT_PADRAO_S) -> bytes:
    """
    Lê exatamente uma quantidade fixada de bytes da porta serial com controle estrito de timeout.
    
    Comportamento:
        Executa leituras em blocos na porta serial até acumular a quantidade solicitada de n_bytes.
        Caso o tempo decorrido ultrapasse o limite de timeout_s antes de completar a recepção,
        lança uma exceção defensiva TimeoutError com a contagem de bytes recebidos até o momento.
        
    Parâmetros:
        porta (serial.Serial): Instância da porta serial aberta conectada à placa.
        n_bytes (int): Quantidade exata de bytes a receber da interface serial.
        timeout_s (float, opcional): Limite máximo em segundos para conclusão da leitura (padrão: 2.0s).
        
    Retorno:
        bytes: Sequência contígua e imutável de bytes recebidos da placa física.
        
    Exceções:
        TimeoutError: Disparada caso n_bytes não sejam recebidos antes de timeout_s.
        
    Variáveis Globais Afetadas:
        Nenhuma (opera diretamente no buffer de hardware do objeto porta).
        
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    dados = bytearray()
    t_limite = time.time() + timeout_s
    while len(dados) < n_bytes:
        if time.time() > t_limite:
            raise TimeoutError(f"Timeout ao aguardar {n_bytes} bytes (recebidos {len(dados)})")
        resto = n_bytes - len(dados)
        chunk = porta.read(resto)
        if chunk:
            dados.extend(chunk)
        else:
            time.sleep(0.001)
    return bytes(dados)


def processar_matriz_remota(porta, A: np.ndarray, B: np.ndarray) -> np.ndarray:
    """
    Coordena o envio e processamento particionado das matrizes A e B pelo co-processador STM32 (HIL).
    
    Comportamento:
        Orquestra a estratégia de Tile-Strip Streaming para contornar o limite de 8 KB de SRAM do microcontrolador:
        1. Fatiamento de A: Itera pelas faixas horizontais de A (tamanho BLOCK = 8). Para cada faixa, envia
           o comando CMD_STRIP_A ('A'), a quantidade de linhas bi e os bytes contíguos de float32.
           Aguarda a confirmação síncrona RESP_ACK ('K') da placa.
        2. Fatiamento de B e cálculo de C: Para a faixa ativa de A, itera pelos blocos verticais de B
           (tamanho BLOCK = 8). Envia o comando CMD_BLOCK_B ('B'), a largura bj e os bytes de B.
           Recebe de volta os floats calculados do sub-bloco C (bi x bj) transmitidos via DMA pelo firmware.
        3. Montagem: Reconstrói a matriz resultante C completa de dimensão N x N (45 x 45 float32).
        
    Parâmetros:
        porta (serial.Serial): Instância da porta serial aberta com a NUCLEO-F030R8.
        A (np.ndarray): Matriz bidimensional de entrada A (dimensão 45 x 45, dtype float32).
        B (np.ndarray): Matriz bidimensional de entrada B (dimensão 45 x 45, dtype float32).
        
    Retorno:
        np.ndarray: Matriz resultante C (dimensão 45 x 45, dtype float32) computada pelo co-processador.
        
    Exceções:
        RuntimeError: Se a placa responder com caractere de sincronismo divergente de RESP_ACK ('K').
        TimeoutError: Se ocorrer timeout durante a transmissão ou recepção serial.
        
    Variáveis Globais Afetadas:
        Nenhuma (função de transformação de matrizes que opera sobre a porta serial).
        
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    C_resultado = np.zeros((N, N), dtype=np.float32)

    # Itera pelas faixas horizontais de A (tamanho BLOCK = 8)
    for ii in range(0, N, BLOCK):
        bi = min(ii + BLOCK, N) - ii
        faixa_A = A[ii:ii + bi, :].astype(np.float32)

        # Envia: 'A' + [bi] + [bytes da faixa]
        porta.write(CMD_STRIP_A + bytes([bi]))
        time.sleep(0.001)
        porta.write(faixa_A.tobytes())

        # Aguarda confirmação 'K'
        ack = ler_exato(porta, 1)
        if ack != RESP_ACK:
            raise RuntimeError(f"Erro de sincronismo na faixa A (esperava 'K', recebeu {ack})")

        # Itera pelos blocos verticais de B (tamanho BLOCK = 8)
        for jj in range(0, N, BLOCK):
            bj = min(jj + BLOCK, N) - jj
            sub_B = B[:, jj:jj + bj].astype(np.float32)

            # Envia: 'B' + [bj] + [bytes do bloco]
            porta.write(CMD_BLOCK_B + bytes([bj]))
            time.sleep(0.001)
            porta.write(sub_B.tobytes())

            # Recebe exatamente bi * bj floats calculados (em bytes)
            qtd_bytes = bi * bj * 4
            dados_C = ler_exato(porta, qtd_bytes)

            # Converte de volta para float32 e insere na matriz final
            bloco_C = np.frombuffer(dados_C, dtype=np.float32).reshape((bi, bj))
            C_resultado[ii:ii + bi, jj:jj + bj] = bloco_C

    return C_resultado


def main():
    """
    Ponto de entrada do script orquestrador de ensaios e validação estatística HIL.
    
    Comportamento:
        Processa argumentos da linha de comando (--port, --baud, --testes). Conecta-se à porta serial da placa
        NUCLEO-F030R8 e realiza o teste de prontidão via comando CMD_PING ('P').
        Executa a bateria de 30 testes matriciais carregando as matrizes binárias de entrada (A e B).
        Compara o resultado retornado pelo co-processador com o gabarito computado pelo NumPy em float32,
        extraindo a norma infinita (erro absoluto máximo) e o desvio em ULPs (Unit in the Last Place).
        Gera e salva o relatório consolidado em formato CSV no diretório 'src_t2/audit/'.
        
    Parâmetros:
        Nenhum (os parâmetros são obtidos via linha de comando através de argparse).
        
    Retorno:
        None.
        
    Variáveis Globais Afetadas:
        - Modifica o sistema de arquivos local criando e sobrescrevendo o arquivo CSV
          'src_t2/audit/auditoria_serial_30_testes.csv'.
          
    Origem:
        Código próprio (Diego Amorim Vidal & André Queiroz Medeiros).
    """
    parser = argparse.ArgumentParser(description="Runner Serial Simplificado (STM32 HIL) para a Tarefa 2")
    parser.add_argument("--port", type=str, default=None, help="Porta serial (ex: COM7)")
    parser.add_argument("--baud", type=int, default=BAUD_RATE_PADRAO, help="Baud rate (default: 460800)")
    parser.add_argument("--testes", type=int, default=30, help="Quantidade de testes (default: 30)")
    args = parser.parse_args()

    porta_nome = args.port or auto_detectar_porta_stlink()

    print("=" * 80)
    print("CO-PROCESSADOR SERIAL SIMPLIFICADO: STM32 HIL (STREAMING POR BLOCOS)")
    print("=" * 80)

    print(f"[*] Conectando à placa física na porta {porta_nome} a {args.baud} bps...")
    try:
        porta = serial.Serial(porta_nome, args.baud, timeout=TIMEOUT_PADRAO_S)
        time.sleep(0.1)
        porta.reset_input_buffer()
        porta.reset_output_buffer()
        # Envia PING ('P') e espera resposta ('O')
        porta.write(CMD_PING)
        resp = ler_exato(porta, 1, timeout_s=3.0)
        if resp != RESP_OK:
            print(f"[AVISO] Resposta inesperada ao PING ({resp}).")
        else:
            print("[OK] Conexão com NUCLEO-F030R8 estabelecida com sucesso!")
    except Exception as e:
        raise RuntimeError(f"[AVISO] Falha ao abrir porta serial ({e}).")

    resultados_auditoria = []
    total_testes = args.testes
    aprovados = 0

    print(f"\nIniciando execução contínua de {total_testes} conjuntos de matrizes (45x45 float32):")
    print("-" * 80)

    for teste_id in range(1, total_testes + 1):
        arq_A = DIR_INPUT / f"A_{teste_id}.bin"
        arq_B = DIR_INPUT / f"B_{teste_id}.bin"

        if not arq_A.exists() or not arq_B.exists():
            print(f"[AVISO] Arquivos do teste {teste_id} não encontrados em {DIR_INPUT}. Pulando...")
            continue

        A = np.fromfile(arq_A, dtype=np.float32).reshape((N, N))
        B = np.fromfile(arq_B, dtype=np.float32).reshape((N, N))

        C_stm = processar_matriz_remota(porta, A, B)

        # Gabarito de referência no NumPy
        C_py = np.matmul(A, B, dtype=np.float32)

        # Métricas estatísticas de erro
        diff = np.abs(C_stm - C_py)
        l_inf = float(np.max(diff))
        val_max = float(np.max(np.abs(C_py)))
        ulp_ref = float(np.spacing(np.float32(val_max)))
        desvio_ulps = float(l_inf / ulp_ref)

        ok = (l_inf <= ATOL_TOLERANCIA)
        if ok:
            aprovados += 1
            status_txt = "APROVADO"
        else:
            status_txt = "FALHA"

        resultados_auditoria.append({
            "teste_id": teste_id,
            "l_inf": l_inf,
            "desvio_ulps": round(desvio_ulps, 2),
            "status": status_txt
        })

        print(f"  -> Teste {teste_id:02d}/{total_testes:02d} | Erro Máx: {l_inf:.6e} | ULPs: {desvio_ulps:4.2f} | [{status_txt}]")

    # Salva o relatório CSV (sobrescreve ensaios anteriores)
    csv_path = DIR_AUDIT / "auditoria_serial_30_testes.csv"
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["teste_id", "l_inf", "desvio_ulps", "status"])
        writer.writeheader()
        writer.writerows(resultados_auditoria)

    print("\n" + "=" * 80)
    print(f"RELATÓRIO CONSOLIDADO: {aprovados}/{total_testes} TESTES APROVADOS ({aprovados / total_testes * 100:.1f}%)")
    print(f"Relatório CSV salvo em:  {csv_path}")
    print("=" * 80)


if __name__ == "__main__":
    main()
