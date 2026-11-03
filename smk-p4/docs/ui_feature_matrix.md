# SMK-P4 UI Blueprint 1.1 — Feature & Parameter Classification Matrix

Per Sections 4, 5, and 9 of the UI Blueprint 1.1 specification, every feature and parameter in `smk-p4/components/ui_blueprint` is classified by actual repository evidence across **four distinct architectural layers**:

1. **Motor AMY (`src/`)**: Capacidade existente no núcleo de síntese AMY upstream.
2. **Runtime P4 Standalone (`smk-p4/components/amy_runtime/` + `smk-p4/main/performance_backend.h`)**: O que está materializado e exposto hoje na API `PerformanceBackend` do firmware P4 0.3.
3. **Controle UI (`smk-p4/components/ui_blueprint/`)**: O que possui widget interativo e validação no `UiModel` / `IUiBackend`.
4. **Integração Fim a Fim (`End-to-End P4 0.3`)**: O que opera de ponta a ponta quando conectado ao `PerformanceBackend` real (`Confirmed` vs `Unsupported/Planned`).

---

## 1. Classification Legend

| Status | Badge | Meaning |
|---|---|---|
| **`AVAILABLE`** | `AVAIL` (`#67C4A5`) | Implementado no runtime P4 standalone (`AmyRuntime` / `PerformanceBackend` / `display_hal`). |
| **`PLANNED`** | `PLAN` (`#E4A44D`) | Presente no motor AMY (`src/`), no `smk-s3`, ou em `specs.md`, aguardando exposição em `PerformanceBackend`. |
| **`UNAVAILABLE`** | `N/A` (`#80919F`) | Inaplicável à família de síntese ou modo ativo (avaliado dinamicamente por `UiModel::effectiveStatus(id)`). |
| **`DEMO`** | `DEMO` (`#EAB65B`) | Simulado exclusivamente em RAM por `UiDemoBackend` (`[SIM DEMO]` meters, slots `USER DEMO`). |

---

## 2. Four-Layer Subsystem Matrix (Engine vs P4 Runtime vs UI vs End-to-End)

