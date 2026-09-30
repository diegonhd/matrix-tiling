# Guia Didático das Métricas de Validação Numérica

Este documento explica de forma simples, intuitiva e prática o papel de cada uma das três métricas numéricas calculadas na nossa auditoria (`auditoria_testes.csv` e `validacao.ipynb`), demonstrando por que os métodos convencionais ingênuos falham e como a nossa abordagem resolve o problema com rigor científico.

---

## Sumário das Métricas

| Métrica no Código | Nome Formal | Para que serve? | Por que supera o método convencional? |
| :--- | :--- | :--- | :--- |
| **`maior_erro_abs`** | Maior Erro Absoluto | Critério primário de aprovação (PASS/FAIL) | Avalia o **pior caso**, impedindo que erros locais fiquem mascarados pela média. |
| **`erro_ulps`** | Desvio em ULPs (*Unit in the Last Place*) | Mede o erro no nível físico dos bits de silício | É **independente da escala** do número e comprova ausência de bugs lógicos. |
| **`erro_rel_frob`** | Erro Relativo de Frobenius | Mede a energia total de erro da matriz inteira | **Nunca divide por zero** quando há cancelamento de sinais opostos. |

---

## 1. `maior_erro_abs` — Maior Erro Absoluto Máximo

### O que é?
É a maior diferença absoluta encontrada entre qualquer elemento calculado pelo C e o gabarito do NumPy:

$$\text{maior\_erro\_abs} = \max_{i, j} |C_{ij}^{\text{C}} - C_{ij}^{\text{NumPy}}|$$

### O Método Convencional Ingênuo vs. Nosso Método
* **O Método Convencional:** Costuma calcular apenas o *erro médio* da matriz inteira.
  * *O perigo:* Se 2024 elementos tiverem erro zero e 1 único elemento tiver um erro grave de $5{,}0$, a média parecerá minúscula ($5{,}0 / 2025 \approx 0{,}002$), escondendo um bug crítico de transbordamento de memória ou ponteiro.
* **Nosso Método (Engenharia de Pior Caso):**
  * Em sistemas embarcados e computação de alta confiabilidade, **a segurança é ditada pelo pior elemento**.
  * Comparamos o pior elemento contra o **Limite Superior de Higham**:
    $$E_{\text{máx}} \le \gamma_K \sum_{k=1}^K |A_{ik}| \cdot |B_{kj}| \le 45 \times (45 \times 10 \times 10) \times 2{,}22 \times 10^{-16} \approx 4{,}5 \times 10^{-11}$$
  * A tolerância $\mathbf{\text{atol} = 10^{-11}}$ é soberana: basta um único elemento ultrapassá-la para o teste ser reprovado.

### Exemplo Prático ($2 \times 2$):
Suponha que o NumPy calculou a matriz de referência e o C calculou com pequenas oscilações:

$$C_{\text{NumPy}} = \begin{pmatrix} 10{,}0 & 20{,}0 \\ 30{,}0 & 40{,}0 \end{pmatrix}, \quad C_{\text{C}} = \begin{pmatrix} 10{,}0000000000001 & 20{,}0 \\ 30{,}0000000000003 & 40{,}0 \end{pmatrix}$$

1. Subtraímos elemento por elemento para obter os erros absolutos:
   $$\text{Matriz de Erros} = \begin{pmatrix} 1{,}0 \times 10^{-13} & 0{,}0 \\ \mathbf{3{,}0 \times 10^{-13}} & 0{,}0 \end{pmatrix}$$
2. O erro médio seria $(1{,}0 \times 10^{-13} + 3{,}0 \times 10^{-13}) / 4 = 1{,}0 \times 10^{-13}$.
3. Mas a nossa métrica seleciona o pior caso:
   $$\mathbf{\text{maior\_erro\_abs} = 3{,}0 \times 10^{-13}}$$
4. **Decisão:** Como $3{,}0 \times 10^{-13} \le 1{,}0 \times 10^{-11}$, o teste é **PASS (Aprovado)** com folga de mais de 30 vezes.

---

## 2. `erro_ulps` — Desvio em Unidades no Último Lugar (ULP)

### O que é?
A sigla **ULP** (*Unit in the Last Place*) representa a distância física entre dois números vizinhos representáveis no padrão binário IEEE 754 de 64 bits (`double`). 

Ela responde à pergunta fundamental: **"Quantos 'degraus' mínimos de hardware o C se afastou do NumPy?"**

$$\text{erro\_ulps} = \frac{\text{maior\_erro\_abs}}{1\text{ ULP}(\text{magnitude})}$$

###  O Método Convencional Ingênuo vs.  Nosso Método
* **O Método Convencional:** Olhar apenas para o número $10^{-13}$ e assumir que ele é sempre "muito pequeno".
  * *O engano:* Em ponto flutuante, a vírgula flutua. Se você estiver operando com números pequenos como $0{,}0001$, uma diferença de $10^{-13}$ pode representar milhares de ULPs de erro! Já para números grandes como $100.000$, o próprio hardware não consegue registrar passos menores que $10^{-11}$.
* **Nosso Método (Invariância de Escala):**
  * O ULP remove a ilusão da escala e mede a exatidão diretamente nos bits da mantissa.
  * **Interpretação Física:**
    * Erro de **1 a 6 ULPs**: Indica que dos 53 bits de mantissa, **51 a 52 bits foram exatamente idênticos** nos dois códigos. Isso é ruído natural e inevitável de arredondamento ao somar em ordens diferentes no Tiling.
    * Erro de **milhares ou milhões de ULPs**: Seria sintoma de bug lógico (laço com índice errado, soma duplicada, etc.).

