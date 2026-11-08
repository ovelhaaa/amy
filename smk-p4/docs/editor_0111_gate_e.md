# SMK-P4 0.11.1 — Gate E Formal Qualification: Configurable LFO, CC1 Vibrato Composition & Modulation Matrix Slot 1

- **Repository:** `ovelhaaa/amy`
- **Branch:** `feature/smk-p4-sound-editor-completion-0111`
- **Baseline:** `feature/smk-p4-live-sound-editor-011`
- **Software Qualification Status:** `PASS`
- **Physical Hardware Validation Status:** `MANUAL / PENDING HARDWARE`

---

## 1. Scope and Parameter Mapping

### E1 — Configurable LFO (`edit_lfo_`, Native `SYNTH_IS_MOD_SOURCE`)
Every admitted Juno (`0..127`) and DX7 (`128..255`) factory patch owns a dedicated per-voice modulation oscillator (`local osc 1`, `role == SYNTH_IS_MOD_SOURCE`, referenced via `mod_source = 1`). `captureEdit()` identifies `edit_lfo_` across all 8 voices and exposes:

| Parameter ID | Enum Name | Canonical Identifier | Range / Options | Native AMY Target | Status |
|---|---|---|---|---|---|
| `48` | `edit::LfoRate` | `lfo.1.rate_hz` | `0.05 .. 30.0 Hz` | `synth[edit_map_[v][edit_lfo_]]->logfreq_coefs[COEF_CONST]` (`FREQ` delta) | **Available (PASS)** |
| `49` | `edit::LfoDepth` | `lfo.1.depth` | `0 .. 100 %` | `COEF_MOD` on target destination + `edit_lfo_` `amp_coefs[COEF_CONST]` | **Available (PASS)** |
| `50` | `edit::LfoWave` | `lfo.1.wave` | `0=SINE, 1=TRIANGLE, 2=SAW_DOWN, 3=SAW_UP, 4=PULSE` | `synth[edit_map_[v][edit_lfo_]]->wave` (`WAVE` delta) | **Available (PASS)** |
| `51` | `edit::LfoDest` | `lfo.1.dest` | `0=OFF, 1=PITCH, 2=FILTER (Juno), 3=AMP` | `logfreq_coefs[COEF_MOD]`, `filter_logfreq_coefs[COEF_MOD]`, `amp_coefs[COEF_MOD]` | **Contextual Available / N/A on FM Filter (PASS)** |

- **Contextual Destination Enforcement:**
  - `LFO → Pitch` (`dest = 1`): modulates pitched oscillators (`main_mask | sub` on Juno; root `ALGO` oscillator `o = 0` on DX7) up to `1/12` octave (`1` semitone) at `100%` depth.
  - `LFO → Filter Cutoff` (`dest = 2`): modulates `filter_logfreq_coefs[COEF_MOD]` up to `±1.5` octaves at `100%` depth on Juno patches; **rejected** (`++out.errors`) and marked `N/A` on DX7 FM patches.
  - `LFO → Amp` (`dest = 3`): modulates `amp_coefs[COEF_MOD]` up to `0.08` log-amp units (`±4.8 dB` clipping-safe tremolo) on Juno root VCA (`components.root`) and DX7 root `ALGO` (`o = 0`).
  - When `LfoDepth` and `LfoDest` are unedited (or during Compare/Revert), `applyEdit()` restores the exact captured ROM `freq_mod`, `filter_mod`, `amp_mod`, `b.amp`, and `b.amp_eg0` across all 8 voices.

### E2 & E3 — Mod Wheel (`CC1`) Composition & Modulation Matrix Slot 1
Slot 1 of the Modulation Matrix is backed by owner-confirmed parameters (`52..56`):

