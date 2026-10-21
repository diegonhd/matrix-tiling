#!/usr/bin/env python3
"""
Orquestrador Serial Host (Runner) Simplificado para a Tarefa 2.
Transmite lotes de matrizes (A e B, 45x45, float32) para o STM32 NUCLEO-F030R8,
recebe a matriz resultante C calculada por blocos e audita contra o NumPy.
"""

import time
import argparse
import csv
from pathlib import Path
import numpy as np
import serial
import serial.tools.list_ports


# Importa os comandos simplificados
from protocolo_serial import (
    N, BLOCK,
    CMD_PING, CMD_STRIP_A, CMD_BLOCK_B,
    RESP_OK, RESP_ACK
)

ATOL_TOLERANCIA = 1.0e-3
BAUD_RATE_PADRAO = 460800

ROOT_DIR = Path(__file__).resolve().parent.parent
DIR_INPUT = ROOT_DIR / "src_t1" / "input_matrix"
DIR_AUDIT = ROOT_DIR / "src_t2" / "audit"
DIR_AUDIT.mkdir(parents=True, exist_ok=True)


def auto_detectar_porta_stlink():
    """Varre e retorna a porta COM do ST-Link conectado."""
    portas = serial.tools.list_ports.comports() # Lista as portas seriais
    for p in portas: # procura na desc algum sinal do stm32
        desc = (p.description or "").lower() 
        mfg = (p.manufacturer or "").lower()
        if "stlink" in desc or "stlink" in mfg or "stm32" in desc:
            return p.device
    if len(portas) == 1:
        return portas[0].device
    return None


def ler_exato(porta, n_bytes: int, timeout_s: float = 2.0) -> bytes:
    """Lê exatamente n_bytes da porta serial ou estoura timeout."""
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
    Coordena a comunicação simples:
    1. Para cada faixa de A: envia e espera confirmação 'K'.
    2. Para cada bloco de B: envia e recebe os floats de C calculados.
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
        ack = ler_exato(porta, 1) # Primeiro bit do 
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
        porta = serial.Serial(porta_nome, args.baud, timeout=2.0)
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
        diff = np.abs(C_stm - C_py) # Erro
        l_inf = float(np.max(diff)) # Maior erro absoluto
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

    # Salva os relatórios CSV e JSON para o framework de auditoria
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
