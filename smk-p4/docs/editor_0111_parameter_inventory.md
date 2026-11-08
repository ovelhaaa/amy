# SMK-P4 0.11.1 — Inventário Integral de Parâmetros (Gate G0)

**Branch:** `feature/smk-p4-sound-editor-completion-0111`  
**Escopo:** Auditoria integral de todos os 88 parâmetros registrados em `ParameterRegistry` (`smk-p4/components/ui_blueprint/src/ui_parameters.cpp`) e dos 57 identificadores persistentes do motor (`edit::Param` `0..56` em `smk-p4/components/amy_runtime/sound_edit.h`).

> **Garantia automatizada (Gate G0):** A função `auditInventory()` em `smk-p4/tests/sound_editor_ui_test.cpp` percorre programaticamente `ParameterRegistry::all()` tanto em patch Juno (`patch 4`) quanto em patch DX7 (`patch 135`) e falha o teste caso qualquer parâmetro reportado como `Available` pelo adaptador de produção (`UiBlueprintPerformanceAdapter`) não possua handler real confirmado pelo owner (`UiCommandState::Confirmed`) ou retorne `Unsupported`/`DemoApplied`.

---

## 1. Parâmetros do Sound Editor — Persistentes no Motor (`edit::Param` `0..56`)

| ID UI (`ParameterDescriptor`) | `edit::Param` ID | Aba UI | Família / Condição | Estado 0.11.1 | Destino Nativo AMY | Persistência | Evidência / Teste |
|---|---:|---|---|---|---|---|---|
| `osc.1.level` | `21` (`OscLevel`) | `OSC` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `amp_coefs[COEF_CONST]` escalado sobre o mix ROM dos osciladores principais identificados | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `osc.sub_level` | `22` (`SubLevel`) | `OSC` | Juno com sub-oscilador materializado | `Conditional` (`Available` quando presente, `N/A` caso contrário) | `amp_coefs[COEF_CONST]` do sub-oscilador (`-1 oct` pulse) | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `osc.noise_level` | `23` (`NoiseLevel`) | `OSC` | Juno com gerador de ruído materializado | `Conditional` (`Available` quando presente, `N/A` caso contrário) | `amp_coefs[COEF_CONST]` do oscilador `NOISE` | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `osc.1.pulse_width` | `24` (`PulseWidth`) | `OSC` | Juno com oscilador `PULSE` audível sem PWM LFO/ENV dominante (ex.: `patch 31`) | `Conditional` (`Available` em `patch 31`, `N/A` em `patch 4` e FM) | `duty_coefs[COEF_CONST]` (`0.05..0.95`) | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `osc.1.detune_ct` | `25` (`Detune`) | `OSC` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | Offset em `logfreq_coefs[COEF_CONST]` (`±100 ct`) nos osciladores principais, preservando Sub/LFO | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.depth` | `16` (`Depth`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | Escalamento relativo de `amp_coefs[COEF_CONST]` apenas nos operadores moduladores; carriers preservados | `V1 + V2` | `sound_editor_test.cpp`, `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.feedback` | `29` (`FmFeedback`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | `feedback` (`0.0..0.00125f`) no oscilador raiz `ALGO` e no operador com auto-feedback nas 8 vozes | `V2` | `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.op.level` (+ `fm.selected_op` 1..6) | `4..9` (`OpLevel0..5`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | `amp_coefs[COEF_CONST]` (`0..100%` do ganho ROM capturado) do operador `algo_source[op]` nas 8 vozes | `V1 + V2` | `sound_editor_test.cpp`, `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.op.ratio` (+ `fm.selected_op` 1..6) | `10..15` (`OpRatio0..5`) | `FM` | DX7 FM (`family == 0`, operador em modo ratio `logfreq_coefs[COEF_NOTE] != 0`) | `Conditional` (`Available` em ops ratio, `N/A` em ops fixed-freq como `patch 153` OP1 e em Juno) | `logfreq_coefs[COEF_CONST]` (`std::log2(ratio)`) do operador `algo_source[op]` nas 8 vozes | `V1 + V2` | `sound_editor_test.cpp`, `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.op.attack` (+ `fm.selected_op` 1..6) | `30..35` (`OpAttack0..5`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | `eg0_times[0]` (`ms_to_samples_int(0..2000 ms)`) do operador `algo_source[op]`, preservando segmentos 1..5 e níveis | `V2` | `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.op.decay` (+ `fm.selected_op` 1..6) | `36..41` (`OpDecay0..5`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | `eg0_times[1]` (`ms_to_samples_int(0..4000 ms)`) do operador `algo_source[op]`, preservando segmentos 0 e 2..5 | `V2` | `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `fm.op.vel_sens` (+ `fm.selected_op` 1..6) | `42..47` (`OpVel0..5`) | `FM` | DX7 FM (`family == 0`) | `Available` (FM) / `Unavailable` (Juno) | `amp_coefs[COEF_VEL]` (`0..100%` → `0.0..2.0f`) do operador `algo_source[op]` nas 8 vozes | `V2` | `gate_d_test.cpp`, `sound_editor_ui_test.cpp` |
| `filter.cutoff_hz` | `0` (`Cutoff`) | `FILTER` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `filter_freq_coefs[COEF_CONST]` (`20..18000 Hz`) nos osciladores filtrados da voz | `V1 + V2` | `sound_editor_test.cpp`, `juno_filter_test.cpp`, `sound_editor_ui_test.cpp` |
| `filter.resonance` | `1` (`Resonance`) | `FILTER` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `resonance` (`0.1..2.0 Q`) nos osciladores filtrados da voz | `V1 + V2` | `sound_editor_test.cpp`, `juno_filter_test.cpp`, `sound_editor_ui_test.cpp` |
| `filter.env_amount` | `19` (`FilterEnv`) | `FILTER` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `filter_freq_coefs[COEF_EG0/COEF_EG1]` (`-11..+11 oct`) nos osciladores filtrados da voz | `V2` | `juno_filter_test.cpp`, `sound_editor_ui_test.cpp` |
| `filter.keytrack` | `20` (`FilterKeytrack`) | `FILTER` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `filter_freq_coefs[COEF_NOTE]` (`0..200%` → `0.0..2.0f`) nos osciladores filtrados da voz | `V2` | `juno_filter_test.cpp`, `sound_editor_ui_test.cpp` |
| `amp.level` | `26` (`PatchLevel`) | `AMP` | Juno & DX7 FM | `Available` (Juno e FM) | Escalamento de ganho por instrumento (`0..100%` sobre ganho ROM de saída), independente de `perf.master_volume` | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `amp.pan` | `27` (`Pan`) | `AMP` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `pan_coefs[COEF_CONST]` (`-64..+63` → `0.0..1.0f`) no oscilador de saída da voz | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `amp.vel_sens` | `28` (`Velocity`) | `AMP` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM; usa `fm.op.vel_sens`) | `amp_coefs[COEF_VEL]` com compensação de teto em `COEF_CONST` (`v=127` invariante) | `V2` | `juno_components_test.cpp`, `sound_editor_ui_test.cpp` |
| `env.amp.attack_ms` | `2` (`Attack`) | `ENV` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `eg0_times[0]` (`1..8000 ms`) em todos os osciladores com `eg0` ativo na voz | `V1 + V2` | `sound_editor_test.cpp`, `gate_a_test.cpp`, `sound_editor_ui_test.cpp` |
| `env.amp.decay_ms` | `17` (`Decay`) | `ENV` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `eg0_times[1]` (`1..30000 ms`) em todos os osciladores com `eg0` ativo na voz | `V2` | `gate_a_test.cpp`, `sound_editor_ui_test.cpp` |
| `env.amp.sustain` | `18` (`Sustain`) | `ENV` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `eg0_values[1]` (`0..100%` → `0.0..1.0f`) em todos os osciladores com `eg0` ativo na voz | `V2` | `gate_a_test.cpp`, `sound_editor_ui_test.cpp` |
| `env.amp.release_ms` | `3` (`Release`) | `ENV` | Juno (`family == 1`) | `Available` (Juno) / `Unavailable` (FM) | `eg0_times[2]` (`1..8000 ms`) em todos os osciladores com `eg0` ativo na voz | `V1 + V2` | `sound_editor_test.cpp`, `gate_a_test.cpp`, `sound_editor_ui_test.cpp` |
| `lfo.1.rate_hz` | `48` (`LfoRate`) | `LFO` / `MOD` | Juno & DX7 FM | `Available` (Juno e FM) | `logfreq_coefs[COEF_CONST]` (`logfreq_of_freq(0.1..20 Hz)`) no oscilador `mod_source` da voz | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `lfo.1.depth` | `49` (`LfoDepth`) | `LFO` / `MOD` | Juno & DX7 FM | `Available` (Juno e FM) | Profundidade base (`0..100%`) aplicada ao destino selecionado de `lfo.1.dest`, composta com CC1/Slot 1 | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `lfo.1.wave` | `50` (`LfoWave`) | `LFO` / `MOD` | Juno & DX7 FM | `Available` (Juno e FM) | `wave` (`SINE, TRIANGLE, SAW_DOWN, SAW_UP, PULSE`) no oscilador `mod_source` da voz | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `lfo.1.dest` | `51` (`LfoDest`) | `LFO` / `MOD` | Juno (`NONE, PITCH, CUTOFF, AMP`) / FM (`NONE, PITCH, AMP`; `CUTOFF` = N/A) | `Available` (opções filtradas contextualmente por `isParameterValueSupported`) | `logfreq_coefs[COEF_MOD]`, `filter_freq_coefs[COEF_MOD]` (Juno) ou `amp_coefs[COEF_MOD]` | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `mod.slot1.enable` | `52` (`ModEnable`) | `MOD` | Juno & DX7 FM | `Available` (Juno e FM) | Habilita/desabilita a rota do Slot 1 (`0` ou `1`) no cálculo owner de CC1 | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `mod.slot1.source` | `53` (`ModSource`) | `MOD` | Juno & DX7 FM (`OFF=0`, `MOD WHEEL=1`) | `Available` (opções `2..9` bloqueadas por `isParameterValueSupported`) | Seleciona `CC1 Mod Wheel` (`mod_wheel_`) como fonte da rota Slot 1 | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `mod.slot1.dest` | `54` (`ModDest`) | `MOD` | Juno (`NONE=0, OSC PITCH=1, FILTER CUT=3`) / FM (`NONE=0, OSC PITCH=1`) | `Available` (`FILTER CUT` bloqueado em FM por `isParameterValueSupported`) | Rota `CC1 -> Vibrato Pitch` (`logfreq_coefs[COEF_MOD]`) ou `CC1 -> Filter Cutoff` (`filter_freq_coefs[COEF_CONST]`) | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `mod.slot1.amount` | `55` (`ModAmount`) | `MOD` | Juno & DX7 FM (`-100..+100%`) | `Available` (Juno e FM) | Intensidade percentual da rota Slot 1 sobre `CC1` | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |
| `mod.slot1.polarity` | `56` (`ModPolarity`) | `MOD` | Juno & DX7 FM (`UNI+=0, BIP+/-=1, INV-=2`) | `Available` (Juno e FM) | Lei de polaridade aplicada a `ModAmount` no cálculo de `CC1` | `V2` | `gate_e_test.cpp`, `sound_editor_ui_test.cpp` |

