# SMK-P4 0.5 — Diagnóstico de Causa-Raiz: Cliques e Retrigger no AMY

## 1. Visão Geral do Fenômeno

Durante a avaliação física da versão Integration 0.4 (assim como na versão 0.3A anterior à interface gráfica), foram identificados pequenos cliques/transientes audíveis durante repetições rápidas de notas e retriggers, com maior proeminência no patch DX7 ROM 135 e presença mensurável no patch DX7 ROM 128.

A investigação auditou isoladamente as 4 camadas da cadeia de reprodução:
1. Entrada USB-MIDI (timestamps, parsing, cabo virtual, duplicações de pacotes);
2. Gerenciamento de vozes do runtime P4 (`amy_runtime.cpp`);
3. Ciclo de vida de osciladores e envelopes no AMY (`src/algorithms.c`, `src/amy.c`, `src/envelope.c`, `src/oscillators.c`);
4. Conversão PCM e renderização de saída.

---

## 2. Auditoria e Isolamento por Camadas

### Camada A — Entrada MIDI
- **Verificação:** Inspeção do parser USB MIDI Host em `smk-p4/components/amy_runtime/usb_input.h` e roteamento de pacotes do M-VAVE SMK25 V2.
- **Evidências:** O SMK25 transmite em cabos virtuais 0 e 1. O firmware seleciona estritamente o cabo 0 e descarta pacotes auxiliares com contadores de diagnóstico intactos. Mensagens de NoteOn repetidas enquanto a nota já está sustentada ou com a mesma tecla pressionada são tratadas de forma idempotente (`voices_[found].held`).
- **Conclusão:** Não há geração de eventos espúrios, jitter desordenado ou duplicação de gates pelo estágio MIDI.

### Camada B — Conversão PCM e I²S
- **Verificação:** Inspeção de `components/audio_pcm5102/pcm5102_output.cpp` e buffers DMA.
- **Evidências:** A descontinuidade abrupta já estava 100% presente no stream PCM sintetizado no host antes de qualquer driver I²S ou DAC físico. Em `build/p404/retrigger_probe/baseline_patch_135.wav` gerado em ambiente offline determinístico, o salto entre amostras consecutivas na fronteira de NoteOn atingia **1087 unidades PCM**.
- **Conclusão:** A causa não provém de DMA, clipping do DAC PCM5102A ou problemas de clock analógico. O transiente é inteiramente gerado no motor DSP.

### Camada C — Gerenciamento de Vozes (`amy_runtime.cpp`)
- **Verificação:** Análise de alocação de slots lógicos (`voices_[kVoices]`) em resposta a NoteOn/NoteOff.
- **Evidências:** Quando uma nota é liberada (`NoteOff`), `release(slot)` envia `note(slot, 0)` ao AMY para iniciar a cauda de release. No runtime 0.4, o slot lógico era marcado como não utilizado (`used = false`). Em repetições consecutivas da mesma nota ou linhas rápidas monofônicas, o slot 0 era re-selecionado para o novo NoteOn enquanto os osciladores de FM do AMY ainda estavam vibrando na fase de release.
- **Conclusão:** A reutilização de um instrumento cujo envelope ainda está soando é uma condição necessária para o defeito, mas o gatilho da descontinuidade matemática reside no tratamento de fase do oscilador pelo AMY.

### Camada D — Ciclo de Vida do Oscilador no AMY Core (`src/algorithms.c` e `src/amy.c`)
- **Verificação:** Auditoria do código de disparo de notas e reset de fase em `src/algorithms.c:146` (`note_on_mod`) e `src/amy.c:1721` (`play_delta`).
- **Mecanismo Causal Comprovado:**
  1. No upstream do AMY, a política declarada em comentário (`src/amy.c:1719`) é:
     ```c
     // We no longer reset the phase here; instead, we reset phase when an oscillator falls silent.
     // But if a trigger_phase is set, use that.
     ```
  2. Patches DX7 (incluindo os patches 128 e 135) possuem parâmetros onde cada operador tem uma fase inicial pré-configurada (`trigger_phase`, comumente `0.25` / 90 graus para preservar o ataque característico de FM a partir do silêncio).
  3. No código original do AMY:
     ```c
     if (AMY_IS_SET(synth[osc]->trigger_phase))
         synth[osc]->phase = F2P(synth[osc]->trigger_phase);
     ```
  4. Quando uma nota era disparada em uma voz já ativa ou em cauda de release com amplitude residual significativa (`msynth[osc]->last_amp > 0`), o AMY **forçava incondicionalmente** a fase de todos os 6 operadores de FM para `0.25`.
  5. Se o oscilador modulador ou portador estivesse passando pelo pico ou pelo vale da onda senoidal (ex.: amplitude 1024), a fase era bruscamente cortada para o instante correspondente a `0.25`, gerando uma queda vertical de tensão de até **1087 unidades em uma única amostra** ($t \to t+1$).

---

## 3. Evidência Numérica de Amostras Adjacentes

A inspeção detalhada de amostras consecutivas no arquivo `baseline_patch_135.wav` gerado pelo probe de retrigger demonstrou a descontinuidade vertical:

```text
Amostras 409852 a 409859 (smk-p4-integration-04 baseline):
[  +492,  +440,  +275,   +63, -1024, -1133,  -980,  -950 ]
                            ^^^^^^^^^^
                    Salto instantâneo: 1087 unidades!
```

Contagem de saltos anômalos no baseline:
- Deltas $> 600$: **15 ocorrências**
- Deltas $> 800$: **7 ocorrências**
- Deltas $> 1000$: **7 ocorrências**

---

## 4. Correção Causal Mínima

A solução atua exatamente na causa-raiz sem violar a intenção musical dos patches DX7:
- Se o oscilador está silencioso (`msynth[osc]->last_amp == 0`), a fase inicial configurada pelo patch (`trigger_phase`) é aplicada normalmente, garantindo que o primeiro ataque de uma nota parta exatamente do ponto de fase planejado pelo timbre original.
- Se o oscilador ainda está soando (`msynth[osc]->last_amp > 0`), o AMY não reseta a fase, preservando a continuidade do ciclo senoidal dos operadores moduladores e portadores e eliminando a descontinuidade brusca.

### Código Modificado no AMY Core

Em `src/algorithms.c`:
```c
// Linha 149:
if (AMY_IS_SET(synth[osc]->trigger_phase) && msynth[osc]->last_amp == 0)
    synth[osc]->phase = F2P(synth[osc]->trigger_phase);
```

Em `src/amy.c`:
```c
// Linhas 1724 e 1742:
if (AMY_IS_SET(synth[osc]->trigger_phase) && msynth[osc]->last_amp == 0)
    synth[osc]->phase = F2P(synth[osc]->trigger_phase);
```

### Resultado Após a Correção

```text
Amostras 406786 a 406793 (smk-p4-audio-quality-05 candidato):
[  +960, +1045,  +799,  +204,  -434,  -786,  -835,  -769 ]
                            ^^^^^^^^
                Passagem por zero natural de onda senoidal contínua
```

Contagem de saltos no candidato corrigido:
- Deltas $> 1000$: **0 ocorrências (100% eliminadas)**
- Deltas $> 800$: **0 ocorrências (100% eliminadas)**
- Deltas $> 600$: **4 ocorrências** (todas correspondentes a derivadas naturais de passagem por zero em frequências agudas a plena amplitude).
