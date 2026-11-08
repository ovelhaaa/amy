# SMK-P4 0.11.1 — Gate D Formal Qualification: Advanced FM Editor (`fm.feedback`, `fm.op.attack`, `fm.op.decay`, `fm.op.vel_sens`, `fm.algo`)

- **Repository:** `ovelhaaa/amy`
- **Branch:** `feature/smk-p4-sound-editor-completion-0111`
- **Baseline:** `feature/smk-p4-live-sound-editor-011`
- **Software Qualification Status:** `PASS`
- **Physical Hardware Validation Status:** `MANUAL / PENDING HARDWARE`

---

## 1. Scope and Parameter Architecture (Gate D0 & Gate D)

### Gate D0 — Scalable 64-Bit Parameter Mask
To admit per-operator FM controls, LFO parameters, and Modulation Matrix parameters without shifting existing IDs (`0..28`), `smk-p4/components/amy_runtime/sound_edit.h` widened `edit::Mask` from `uint32_t` to `uint64_t` (`edit::bit(unsigned p)`) and expanded `edit::Param` to `Count = 57`:

| Parameter ID | Enum Name | Canonical Identifier | Range | Native AMY Target | Status |
|---|---|---|---|---|---|
| `29` | `edit::FmFeedback` | `fm.feedback` | `0 .. 100 %` | `synth[edit_map_[v][0]]->feedback` (`0.0 .. 0.16f` via `FEEDBACK` delta) | **Available (PASS)** |
| `30 .. 35` | `edit::OpAttack0..5` | `fm.op1..6.attack_ms` | `0 .. 2000 ms` | `synth[edit_map_[v][edit_ops_[op]]]->breakpoint_times[0][1]` (`BP_START + 2`) | **Contextual Available / N/A (PASS)** |
| `36 .. 41` | `edit::OpDecay0..5` | `fm.op1..6.decay_ms` | `0 .. 4000 ms` | `synth[edit_map_[v][edit_ops_[op]]]->breakpoint_times[0][op_decay_index]` (`BP_START + 2*seg`) | **Contextual Available / N/A (PASS)** |
| `42 .. 47` | `edit::OpVel0..5` | `fm.op1..6.vel_sens` | `0 .. 100 %` | `synth[edit_map_[v][edit_ops_[op]]]->amp_coefs[COEF_VEL]` (`0.0 .. 2.0f`) + per-voice operator velocity propagation | **Available (PASS)** |
| — | — | `fm.algo` | `1 .. 32` | Structural (`synth[base]->algorithm` + `algo_source[0..5]`) | **Planned (Read-only badge)** |

---

## 2. Native Semantics & Implementation Details

### D1 — FM Feedback (`fm.feedback`, ID `29`)
- In `src/algorithms.c` (`render_algo`, lines 222–224), operators whose routing flag has `FB_IN` read `feedback_level = F2S(synth[osc]->feedback)` from the voice's root `ALGO` oscillator (`edit_map_[v][0]`).
- Builtin DX7 patches (`src/patches.h` `128..255`) use native `feedback` in `0.0 .. 0.16f` (`b0` .. `b0.16`).
- `captureEdit()` captures `edit_oscs_[0].feedback` and maps `0.0 .. 0.16f` to `0 .. 100%` (`feedback * 625.0f`).
- `applyEdit()` emits a native `FEEDBACK` delta on `edit_map_[v][0]` across all 8 voices, restoring `b.feedback` bit-for-bit on Compare/Revert.

### D2 — Per-Operator Attack & Decay (`fm.op.attack`, `fm.op.decay`, IDs `30..41`)
- Builtin DX7 operators (`v2..v7`) use `eg_type[0] = ENVELOPE_DX7` with 5 breakpoints (`breakpoint_times[0][0..4]`, `breakpoint_values[0][0..4]`), where segment `0` is initial level `L4` at `t=0`, segment `1` is Attack (`R1 -> L1`), segments `2` and `3` are Decay 1 / Decay 2 (`R2 -> L2`, `R3 -> L3`), and segment `4` is Release (`R4 -> L4`).
- The implementation **never** replaces the 5-segment `ENVELOPE_DX7` with a 3-point ADSR:
  - `OpAttack0..5` (`30..35`) is marked available when segment `1` has a non-zero level transition (`|L1 - L4| > 1e-4`) and its ROM duration is within `0 .. 2000 ms`.
  - `OpDecay0..5` (`36..41`) identifies the active decay segment (`op_decay_index = 2` when `|L2 - L1| > 1e-4`, or `3` when `t2 == 0` and `|L3 - L2| > 1e-4`) and is marked available when its ROM duration is within `0 .. 4000 ms`. Operators with flat sustain (`L1 == L2 == L3`, e.g., Patch `135` `DX7 PIANO 1` OP3) or multi-second decay (`> 4000 ms`, e.g., Patch `135` OP2/OP6 with `24000 ms`) are marked `N/A` in `out.available`.
  - `applyEdit()` updates only the target segment's sample count via `BP_START + 2 * seg_idx` across all 8 voices, leaving all other segments, breakpoint levels, and `ENVELOPE_DX7` curves untouched, and restoring exact captured sample counts (`b.op_attack_samples`, `b.op_decay_samples`) on Compare/Revert.

