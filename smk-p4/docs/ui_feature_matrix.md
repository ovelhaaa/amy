# SMK-P4 0.13.1 — Feature & Parameter Classification Matrix

Esta matriz classifica todas as funcionalidades e parâmetros do SMK-P4 na versão **0.13.1**, refletindo fielmente as capacidades consolidadas entre as quatro camadas da arquitetura:

1. **Motor AMY (`src/`)**: Síntese nativa upstream (aditivo, FM, Juno, filtros, efeitos, PCM).
2. **Runtime P4 Standalone (`smk-p4/components/amy_runtime/` + `smk-p4/main/performance_backend.h`)**: Recursos materializados no firmware P4 standalone de 8 vozes @ 48 kHz.
3. **Controle UI (`smk-p4/components/ui_blueprint/`)**: Widgets interativos, visualizadores e modelo `UiModel` / `IUiBackend`.
4. **Integração Fim a Fim (`End-to-End P4 0.13.1`)**: Comportamento real do sintetizador via `UiBlueprintPerformanceAdapter` e `LocalAmyPerformanceBackend`.

---

## 1. Legenda de Classificação

| Status | Badge UI | Significado Técnico em 0.13.1 |
|---|---|---|
| **`AVAILABLE`** | `AVAIL` (`#67C4A5`) | Totalmente funcional de ponta a ponta: controle visual, despacho assíncrono, confirmação pelo owner AMY e áudio em tempo real. |
| **`CONDITIONAL`** | `AVAIL` / `N/A` | Disponível condicionalmente dependendo da família de síntese ativa (Juno vs DX7 FM) ou estado de notas. |
| **`READ ONLY`** | `AVAIL` | Informação ou telemetria em tempo real derivada do motor, sem controle de escrita direta pela UI. |
| **`PLANNED`** | `PLAN` (`#E4A44D`) | Recurso reservado no modelo para a versão 0.14 (Performance Control / Sequencer / Layers). |
| **`UNAVAILABLE`** | `N/A` (`#80919F`) | Inaplicável à família de síntese selecionada (avaliado por `UiModel::effectiveStatus(id)`). |
| **`DEMO`** | `DEMO` (`#EAB65B`) | Simulação em RAM exclusiva do modo de demonstração sem motor AMY (`UiDemoBackend`). |

---

## 2. Matriz de Subsistemas (0.13.1 Consolidado)