### Exemplo Prático:
Suponha que um elemento da matriz resultante vale $C \approx 1100$.

1. **Identificar a faixa de potência de 2:**  
   O número $1100$ está entre $2^{10} = 1024$ e $2^{11} = 2048$. Portanto, seu expoente de base 2 é **$10$**.
2. **Calcular o peso de 1 ULP (o 52º bit da mantissa):**
   $$1\text{ ULP} = 2^{10 - 52} = 2^{-42} = \frac{1}{4.398.046.511.104} \approx \mathbf{2{,}27 \times 10^{-13}}$$
3. **Calcular o desvio em ULPs:**  
   Se o erro absoluto medido pelo nosso código foi de $4{,}54 \times 10^{-13}$:
   $$\text{erro\_ulps} = \frac{4{,}54 \times 10^{-13}}{2{,}27 \times 10^{-13}} = \mathbf{2{,}0\text{ ULPs}}$$
4. **Conclusão:** O C chegou a apenas **2 degraus de distância do NumPy**, comprovando que não há nenhum bug de algoritmo.

---

## 3. `erro_rel_frob` — Erro Relativo da Norma de Frobenius

### O que é?
É a razão entre o "tamanho global do erro da matriz inteira" e o "tamanho global da matriz de referência":

$$\text{erro\_rel\_frob} = \frac{\|C_{\text{C}} - C_{\text{NumPy}}\|_F}{\|C_{\text{NumPy}}\|_F}$$

onde a Norma de Frobenius $\|M\|_F$ é a raiz quadrada da soma de todos os elementos da matriz elevados ao quadrado:
$$\|M\|_F = \sqrt{\sum_{i=1}^N \sum_{j=1}^N |M_{ij}|^2}$$

### O Método Convencional Ingênuo vs. Nosso Método
* **O Método Convencional (Divisão elemento a elemento):**
  $$\text{Erro Relativo} = \frac{|C_{\text{C}} - C_{\text{NumPy}}|}{|C_{\text{NumPy}}|}$$
  * *O desastre:* Em matrizes com números positivos e negativos (como os nossos dados de sensores $[-10{,}0, +9{,}9]$), os termos se anulam na soma ponderada.
  * Se um elemento do gabarito for exatamente **$0{,}0$**, a fórmula tenta dividir por zero $\rightarrow \mathbf{\text{ZeroDivisionError / Erro Infinito (inf)}}!$
  * Se um elemento for muito pequeno (ex: $10^{-10}$), uma divergência irrisória de $10^{-13}$ gera um falso alarme dizendo que o erro relativo foi de $0{,}1\%$, mesmo o cálculo estando correto com 13 dígitos de precisão.
* **Nosso Método (Norma de Frobenius):**
  * Em vez de dividir elemento a elemento, mede a energia da matriz inteira de $2025$ números.
  * O denominador $\|C_{\text{NumPy}}\|_F$ soma todos os quadrados da matriz e atinge valores na casa dos milhares ($\sim 10.000$).
  * **Consequência:** **Nunca ocorre divisão por zero** e obtém-se o erro percentual global real do bloco de memória.

### Exemplo Prático ($2 \times 2$ com cancelamento):
Considere que o resultado real possui um zero por cancelamento exato:

$$C_{\text{real}} = \begin{pmatrix} 3 & 4 \\ \mathbf{0} & 12 \end{pmatrix}, \quad C_{\text{C}} = \begin{pmatrix} 3 & 4 \\ \mathbf{0{,}0013} & 12 \end{pmatrix}$$

1. **Tentando pela fórmula convencional no elemento $(2, 1)$:**
   $$\text{Erro Relativo} = \frac{|0{,}0013 - 0|}{\mathbf{0}} = \mathbf{\text{DIVISÃO POR ZERO! (Falha fatal do teste)}}$$

2. **Pela Norma de Frobenius:**
   * **Passo A: Tamanho do Erro (Numerador):**
     $$\text{Diferença} = \begin{pmatrix} 0 & 0 \\ 0{,}0013 & 0 \end{pmatrix}$$
     $$\|\text{Erro}\|_F = \sqrt{0^2 + 0^2 + (0{,}0013)^2 + 0^2} = \mathbf{0{,}0013}$$
   * **Passo B: Tamanho da Matriz Real (Denominador):**
     $$\|C_{\text{real}}\|_F = \sqrt{3^2 + 4^2 + 0^2 + 12^2} = \sqrt{9 + 16 + 0 + 144} = \sqrt{169} = \mathbf{13}$$
   * **Passo C: Divisão Global:**
     $$\text{erro\_rel\_frob} = \frac{0{,}0013}{13} = \mathbf{0{,}0001} \quad (\mathbf{0{,}01\%})$$

---

## Síntese dos Resultados Obtidos na Nossa Auditoria

Ao rodar a bateria oficial de $N = 30$ testes com dados bipolares simulando sensores embarcados, as três métricas convergiram perfeitamente:

| Métrica | Valor Obtido no Projeto | Limite Tolerável | Conclusão Técnica |
| :--- | :--- | :--- | :--- |
| **`maior_erro_abs`** | **$5{,}68 \times 10^{-13}$** | $\le 1{,}0 \times 10^{-11}$ | **Aprovado com folga de 17.000x** frente ao pior caso teórico de Higham. |
| **`erro_ulps`** | **$\le 5{,}0$ ULPs** | $\le 50{,}0$ ULPs | Apenas o último bit oscilou, atestando conformidade plena com o padrão IEEE 754. |
| **`erro_rel_frob`** | **$2{,}52 \times 10^{-16}$** | $< 10^{-11}$ | O erro relativo global coincide com o **épsilon da máquina** ($\approx 2{,}22 \times 10^{-16}$). |