| Subsystem / Feature | 1. Motor AMY (`src/`) | 2. Runtime P4 Standalone (`0.3`) | 3. Controle UI (`1.1`) | 4. End-to-End (`PerformanceBackend`) | Repository Evidence |
|---|---|---|---|---|---|
| **ROM Patches `128 DX7 BRASS 1` & `135 DX7 PIANO 1`** | `AVAILABLE` | `AVAILABLE` (`kPatches[]`) | `AVAILABLE` (`PatchSelector` / `Browser`) | **`Confirmed` (`setPatch`)** | `smk-p4/components/amy_runtime/runtime_config.h`, `performance_backend.h` |
| **Catálogo Estendido (`000 JUNO BRASS SET 1` .. `212`)** | `AVAILABLE` (`patches.h`) | `PLANNED` (não materializado em `kPatches[]`) | `AVAILABLE` (`Browser` / `Model`) | **`Unsupported` (filtrado por `isPatchSelectable`)** | `smk-s3/components/synth/src/factory_patches.cpp` |
| **Master Volume (`perf.master_volume`)** | `AVAILABLE` | `AVAILABLE` (`setMaster`) | `AVAILABLE` (`StageMinimal` slider) | **`Confirmed` (`setMaster`)** | `smk-p4/components/amy_runtime/amy_runtime.cpp`, `performance_backend.h` |
| **Teclado Virtual (`Note On` / `Note Off` / `Panic`)** | `AVAILABLE` | `AVAILABLE` (`noteOn`, `noteOff`, `panic`) | `AVAILABLE` (`VirtualKeyboard` + token) | **`Confirmed` (`noteOn` / `noteOff` / `panic`)** | `smk-p4/main/performance_backend.h`, `widgets/virtual_keyboard.cpp` |
| **Octave Shift (`perf.octave_shift`)** | N/A (Transposição de entrada) | Sem `setOctaveShift()` na API | `AVAILABLE` (`OCT -` / `OCT +` in-place) | **`Confirmed` (transposição segura na camada UI/Adapter)** | `widgets/virtual_keyboard.cpp`, `docs/ui_backend_port_contract.md` |
| **Macros 1–8 (`perf.macro.1..8`: `CHAR`, `BRIGHT`, `MOTION`, `SHAPE`, `ATTACK`, `RELEASE`, `SPACE`, `DRIVE`)** | `AVAILABLE` (parâmetros AMY) | `PLANNED` (`setMacro` retorna `false`) | `AVAILABLE` (`MACRO FOCUS` / `KEYS FOCUS`) | **`Unsupported` (reverte slider no modo live até `setMacro` ser ligado)** | `smk-p4/main/local_performance_backend.h`, `smk-s3/components/synth/include/macro_profile.h` |
| **USB-MIDI Expressivo (`Pitch Bend`, `CC1 Mod Wheel`, `CC64 Sustain`, `Vel Sens`)** | `AVAILABLE` | `AVAILABLE` (via USB-MIDI direto no `AmyRuntime`) | `AVAILABLE` (cartões em `MIDI` / `AMP` / `LFO`) | **Ativo via USB-MIDI; setter UI dedicado `PLANNED` em `PerformanceBackend`** | `smk-p4/components/amy_runtime/amy_runtime.cpp`, `usb_input.h` |
| **Osciladores, FM 6-Op, Filtros, ADSR, LFO, Matriz Mod (`osc.*`, `fm.*`, `filter.*`, `env.*`, `lfo.*`, `mod.*`)** | `AVAILABLE` (`src/algorithms.c`, `filters.c`, `envelope.c`) | `PLANNED` (presets ROM fixos em `0.3`) | `AVAILABLE` (`SoundEditor` 7 abas + `Modulation`) | **`Unsupported` (`PLAN`)** | `src/amy.h`, `smk-s3/components/synth/include/patch_types.h` |
| **Layers A/B & Split (`layer_a.*`, `layer_b.*`)** | `AVAILABLE` (múltiplos `synth`) | `AVAILABLE` (Layer A única) / `PLANNED` (Layer B) | `AVAILABLE` (`LayersScreen` + `SplitZones`) | **Layer A ativa; Dual/Split `Unsupported` (`PLAN`)** | `smk-s3/components/audio/include/performance.h` |
| **Mixer & Medidores (`mixer.*`, `SignalMeter`)** | `PLANNED` | `PLANNED` | `DEMO` (`[SIM DEMO]`) | **`Unsupported` (`PLAN` / `SIM DEMO`)** | `widgets/signal_meter.cpp` |
| **Efeitos Master (`Chorus`, `Delay`, `Reverb`, `Drive`, `EQ`)** | `AVAILABLE` (`chorus.c`, `echo.c`, `reverb.c`) | `PLANNED` (desativados no orçamento `0.3`) | `AVAILABLE` (`EffectsScreen`) | **`Unsupported` (`PLAN`)** | `smk-p4/components/amy_runtime/amy_runtime.cpp` |
| **Arpeggiator, Sequencer & Drums (`arp.*`, `seq.*`, `drums.*`)** | `AVAILABLE` (`pcm_tiny.h` para Drums) | `PLANNED` | `AVAILABLE` (`StepGrid` 16 passos) | **`Unsupported` (`PLAN`)** | `smk-s3/components/sequencer/`, `src/pcm_tiny.h` |
| **Harmony Assist (`Scale Quantizer` & `Chord Assist`)** | N/A (MIDI DSP) | `PLANNED` | `AVAILABLE` (cálculo real no `UiModel` + `HarmonyView` in-place) | **`Unsupported` no motor (`PLAN`) / Preview ativo na UI** | `smk-s3/components/midi/include/scale_quantizer.h`, `chord_memory.h` |
| **Display `800 × 480` & Monitor de Heap LVGL 9.3.0** | N/A | `AVAILABLE` (`ST7701S` + `GT911` + `lv_mem_monitor`) | `AVAILABLE` (`DiagnosticsScreen`) | **`Confirmed`** | `smk-p4/components/board/display_hal.h` |

---

## 3. Canonical Parameter Registry (`132` Parameters in `ui_parameters.cpp`)