| Subsistema / Funcionalidade | 1. Motor AMY | 2. Runtime P4 Standalone | 3. Controle UI | 4. Fim a Fim 0.13.1 | Evidência no Repositório |
|---|---|---|---|---|---|
| **Catálogo Factory (Juno 0..31, DX7 128..159)** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `patch_library.h`, `kPatches[]` |
| **Editor Síntese Juno (`osc.*`, `filter.*`, `env.*`)** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `sound_edit_runtime.inc`, `sound_editor_screen.cpp` |
| **Editor Síntese FM 6-Op (`fm.op.*`, `fm.depth`, `feedback`)**| `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `sound_edit_runtime.inc`, `sound_editor_screen.cpp` |
| **Seletor de Algoritmo FM (32 Algoritmos + Diagrama)** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `fm_algorithm_selector.cpp`, `fm_topology_diagram.cpp` |
| **Compare & Revert** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `local_performance_backend.h`, `sound_editor_screen.cpp` |
| **User Presets (16 slots V1/V2 transacionais)** | N/A | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `preset_transaction.h`, `user_preset_nvs_v2.h` |
| **Diagnósticos Tipados de Presets** | N/A | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `PresetRejectReason`, `local_performance_backend.h` |
| **Acompanhamento Assíncrono Save/Recall** | N/A | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `PresetOperationState`, `UiBlueprintPerformanceAdapter` |
| **Confirmação Protegida de Overwrite** | N/A | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `sound_editor_screen.cpp` |
| **Efeitos Master (Chorus, Delay, Reverb, EQ Tone)** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `fx_runtime.inc`, `effects_screen.cpp` |
| **Harmony Assist (Scale Quantizer, Chords)** | N/A | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `harmony.h`, `harmony_runtime.cpp` |
| **Teclado Virtual & Panic** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`AVAILABLE`** | `virtual_keyboard.cpp`, `performance_backend.h` |
| **Telemetria de Sistema & Diagnósticos** | `AVAILABLE` | `AVAILABLE` | `AVAILABLE` | **`READ ONLY`** | `diagnostics_screen.cpp`, `PRESET_DIAG`, `FM_ALGO_DIAG` |
| **Layers A/B & Split Zones** | `AVAILABLE` | `CONDITIONAL` (Layer A ativa) | `AVAILABLE` | **`PLANNED` (0.14)** | `layers_screen.cpp` |
| **Arpeggiator, Sequencer & Drum Grid** | `AVAILABLE` | `PLANNED` | `AVAILABLE` | **`PLANNED` (0.14)** | `step_grid.cpp`, `sequencer_screen.cpp` |
| **Performance Macros 1–8** | `AVAILABLE` | `CONDITIONAL` (M2, M5, M6 mapeados) | `AVAILABLE` | **`PLANNED` (0.14)** | `performance_screen.cpp` |

---

## 3. Matriz Canônica de Parâmetros por Namespace

| Namespace | Total | Status 0.13.1 | Comportamento Dinâmico |
|---|---|---|---|
| `perf.*` | 13 | `perf.patch_id`, `perf.master_volume`, `perf.octave_shift`: `AVAILABLE`. Macros 2, 5, 6: `AVAILABLE`. Demais macros e BPM: `PLANNED`. | — |
| `osc.*` | 9 | `osc.1.level`, `osc.sub_level`, `osc.noise_level`, `osc.1.pulse_width`, `osc.1.detune_ct`: `AVAILABLE` (Juno). | `N/A` sob patches DX7 FM. |
| `fm.*` | 8 | `fm.algo`, `fm.selected_op`, `fm.depth`, `fm.feedback`, `fm.op.ratio`, `fm.op.level`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens`: `AVAILABLE`. | `N/A` sob patches analógicos Juno. |
| `filter.*` | 6 | `filter.cutoff_hz`, `filter.resonance`, `filter.env_amount`, `filter.keytrack`: `AVAILABLE` (Juno). | `N/A` sob patches DX7 FM. |
| `amp.*` | 4 | `amp.level`, `amp.pan`, `amp.vel_sens`: `AVAILABLE`. `amp.portamento_ms`: `PLANNED`. | — |
| `env.*` | 5 | `env.amp.attack_ms`, `env.amp.decay_ms`, `env.amp.sustain`, `env.amp.release_ms`: `AVAILABLE` (Juno). | `N/A` sob patches DX7 FM (usam envelopes 6-Op dedicados). |
| `lfo.*` | 6 | `lfo.1.rate_hz`, `lfo.1.depth`, `lfo.1.wave`, `lfo.1.dest`: `AVAILABLE`. | — |
| `mod.*` | 5 | `mod.slot1.enable`, `mod.slot1.source`, `mod.slot1.dest`, `mod.slot1.amount`, `mod.slot1.polarity`: `AVAILABLE`. | — |
| `fx.*` | 10 | `fx.chorus.mode`, `fx.delay.time_ms`, `fx.delay.feedback`, `fx.delay.mix`, `fx.reverb.size`, `fx.reverb.mix`, `fx.master_tone`: `AVAILABLE`. | — |
| `layer_a.*` / `layer_b.*` | 14 | `layer_a.patch_id`, `layer_a.enable`: `AVAILABLE`. Demais: `PLANNED`. | — |
| `mixer.*` | 9 | `PLANNED` (0.14) / Visualizável em Demo. | — |
| `arp.*` / `seq.*` / `drums.*` | 24 | `PLANNED` (0.14) / Step grid interativo em Demo RAM. | — |