### D3 — Per-Operator Velocity Sensitivity (`fm.op.vel_sens`, IDs `42..47`)
- In AMY (`src/amy.c` `hold_and_modify`), each DX7 operator evaluates `amp_combine_controls(ctrl_inputs, synth[osc]->amp_coefs)` using `ctrl_inputs[COEF_VEL] = synth[osc]->velocity` and `synth[osc]->amp_coefs[COEF_VEL]`.
- Because `play_delta(VELOCITY)` skips `SYNTH_IS_ALGO_SOURCE` oscillators, `Runtime::note(slot, velocity)` propagates `velocity / 127.0f` to the 6 operators (`edit_map_[slot][edit_ops_[0..5]]`) of the triggered voice on Note-On (`velocity > 0`), without modifying incoming MIDI velocity or the root `ALGO` velocity scaling.
- `OpVel0..5` (`0 .. 100%`) maps to `amp_coefs[COEF_VEL]` (`0.0 .. 2.0f`). At `0%` (ROM default), `COEF_VEL == 0.0f` is skipped in `amp_combine_controls`, preserving 100% bit-exact factory PCM.

### D4 — FM Algorithm (`fm.algo`) Structural Audit
- Changing `synth[base]->algorithm` changes carrier vs. modulator roles (`edit_modulators_[0..5]`), feedback routing (`FB_IN`/`FB_OUT`), and relative gain assumptions of the 6 operators (`v2..v7`), which requires structural voice re-verification rather than a simple parameter delta.
- Per Gate D4 rules, `fm.algo` is documented as a structural operation, displayed as a read-only algorithm indicator (`ALGO <1..32>`) on the FM tab, and kept `PLANNED` (`ParameterStatus::Planned`) in `0.11.1`.

---

## 3. Automated Validation Evidence (`gate_d_test.cpp`)

Executed via:
```powershell
.\smk-p4\tests\run_sound_editor.ps1 -GateDOnly
```

### Summary of Verified Results
- **Patches Qualified:** `128` (`DX7 BRASS 1`), `135` (`DX7 PIANO 1`), `138` (`DX7 E.PIANO 1`), `142` (`DX7 BASS 1`).
- **Audible Operator Coverage:**
  - `patch=128`: `attack=6/6`, `decay=6/6`, `vel=6/6`
  - `patch=135`: `attack=6/6`, `decay=3/6` (`op=2, 3, 6` verified contextual `N/A`), `vel=6/6`
  - `patch=138`: `attack=6/6`, `decay=6/6`, `vel=6/6`
  - `patch=142`: `attack=6/6`, `decay=4/6` (`op=2, 5` verified contextual `N/A`), `vel=6/6`
- **Velocity Curve Verification (`patch=135`, `OpVel0 = 85%`):**
  - `vel=40` energy: `135,208,738`
  - `vel=80` energy: `658,877,464`
  - `vel=127` energy: `2,731,417,960` (`e40 < e80 < e127`, and `vel=127` matches unedited `vel=127` bit-for-bit).
- **Invariants Verified:**
  - 8-voice & Harmony Assist (`chord=13`) consistency: `PASS`
  - Sustain (`CC64`) hold & release + retrigger: `PASS`
  - Compare (`action 0`), Revert (`action 1`), Save As (`action 2`), Recall (`action 3`): `PASS` (bit-exact PCM restoration)
  - Operator isolation (no cross-operator mutation or `algo_source` corruption): `PASS`
  - Real-time heap allocations during render: `0` (`PASS`)
  - Audio clipping & OOM count: `0` (`PASS`)
