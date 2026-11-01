# Multiplicação de Matrizes com Tiling - Tarefa 1 (src_t1)

## 1. Estrutura e Hierarquia de Arquivos

```text
src_t1/
├── matrix_gen.c                # Gerador determinístico em C das matrizes de teste (A e B)
├── matrix_multi.c              # Motor computacional em C com algoritmo de Tiling (B=8)
├── matrix_multi.py             # Motor de referência em Python (NumPy float32)
├── README.md                   # Documentação da hierarquia e funções dos componentes
│
├── framework_testes.ipynb      # Caderno prático de auditoria metrológica e validação
├── fundamentacao_teorica.ipynb # Caderno teórico: padrão IEEE 754 e limites de Higham
│
├── input_matrix/               # Matrizes de entrada em formato binário bruto (float32)
│   ├── A_1.bin ... A_30.bin    # 30 matrizes operando A (8.100 bytes cada, N=45)
│   ├── B_1.bin ... B_30.bin    # 30 matrizes operando B (8.100 bytes cada, N=45)
│   ├── A_identidade.bin        # Teste unitário 1: Matriz aleatória de teste
│   ├── B_identidade.bin        # Teste unitário 1: Matriz Identidade I
│   ├── A_zeros.bin             # Teste unitário 2: Matriz aleatória de teste
│   ├── B_zeros.bin             # Teste unitário 2: Matriz Nula Z
│   ├── A_mforcada.bin          # Teste unitário 3: Matriz com magnitude extrema (2^20)
│   └── B_mforcada.bin          # Teste unitário 3: Matriz com frações pequenas (2^-4)
│
├── bin_c/                      # Matrizes resultantes calculadas pelo executável C (Tiling)
│   ├── C_1.bin ... C_30.bin    # Produtos C = A x B calculados pelo motor em C
│   ├── C_identidade.bin        # Produto unitário: A x I
│   ├── C_zeros.bin             # Produto unitário: A x 0
│   └── C_mforcada.bin          # Produto unitário: Alinhamento forçado de expoentes
│
├── bin_py/                     # Matrizes de referência calculadas pelo Python (NumPy)
│   ├── C_1.bin ... C_30.bin    # Produtos C = A @ B de gabarito aritmético
│   ├── C_identidade.bin        # Gabarito NumPy: A x I
│   ├── C_zeros.bin             # Gabarito NumPy: A x 0
│   └── C_mforcada.bin          # Gabarito NumPy: Alinhamento forçado
│
└── audit/                      # Relatórios da auditoria metrológica independente
    ├── auditoria_testes.csv    # Registro tabular dos testes, sementes, hashes e erros
    └── auditoria_testes.json   # Registro estruturado em JSON para rastreabilidade
```

---

## 2. Função de Cada Arquivo no Projeto

| Arquivo / Diretório | Camada / Papel | Função Principal |
| :--- | :--- | :--- |
| [`matrix_gen.c`]| Gerador de Estímulos (C) | Gera e salva em `input_matrix/` os 30 pares de matrizes aleatórias bipolares $[-10.0, +9.9]$ e os 3 pares unitários (Identidade, Zeros e Alinhamento Forçado de 24 bits) em formato binário bruto IEEE 754 `float32`. |
| [`matrix_multi.c`] | Motor Computacional (C) | Executa o produto matricial $C = A \times B$ utilizando técnica de Tiling em 6 laços aninhados com blocos $8 \times 8$, otimizando a localidade na cache L1/L2 e persistindo os resultados em `bin_c/`. |
| [`matrix_multi.py`]| Motor de Referência (Python) | Processa exatamente os mesmos arquivos de entrada utilizando o operador vetorizado `@` do NumPy em `float32`, salvando os resultados em `bin_py/` como gabarito aritmético (*ground truth*). |
| [`README.md`] | Documentação Técnica | Explica a estrutura de diretórios, a hierarquia dos arquivos e a responsabilidade funcional de cada componente na solução. |
| [`framework_testes.ipynb`] | Auditoria Metrológica | Notebook Jupyter independente que lê os binários de `bin_c/` e `bin_py/`, calcula métricas de conformidade (erro absoluto $L_\infty$ e desvio em ULPs) e valida hashes SHA-256. |
| [`fundamentacao_teorica.ipynb`]| Embasamento Analítico | Notebook dedicado à teoria de precisão de máquina, representação IEEE 754 de 32 bits, limite analítico de erro de Higham e estabilidade numérica. |
| `input_matrix/` | Repositório de Entrada | Armazena os pares de matrizes de entrada $A$ e $B$ com dimensões $45 \times 45$ (2025 elementos / 8.100 bytes cada) gravados de forma bruta (*raw*), desacoplando a geração do processamento. |
| `bin_c/` | Armazenamento de Saída (C) | Contém as matrizes $C$ geradas pelo motor em C com Tiling, mantendo o mesmo formato binário sem cabeçalhos para auditoria metrológica cega. |
| `bin_py/` | Armazenamento de Gabarito (Py) | Contém as matrizes $C$ calculadas pelo NumPy, servindo de base de comparação direta bit-a-bit e em ponto flutuante. |
| `audit/` | Registro de Conformidade | Guarda os arquivos `auditoria_testes.csv` e `auditoria_testes.json` contendo o histórico auditável de aprovação de 100% dos testes. |

---

## 3. Motivação da Arquitetura e Desacoplamento dos Arquivos

1. **Desacoplamento Estrito entre Módulos**:
   * O gerador (`matrix_gen.c`), o motor C (`matrix_multi.c`) e o motor Python (`matrix_multi.py`) comunicam-se exclusivamente através do sistema de arquivos em formato binário bruto (`float32` *raw* de 8.100 bytes).
   * Esse isolamento impede qualquer contaminação de memória compartilhada entre a implementação em C e o gabarito em Python.

2. **Auditoria Metrológica Cega e Independente**:
   * O framework de validação não tem acesso aos processos em execução; consome diretamente os arquivos gerados em disco, calculando erros em ULPs e hashes criptográficos SHA-256 para garantir integridade e reproducibilidade científica.
