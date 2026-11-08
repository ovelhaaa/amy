# SMK-P4 0.11.1 — Gate A: Juno ADSR Completion (`Decay` / `Sustain`)

## 1. Status

| Scope | Status |
|---|---|
| Host native AMY + LVGL 9.3 qualification | **PASS** |
| Physical Guition ESP32-P4 + PCM5102A + SMK25 V2 hardware validation | **MANUAL / NOT RUN** |

Gate A is formally accepted in software without duplicating or reimplementing the native `Decay` and `Sustain` controls already integrated into `smk-p4/components/amy_runtime`. Physical hardware verification remains pending until explicit owner flash authorization.

---

## 2. Implemented Controls & Native AMY Semantics

| UI ID | Enum (`edit::Param`) | Range / Unit | Native AMY Target | Eligibility Rule |
|---|---|---|---|---|
| `env.amp.decay_ms` | `edit::Decay` (`17`) | `5.0 .. 12000.0 ms` | `synth[osc]->breakpoint_times[0][1]` (via `BP_START + 2` delta in samples, or `e.eg0_times[1]` in ms) | Non-modulator Juno oscillator (`role != SYNTH_IS_MOD_SOURCE`, `eg_type[0] != ENVELOPE_DX7`, `max_num_breakpoints[0] >= 3`, active `bp[1]` time and value set, and native value within `[5, 12000] ms`) |
| `env.amp.sustain` | `edit::Sustain` (`18`) | `0.0 .. 100.0 %` (`0.0 .. 1.0`) | `synth[osc]->breakpoint_values[0][1]` (via `BP_START + 3` delta) | Same topology rule; native value within `[0, 100] %` |

### Topology & Restoration Guarantees
1. **Eight-Voice Materialized Topology**:
   - In `capturePatchEdit`, `Decay` and `Sustain` inspect the materialized voice oscillators (`edit_map_[0][i]`) rather than assuming fixed oscillator indices.
   - Modulator oscillators (`SYNTH_IS_MOD_SOURCE`) and DX7 envelopes (`ENVELOPE_DX7`) are excluded.
   - Patches whose decay segment time or sustain breakpoint value lies outside the safe control domain (e.g., patches `1` and `8` where native decay exceeds `[5, 12000] ms`) are marked contextually unavailable (`~(1 << param)`) rather than fabricating an invalid baseline.
2. **Exact Sample-Domain Restoration (Compare / Revert)**:
   - Native AMY stores `breakpoint_times[0][1]` in audio samples (`uint32_t`). Rounding to integer milliseconds during capture could introduce sub-millisecond drift on Compare/Revert if re-converted from `ms`.
   - `OscBackup` preserves both exact `decay_samples` (`synth[osc]->breakpoint_times[0][1]`) and `sustain` (`synth[osc]->breakpoint_values[0][1]`).
   - During `Compare` (`editAction(0)`) or `Revert` (`editAction(1)`), when `Decay` or `Sustain` is not in the active override mask, `applyEdit` emits direct `BP_START + 2` (sample count) and `BP_START + 3` (float level) deltas across all 8 voices (`edit_map_[v][i]`), guaranteeing byte-identical PCM restoration on fresh notes.
3. **Held-Note Safety & Non-Retrigger**:
   - Live edits to `Decay` and `Sustain` update `breakpoint_times[0][1]` and `breakpoint_values[0][1]` via `amy_add_event` / delta queue without setting `velocity` or `midi_note`, preserving `synth[i]->note_on_clock`, active voice ownership, sustain pedal state (`CC64`), and Harmony Assist chord groups.

---

## 3. Executable Validation & Evidence

### Commands Executed
```powershell
./smk-p4/tests/run_sound_editor.ps1 -GateAOnly
./smk-p4/tests/run_sound_editor.ps1
./smk-p4/tests/run_sound_editor_ui.ps1
```

### Verified Acceptance Criteria (`smk-p4/tests/gate_a_test.cpp`)
- **Native Breakpoint Verification (64 Oscillators / 8 Voices)**:
  - Across Juno patches `0, 1, 2, 3, 4, 8, 31`:
    - Eligible ADSR oscillators across all 8 voices update `breakpoint_times[0][1]` to `lround(value_ms * 48000 / 1000)` when `edit::Decay` is active, and restore exact `orig_bp_times[i][1]` on `Compare` and `Revert`.
    - Eligible ADSR oscillators across all 8 voices update `breakpoint_values[0][1]` to `value_pct / 100.0f` when `edit::Sustain` is active, and restore exact `orig_bp_vals[i][1]` on `Compare` and `Revert`.
    - All other breakpoint indices (`j != 1`) and modulator oscillators (`SYNTH_IS_MOD_SOURCE`) remain strictly unchanged.
    - Patches `1` and `8` report contextual `N/A` when native decay segment time exceeds the slider domain (`> 12000 ms`), while patches `0, 2, 3, 4, 31` pass full 1-voice and 8-voice PCM qualification for both `Decay` and `Sustain`.
- **PCM Audio Verification (1 Voice & 8-Voice Harmony Assist)**:
  - Every qualified `(patch, param, harmony_on)` combination renders `65,536` stereo int16 samples (`128` blocks × `256` frames) covering:
    - Note On (`C4`, velocity `100`), live held-note parameter update at block `32` (`note_on_clock` invariant across all 64 oscillators), `CC64` sustain pedal hold (blocks `60..80`), Note Off release, and retrigger at block `96`.
    - Assertions passed:
      - `edited != original`
      - `compared == original` (byte-identical)
      - `reverted == original` (byte-identical)
      - `recalled == edited` (byte-identical after V2 NVS Save, Recall, and simulated reboot)
- **Storage & V1 Compatibility**:
  - `Save/Recall` persists `Decay` and `Sustain` in `sound011v2` (`160 B` record, CRC32 + `kRomCompatibility`).
  - Legacy `V1` presets (`sound011`, `128 B` record) remain readable alongside `V2` without corrupting or falsely masking `Decay`/`Sustain`.
- **Real-Time Constraints**:
  - `render_allocations = 0` (`heap_probe` wrapping `malloc`/`calloc`/`realloc`/`free` during `runtime.apply` + `runtime.render`).
  - `clipping = 0` and `edit.errors = 0` across all test cases.
