# SMK-P4 0.15.2 — Avaliação e Refinamento de Transições de Delay

## 1. Auditoria do DSP de Delay/Echo Upstream AMY

### 1.1 Arquitetura Interna (`src/delay.c`, `src/amy.c`)

O efeito de Echo do AMY opera sobre uma estrutura `delay_line_t` por canal estéreo:
- `samples`: buffer circular contíguo alocado em PSRAM (`enclosing_power_of_2(max_delay_samples)`).
- `fixed_delay`: número inteiro de amostras de atraso (tap de leitura).
- `next_in`: índice circular de escrita atual.
- `len`: tamanho do buffer (potência de 2, permitindo máscara `len - 1`).

O laço de processamento em `delay_line_in_out_fixed_delay()`:
```c
while(n_samples-- > 0) {
    SAMPLE delay_out = DEL_OUT(delay_line, 0);
    SAMPLE next_in = *in++ + SMULR6(feedback_level, delay_out);
    DEL_IN(delay_line, next_in);
    *out++ += MUL8_SS(mix_level, delay_out);
}
```

**Propriedades críticas do DSP upstream:**
1. A amostra lida `delay_out` alimenta **simultaneamente** a saída de áudio (`*out += mix * delay_out`) e a malha de realimentação (`next_in = in + feedback * delay_out`).
2. Uma alteração imediata de `fixed_delay` durante sinal ativo desloca o índice de leitura `(next_in - fixed_delay) & mask`. Se houver descontinuidade de amplitude entre a posição antiga e a nova no buffer circular, surge um degrau de tensão (`click` / transiente).
3. A realimentação não possui limitador não-linear interno além da saturação em ponto fixo (`SAMPLE`), portanto qualquer aumento temporário de ganho efetivo na malha causa saturação/clipping imediato.

---

## 2. Comparação das Três Estratégias (Gate C.2)

### Estratégia A — Wet Ducking Adaptativo (Refinada)
- **Princípio:** Reduz rapidamente o ganho wet (`echo_mix_`), comuta o tap de leitura quando o nível atinge o patamar de segurança e restaura o wet suavemente.
- **Vantagens:**
  - 100% compatível com o core AMY (sem qualquer alteração nos arquivos upstream `src/`).
  - Segurança matemática absoluta contra realimentação descontrolada e clipping ($G_{\text{loop}} \le \text{feedback}$).
  - Custo de DSP nulo: utiliza os parâmetros já existentes de `config_echo()`.
  - Zero alocações de memória ou instabilidade em PSRAM.
- **Limitação mitigada:** A versão 0.15.1 silenciava o wet em degraus fixos mesmo para variações ínfimas de BPM. Na 0.15.2, adicionamos limiar adaptativo: variações menores que 2 ms não sofrem ducking (descontinuidade imperceptível), e variações de divisão musical executam transição rápida e suave de 30 ms.

### Estratégia B — Smooth Retiming (Slew de Leitura)
- **Princípio:** O índice de atraso varia gradualmente bloco a bloco em direção ao novo tempo, similar ao efeito de velocidade variável de eco de fita analógico (tape delay).
- **Avaliação:**
  - Para variações pequenas ($\le 5\text{ ms}$), produz efeito musical agradável.
  - Para saltos de divisão rítmica (ex: $1/8 \to 1/2$, saltando de 250 ms para 1.000 ms = $\Delta 750\text{ ms} = 36.000$ amostras), a taxa de variação necessária para não soar dissonante exigiria vários segundos de transição, descompassando a métrica em apresentações ao vivo.
  - Se a taxa de variação for acelerada, o deslocamento Doppler gera frequências inarmônicas e instabilidade com feedback alto (> 80%).

### Estratégia C — Dual Read Tap Crossfade
- **Princípio:** Leitura simultânea de dois ponteiros (`tap_old` e `tap_new`) no mesmo buffer circular, aplicando crossfade $A \to B$.
- **Avaliação Técnica e Risco:**
  - Exigiria alterar `delay_line_t` e reescrever `delay_line_in_out_fixed_delay()` dentro de `src/delay.c`.
  - **Problema Crítico de Feedback:** Qual tap alimenta a realimentação durante o crossfade?
    - Se ambos alimentarem: o ganho de realimentação efetivo dobra temporariamente, provocando explosão de volume e distorção com feedback $> 50\%$.
    - Se apenas o antigo alimentar: o novo tap não tem recirculação.
    - Se for interpolado: ocorrem cancelamentos de fase (efeito pente) no sinal recirculado.
  - Viola a Regra 5 (`Preserve upstream AMY`) e introduz risco grave em tempo real sem benefício acústico justificável.

### Conclusão e Decisão
Mantém-se a **Estratégia A Refinada**, documentando a decisão conforme estipulado no Gate C.2:
*"Se a estratégia anterior apresentar melhor relação entre custo, segurança e sonoridade, mantê-la e documentar a decisão."*

---

## 3. Refinamento Implementado (Gate C.3)

1. **Deadband Adaptativo:** Diferenças de tempo inferiores a $1{,}5\text{ ms}$ ($\sim 72$ amostras a 48 kHz) são aplicadas diretamente sem ducking, eliminando oscilações espúrias decorrentes de jitter de Tap Tempo ou pequenos ajustes manuais.
2. **Retargeting sem Backlog:** Se uma nova solicitação de tempo ou divisão chegar durante uma transição em andamento, o alvo é atualizado imediatamente (`target_ms`), convergindo para o destino final sem criar fila ou transições pendentes.
3. **Rampa Suave:** O passo de redução de wet respeita a proporção do mix atual, com descida controlada em 3 blocos ($\sim 16\text{ ms}$) e retorno em 4 blocos ($\sim 21\text{ ms}$), totalizando $\le 37\text{ ms}$ de estabilização.
4. **Preservação de Cauda Recirculante:** Como o buffer circular da linha de atraso retém o histórico do sinal, o retorno do wet reintroduz as repetições anteriores do buffer com o novo espaçamento métrico.
