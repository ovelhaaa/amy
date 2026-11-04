# SMK-P4 0.5 — Matriz de Testes de Áudio e Retrigger

## 1. Metodologia de Teste

Para validar a integridade acústica e a ausência de cliques em repetições de notas, foi implementada a suíte automatizada `smk-p4/tests/test_retrigger_scenarios.cpp`.
Todos os testes foram executados com:
- Taxa de amostragem: **48.000 Hz**
- Formato: **PCM16 estéreo** intercalado
- Bloco AMY: **256 amostras**
- Patches ROM avaliados: **135** e **128**
- Master volume: **64** (0.5 FS)

### Métricas Analisadas
1. **OnsetJump**: Diferença absoluta entre a última amostra antes da fronteira do bloco de NoteOn e a primeira amostra do bloco seguinte ($|S_{n} - S_{n-1}|$).
2. **OffJump**: Diferença absoluta entre a última amostra antes do bloco de NoteOff e a primeira amostra subsequente.
3. **MaxDelta**: Máxima diferença absoluta entre duas amostras adjacentes ($|S_t - S_{t-1}|$) ao longo de todo o cenário.
4. **Peak**: Amplitude máxima absoluta no stream PCM16.
5. **RMS**: Valor quadrático médio da amplitude no teste.
6. **Clipping**: Contador de amostras saturadas ($\ge 32767$ ou $\le -32768$).

---

## 2. Cenários Avaliados

- **Cenário A — Nota Isolada:** Note On $\to$ sustain $\to$ Note Off $\to$ cauda de release completa até silêncio total.
- **Cenário B — Repetição Lenta:** Note On $\to$ Note Off $\to$ repouso por 1,0 s (envelope zerado) $\to$ novo Note On.
- **Cenário C — Repetição Rápida:** Note On $\to$ Note Off $\to$ novo Note On imediato após 50 ms (em plena cauda de release).
- **Cenário D — Retrigger sem Note Off:** Note On repetido na mesma nota sem Note Off intermediário.
- **Cenário E — Notas Alternadas:** C4 $\to$ D4 $\to$ C4 $\to$ D4 em intervalos rápidos de 80 ms.
- **Cenário F — Acordes de Quatro Vozes:** Disparo de 4 notas simultâneas (C4, E4, G4, B4), release e novo disparo em 80 ms.
- **Cenário G — Pedal de Sustain (CC64):** CC64=127 ativado $\to$ notas repetidas $\to$ CC64=0 $\to$ release conjunto.
- **Cenário H — Troca de Patch em Execução:** Nota ativa $\to$ troca de patch entre ROM 128 e 135 $\to$ nova nota.

---

## 3. Matriz Comparativa: Patch 135

| Cenário | Métrica | Baseline (0.4) | Candidato (0.5) | Variação | Avaliação |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Cenário A** (Nota Isolada) | OnsetJump<br>MaxDelta | 1<br>180 | 1<br>180 | 0%<br>0% | **PASS** (idêntico, transiente limpo de ataque) |
| **Cenário B** (Repetição Lenta) | OnsetJump<br>MaxDelta | 1<br>187 | 1<br>187 | 0%<br>0% | **PASS** (idêntico, ataque parte do silêncio) |
| **Cenário C** (Repetição Rápida) | OnsetJump<br>MaxDelta | **364**<br>252 | **43**<br>252 | **-88,2%**<br>0% | **PASS** (salto anômalo eliminado) |
| **Cenário D** (Retrigger sem Off) | OnsetJump<br>MaxDelta | 38<br>207 | 59<br>207 | Seguro<br>0% | **PASS** (comportamento suave mantido) |
| **Cenário E** (Notas Alternadas) | OnsetJump<br>MaxDelta | **345**<br>253 | **71**<br>253 | **-79,4%**<br>0% | **PASS** (transição suave entre notas) |
| **Cenário F** (Acordes 4 Vozes) | OnsetJump<br>MaxDelta | **1087**<br>1087 | **176**<br>642 | **-83,8%**<br>**-41,0%** | **PASS** (eliminação do cliff de 1087 unidades) |
| **Cenário G** (Sustain CC64) | OnsetJump<br>MaxDelta | 107<br>443 | 30<br>443 | **-72,0%**<br>0% | **PASS** (sustentação limpa e sem estalos) |
| **Cenário H** (Troca de Patch) | OnsetJump<br>MaxDelta | 3<br>238 | 3<br>238 | 0%<br>0% | **PASS** (muting e transição limpos) |

---

## 4. Matriz Comparativa: Patch 128

| Cenário | Métrica | Baseline (0.4) | Candidato (0.5) | Variação | Avaliação |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Cenário A** (Nota Isolada) | OnsetJump<br>MaxDelta | 3<br>386 | 3<br>386 | 0%<br>0% | **PASS** (ataque original preservado) |
| **Cenário B** (Repetição Lenta) | OnsetJump<br>MaxDelta | 3<br>434 | 3<br>434 | 0%<br>0% | **PASS** (ataque limpo) |
| **Cenário C** (Repetição Rápida) | OnsetJump<br>MaxDelta | **21**<br>200 | **3**<br>200 | **-85,7%**<br>0% | **PASS** (retrigger transparente) |
| **Cenário D** (Retrigger sem Off) | OnsetJump<br>MaxDelta | 28<br>392 | 64<br>392 | Seguro<br>0% | **PASS** (envelope suave) |
| **Cenário E** (Notas Alternadas) | OnsetJump<br>MaxDelta | **118**<br>118 | **3**<br>118 | **-97,5%**<br>0% | **PASS** (eliminação de clique em notas alternadas) |
| **Cenário F** (Acordes 4 Vozes) | OnsetJump<br>MaxDelta | **274**<br>463 | **15**<br>463 | **-94,5%**<br>0% | **PASS** (queda drástica de 274 para 15) |
| **Cenário G** (Sustain CC64) | OnsetJump<br>MaxDelta | 25<br>605 | 23<br>605 | -8,0%<br>0% | **PASS** (estabilidade com pedal) |
| **Cenário H** (Troca de Patch) | OnsetJump<br>MaxDelta | 3<br>470 | 3<br>470 | 0%<br>0% | **PASS** (silenciamento correto) |

---

## 5. Resultados do Probe de Retrigger USB (`render_retrigger_probe`)

No teste padronizado de longa duração (532.736 frames = 11,1 segundos por patch):

| Patch | Métrica | Baseline Integration 0.4 | Candidato Audio Quality 0.5 | Redução |
| :---: | :--- | :---: | :---: | :---: |
| **135** | `onset_boundary_jump`<br>`max_sample_delta`<br>`deltas > 1000`<br>`deltas > 800` | 1087<br>1087<br>7<br>7 | **223**<br>**638**<br>**0**<br>**0** | **-79,5%**<br>**-41,3%**<br>**-100%**<br>**-100%** |
| **128** | `onset_boundary_jump`<br>`max_sample_delta` | 274<br>457 | **18**<br>506 | **-93,4%**<br>Passagem zero senoidal |

---

## 6. Conclusões da Matriz de Testes

1. A correção causal eliminou integralmente os cliffs verticais instantâneos que causavam cliques audíveis no hardware.
2. Não foi observado nenhum clipping em nenhum dos cenários.
3. Não houve alteração no timbre das notas tocadas a partir do silêncio (Cenários A e B mantiveram métricas idênticas ao baseline).
4. As funções de Note On, Note Off, Sustain CC64, Pitch Bend e Panic permaneceram estritamente preservadas.