| Namespace | Count | `AVAILABLE` / `DEMO` Parameters | `PLANNED` Parameters | Dynamic `UNAVAILABLE` Rule |
|---|---|---|---|---|
| `perf.*` | 13 | `perf.patch_id`, `perf.master_volume`, `perf.octave_shift` | `perf.bpm`, `perf.layer_mode`, `perf.macro.1` (`M1 CHAR`), `perf.macro.2` (`M2 BRIGHT`), `perf.macro.3` (`M3 MOTION`), `perf.macro.4` (`M4 SHAPE`), `perf.macro.5` (`M5 ATTACK`), `perf.macro.6` (`M6 RELEASE`), `perf.macro.7` (`M7 SPACE`), `perf.macro.8` (`M8 DRIVE`) | — |
| `osc.*` | 9 | — | `osc.1.wave`, `osc.1.pitch_st`, `osc.1.detune_ct`, `osc.1.level`, `osc.1.pan`, `osc.1.unison`, `osc.1.pulse_width`, `osc.sub_level`, `osc.noise_level` | `osc.1.pulse_width` é `UNAVAILABLE` quando `osc.1.wave != PULSE (1)` |
| `fm.*` | 8 | `fm.selected_op` (`DEMO`) | `fm.algo`, `fm.op.ratio`, `fm.op.level`, `fm.feedback`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens` | Todos `fm.*` são `UNAVAILABLE` quando `osc.1.wave != FM ALGO (8)` / família $\neq$ `FM` |
| `filter.*` | 6 | — | `filter.type`, `filter.cutoff_hz`, `filter.resonance`, `filter.drive`, `filter.keytrack`, `filter.env_amount` | `UNAVAILABLE` quando `filter.type == NONE (1)` ou família sem filtro |
| `amp.*` | 4 | `amp.vel_sens` (via MIDI no runtime; setter UI `PLANNED`) | `amp.level`, `amp.pan`, `amp.portamento_ms` | — |
| `env.*` | 5 | — | `env.amp.attack_ms`, `env.amp.decay_ms`, `env.amp.sustain`, `env.amp.release_ms`, `env.amp.curve` | — |
| `lfo.*` | 6 | `lfo.1.rate_hz`, `lfo.1.depth` (vibrato 5 Hz via CC1 no runtime; setter UI `PLANNED`) | `lfo.1.wave`, `lfo.1.phase_deg`, `lfo.1.sync`, `lfo.1.dest` | — |
| `mod.*` | 4 | `mod.slot1.source`, `mod.slot1.dest` (CC1 -> Vibrato fixo no runtime) | `mod.slot1.amount`, `mod.slot1.polarity` | — |
| `layer_a.*` | 7 | `layer_a.enable`, `layer_a.patch_id` | `layer_a.channel` (`CH A` / `LAYER A CH`), `layer_a.octave`, `layer_a.transpose`, `layer_a.key_low`, `layer_a.key_high` | — |
| `layer_b.*` | 7 | — | `layer_b.enable`, `layer_b.patch_id`, `layer_b.channel` (`CH B` / `LAYER B CH`), `layer_b.octave`, `layer_b.transpose`, `layer_b.key_low`, `layer_b.key_high` | — |
| `mixer.*` | 9 | — | `mixer.layer_a.level`, `mixer.layer_a.pan`, `mixer.layer_a.mute`, `mixer.layer_b.level`, `mixer.layer_b.pan`, `mixer.layer_b.mute`, `mixer.drums.level`, `mixer.drums.mute`, `mixer.fx_send` | — |
| `fx.*` | 10 | — | `fx.chorus.mode`, `fx.chorus.depth`, `fx.delay.time_ms`, `fx.delay.feedback`, `fx.delay.mix`, `fx.reverb.size`, `fx.reverb.mix`, `fx.drive`, `fx.master_tone`, `fx.routing` | — |
| `arp.*` | 8 | — | `arp.enable`, `arp.clock_source`, `arp.rate`, `arp.gate_pct`, `arp.octaves`, `arp.direction`, `arp.latch`, `arp.swing_pct` | — |
| `seq.*` | 7 | — | `seq.playing`, `seq.pattern`, `seq.length`, `seq.step_vel`, `seq.step_prob`, `seq.step_ratchet`, `seq.transpose` | — |
| `drums.*` | 5 | — | `drums.track`, `drums.level`, `drums.pan`, `drums.tune_st`, `drums.decay_ms` | — |
| `harmony.*` | 12 | — | `harmony.scale_enable`, `harmony.root_note`, `harmony.scale_type`, `harmony.quant_mode`, `harmony.preserve_outside`, `harmony.chord_enable`, `harmony.chord_type`, `harmony.voicing`, `harmony.inversion`, `harmony.spread`, `harmony.vel_behavior`, `harmony.dest_layer` | Sub-parâmetros dependem de `harmony.scale_enable` ou `harmony.chord_enable` |
| `midi.*` | 7 | `midi.input_channel`, `midi.bend_range_st`, `midi.sustain_enable`, `midi.modwheel_enable` (suportados no caminho USB-MIDI do runtime; configuração UI `PLANNED`) | `midi.vel_curve`, `midi.cc_learn_active`, `midi.clock_sync` | — |
| `sys.*` | 5 | `sys.sample_rate_hz` (`AVAILABLE`, `kCapReadOnly`), `sys.brightness` (`DEMO`), `sys.theme` (`DEMO`) | `sys.master_tuning_hz`, `sys.boot_mode` | `sys.sample_rate_hz` é somente leitura (`48000 Hz`) |