| Parameter ID | Enum Name | Canonical Identifier | Default | Supported Values in `0.11.1` | Status |
|---|---|---|---|---|---|
| `52` | `edit::ModEnable` | `mod.slot1.enable` | `1` (`ON`) | `0=OFF, 1=ON` | **Available (PASS)** |
| `53` | `edit::ModSource` | `mod.slot1.source` | `1` (`MOD WHL`) | `1=MOD WHL (CC1)` (`0=LFO 1, 2=AMP ENV` rejected / marked `N/A`) | **Available (PASS)** |
| `54` | `edit::ModDest` | `mod.slot1.dest` | `1` (`OSC PIT`) | `0=NONE, 1=OSC PIT (Juno/DX7), 3=FLT CUT (Juno)` (`2=SUB LVL` rejected; `3=FLT CUT` rejected on FM) | **Contextual Available / N/A (PASS)** |
| `55` | `edit::ModAmount` | `mod.slot1.amount` | `+50 %` | `-100 .. +100 %` | **Available (PASS)** |
| `56` | `edit::ModPolarity` | `mod.slot1.polarity` | `1` (`BIP +/-`) | `0=UNI (+), 1=BIP (+/-)` | **Available (PASS)** |
| — | — | `mod.slot2..4.*` | — | Additional slots (`2..4`) | **Planned (Read-only badge)** |

- **Predictable Composition with `CC1`:**
  - **Mod Wheel → Pitch (`ModDest == 1`):** In `Runtime::tune(slot)`, when `ModEnable == 1` and `ModSource == 1` and `ModDest == 1`, `CC1` (`0..127`) scales pitch vibrato by `(ModAmount / 100.0f) * (cc1 / 127.0f)` at `LfoRate` (when edited) or `5.0 Hz` (default), honoring `ModPolarity` (`UNI +` non-negative vs. `BIP +/-` symmetrical). At default Slot 1 settings (`Enable=1, Source=MOD WHL, Dest=OSC PIT, Amount=+50%, Polarity=BIP`), behavior is 100% identical to baseline `0.11`. Setting `ModEnable = 0` or `ModDest = 0` (`NONE`) cleanly disables `CC1` pitch modulation (`cc127_disabled == cc0`).
  - **Mod Wheel → Filter Cutoff (`ModDest == 3`, Juno):** `Runtime::updateModWheelFilter()` composes the manual/macro Juno filter cutoff (`base_logfreq`) with `(cc1 / 127.0f) * eff_amt * 3.0f` octaves across all 8 voices. On USB disconnect, `cancelUsb()` resets `modulation_[]` to `0` and immediately restores `base_logfreq`.

---

## 2. Automated Validation Evidence (`gate_e_test.cpp`)

Executed via:
```powershell
.\smk-p4\tests\run_sound_editor.ps1 -GateEOnly
```

### Summary of Verified Results
- **Extended Codec Capacity (`testCodecExtendedCapacity`):**
  - `25` simultaneous Juno + LFO + MOD parameters roundtrip losslessly in 160-byte V2 record (`mode = 1`).
  - `42` simultaneous FM + LFO + MOD parameters roundtrip in 160-byte V2 record (`mode = 2`).
- **LFO Qualification (`testLfoControls` on patches `0, 4, 128, 135`):**
  - Verified audible PCM differentiation for `LfoRate` (`8 Hz` vs `14 Hz`), `LfoDepth` (`0%` vs `80%`), `LfoWave` (`SINE` vs `PULSE`), and `LfoDest` (`PITCH`, `AMP`, and `FILTER` on Juno) with 8 active voices under Harmony Assist (`chord = 13`).
  - Verified rejection (`++out.errors`) of `LfoDest = 2` (`FILTER`) on DX7 patches (`128, 135`).
  - Verified bit-exact PCM restoration on Compare and Revert, and exact parameter restoration on V2 Save/Recall.
- **Modulation Matrix & `CC1` Qualification (`testModMatrixAndCc1Composition`):**
  - Verified `CC1 = 0`, `CC1 = 64`, `CC1 = 127` produce distinct PCM on Juno (`4`) and DX7 (`135`) for `Mod Wheel → Pitch`, and distinct PCM across `ModAmount` (`50%` vs `100%`), `ModPolarity` (`BIP` vs `UNI`), and `ModEnable` (`ON` vs `OFF`, where `OFF` at `CC1=127` equals `CC1=0` bit-for-bit).
  - Verified `Mod Wheel → Filter Cutoff` (`ModDest = 3`) on Juno (`0`) shifts `filter_logfreq_coefs[COEF_CONST]` across all 8 voices at `CC1 = 0, 64, 127`, composes additively in log-frequency with manual `Cutoff = 500 Hz`, restores factory cutoff on Compare, and resets cleanly on USB disconnect.
  - Verified rejection (`++out.errors`) of unsupported `ModSource` (`0, 2`) and `ModDest` (`2`).
- **Real-Time & Safety Invariants:**
  - `0` heap allocations during render, `0` clipped samples, `0` OOM events.