---

## 2. Parâmetros do Sound Editor e Performance — Sessão / Macros / Planejados

| ID UI (`ParameterDescriptor`) | Aba / Tela | Estado 0.11.1 | Comportamento no Adaptador Real (`UiBlueprintPerformanceAdapter`) | Persistência |
|---|---|---|---|---|
| `fm.selected_op` | `SoundEditor / FM` | `Available` (FM) / `Unavailable` (Juno) | Seleciona o operador ativo (`OP1..OP6`) na ordem materializada `algo_source` e sincroniza `fm.op.level`, `fm.op.ratio`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens` | Sessão UI |
| `perf.macro.2` (`M2 BRIGHT`) | `Performance` | `Available` | Coalescido no owner (`macros[0]`): compõe multiplicativamente com `filter.cutoff_hz` (Juno) ou `fm.depth` (FM) | Sessão / Composto |
| `perf.macro.5` (`M5 ATTACK`) | `Performance` | `Available` (Juno) / `Unavailable` (FM) | Coalescido no owner (`macros[1]`): compõe multiplicativamente com `env.amp.attack_ms` (Juno) | Sessão / Composto |
| `perf.macro.6` (`M6 RELEASE`) | `Performance` | `Available` (Juno) / `Unavailable` (FM) | Coalescido no owner (`macros[2]`): compõe multiplicativamente com `env.amp.release_ms` (Juno) | Sessão / Composto |
| `perf.patch_id` | `Performance / Browser` | `Available` | Troca de patch confirmada por snapshot nos 229 patches admitidos (guardada por `dirty` quando há edição não salva) | Origem salva em `V1/V2` |
| `perf.master_volume` | `Performance / AMP` | `Available` | Ganho master global (`0..127`) com suavização por frame no owner, independente de `amp.level` | Sessão |
| `perf.octave_shift` | `Performance` | `Available` | Transposição de oitava (`-2..+2`) do teclado touch com liberação segura de notas ativas | Sessão UI |
| `osc.1.wave`, `osc.1.pitch_st`, `osc.1.pan`, `osc.1.unison` | `SoundEditor / OSC` | `Planned` | Desabilitados na UI real (`PLAN`); `requestParameter*` retorna `UiCommandState::Unsupported` | Não |
| `fm.algo` | `SoundEditor / FM` | `Planned` | Operação estrutural (exige reconstrução de `algo_source` e envelopes); exibido como `PLAN` e desabilitado | Não |
| `filter.type`, `filter.drive` | `SoundEditor / FILTER` | `Planned` | Exibidos como `PLAN` e desabilitados; `requestParameter*` retorna `UiCommandState::Unsupported` | Não |
| `amp.portamento_ms` | `SoundEditor / AMP` | `Planned` | Exibido como `PLAN` e desabilitado; `requestParameter*` retorna `UiCommandState::Unsupported` | Não |
| `env.amp.curve` | `SoundEditor / ENV` | `Planned` | Exibido como `PLAN` e desabilitado; `requestParameter*` retorna `UiCommandState::Unsupported` | Não |
| `lfo.1.phase_deg`, `lfo.1.sync` | `SoundEditor / LFO` | `Planned` | Exibidos como `PLAN` e desabilitados; `requestParameter*` retorna `UiCommandState::Unsupported` | Não |
| `perf.macro.1`, `perf.macro.3`, `perf.macro.4`, `perf.macro.7`, `perf.macro.8` | `Performance` | `Planned` | Desabilitados na UI real; `requestParameter*` retorna `UiCommandState::Unsupported` | Não |

---

## 3. Demais Subsistema Registrados (`Harmony`, `Layers`, `Mixer`, `FX`, `Arp`, `Seq`, `Drums`, `MIDI`, `System`)

| Grupo / IDs | Estado no Adaptador Real 0.11.1 | Notas de Implementação |
|---|---|---|
| **Harmony Assist** (`harmony.scale_enable`, `harmony.chord_enable`, `harmony.apply_touch`, `harmony.root_note`, `harmony.scale_type`, `harmony.quant_mode`, `harmony.chord_type`, `harmony.inversion`, `harmony.voicing`, `harmony.spread`, `harmony.usb_channel`, `harmony.preset`) | `Available` (12 parâmetros) | Confirmados por snapshot do owner (`harmonyState()`); opções planejadas (`ADD9`, `9TH`, voicings `>1`) filtradas por `isParameterValueSupported`. `harmony.preserve_outside`, `harmony.vel_behavior`, `harmony.dest_layer` permanecem `Planned`. |
| **System Read-Only** (`sys.sample_rate_hz`) | `ReadOnly` (`48000 Hz`) | Protegido por `kCapReadOnly`; escrita rejeitada tanto em `UiModel` quanto no adaptador. |
| **Layers / Mixer / FX / Arp / Seq / Drums / MIDI Config / System Settings** (demais IDs de `ParameterRegistry`) | `Planned` no `UiBlueprintPerformanceAdapter` | Exibidos com badge `PLAN` e controles desabilitados quando conectados ao backend real (`isParameterEditable(id) == false`), garantindo zero falsos positivos na interface. |
