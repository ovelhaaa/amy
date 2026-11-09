# Native AMY FX Suite (0.12) - Parameter Matrix & UI Touchscreen Integration

## 1. Overview

Gate G connects the Native AMY FX Suite to the 800×480 touchscreen interface via LVGL 9.3 and the `UiBlueprintPerformanceAdapter` contract. Every parameter marked `Available` in the Effects Screen is backed by an active AMY engine destination on Bus 0, undergoes command tokenization and coalescing, and requires owner snapshot round-trip confirmation before displaying a confirmed state.

---

## 2. FX Parameter Matrix

| Parameter ID | Screen / Section | Label | Type | UI Range | Engine Range | Engine Destination | Status |
|---|---|---|---|---|---|---|---|
| `fx.chorus.mode` | Effects / Row 1 | CHORUS MODE | Enum | 0..5 | 0..5 (Off, Classic, Juno I, Juno II, Ensemble, Vibrato) | `config_chorus(...)` | **Available** |
| `fx.chorus.depth` | Effects / Row 1 | CHORUS DEPTH | Float | 0..100 % | 0.0..1.0 | `config_chorus(bus, level, delay, rate, depth)` | **Available** |
| `fx.delay.time_ms` | Effects / Row 1 | DELAY TIME | Float | 10..1000 ms | 10..1000 ms | `config_echo(bus, level, time_ms, 1365.33, fdbk, 0)` | **Available** |
| `fx.delay.feedback` | Effects / Row 1 | DELAY FDBK | Float | 0..95 % | 0.0..0.95 | `config_echo(..., feedback, ...)` (hard-capped at 95%) | **Available** |
| `fx.delay.mix` | Effects / Row 2 | DELAY MIX | Float | 0..100 % | 0.0..1.0 | `config_echo(bus, level, ...)` | **Available** |
| `fx.reverb.size` | Effects / Row 2 | REVERB SIZE | Float | 0..100 % | 0.40..0.95 | `config_reverb(bus, level, liveness, 0.57, 3000)` | **Available** |
| `fx.reverb.mix` | Effects / Row 2 | REVERB MIX | Float | 0..100 % | 0.0..1.0 | `config_reverb(bus, level, ...)` | **Available** |
| `fx.master_tone` | Effects / Row 3 | TILT TONE | Float | -100..+100 % | -1.0..+1.0 | `config_eq(bus, eq_l, eq_m, eq_h)` (3-band tilt macro) | **Available** |
| `fx.drive` | Effects / Row 3 | BUS DRIVE | Float | 0..100 % | N/A | Soft saturation (planned for future app DSP) | *Planned* |
| `fx.routing` | Effects / Row 3 | FX CHAIN | Enum | 0..3 | N/A | Fixed AMY chain: `EQ -> Chorus -> Echo -> Reverb` | *Planned* |
| `mixer.fx_send` | Effects / Row 2 | BUS FX SEND | Float | 0..100 % | N/A | Aux Send (bus routing fixed on Bus 0 in 0.12) | *Planned* |

---

## 3. UI Layout & Industrial Design

The Effects screen (`ScreenId::Effects`) occupies the 800×480 display with three structured parameter rows below a prominent status and routing banner:

1. **Top Banner (y = 4..42)**:
   ```text
   NATIVE AMY FX CHAIN: EQ -> CHORUS -> DELAY -> REVERB  |  HARD REAL-TIME BUS 0
   ```
   Renders the fixed topology of the native AMY signal path.

2. **Row 1 (y = 46..156)**:
   - `fx.chorus.mode` (Card 190×108)
   - `fx.chorus.depth` (Card 190×108)
   - `fx.delay.time_ms` (Card 190×108)
   - `fx.delay.feedback` (Card 190×108)

3. **Row 2 (y = 160..270)**:
   - `fx.delay.mix` (Card 190×108)
   - `fx.reverb.size` (Card 190×108)
   - `fx.reverb.mix` (Card 190×108)
   - `mixer.fx_send` (Card 190×108 - marked `PLAN`)

4. **Row 3 (y = 274..386)**:
   - `fx.drive` (Card 256×112 - marked `PLAN`)
   - `fx.master_tone` (Card 256×112)
   - `fx.routing` (Card 256×112 - marked `PLAN`)

---

## 4. Owner Confirmation Architecture

In compliance with the architecture rules:
1. When the user interacts with an FX widget, `UiBlueprintPerformanceAdapter::requestParameterFloat/Int` calls `LocalAmyPerformanceBackend::setFxParameter(id, val, now_ms)`.
2. The UI adapter returns `UiCommandState::Requested`, with notice `"FX REQUESTED; WAIT OWNER"`.
3. The backend coalesces high-frequency slider updates with a 15 ms rate limit into `commands_.fx` (bounded lock-free ring buffer of 8 requests).
4. AMY audio thread processes `applyFx()` and updates `snapshot_.fx`.
5. On the subsequent snapshot publish, `UiBlueprintPerformanceAdapter::service()` matches the confirmed owner state and transitions the command state to `UiCommandState::Confirmed` (`"FX CONFIRMED BY OWNER"`).
6. Compare and Revert actions preserve/restore the baseline `original_fx_` configuration alongside synthesis edits.

---

## 5. Verification

- **Automated Test**: `smk-p4/tests/sound_editor_ui_test.cpp` (Section 7).
- **Execution Script**: `powershell -ExecutionPolicy Bypass -File .\smk-p4\tests\run_sound_editor_ui.ps1`.
- **Result**: `PASS LVGL9.3: all 7 editor tabs, Effects screen, Gate G0 inventory audit, FM/LFO/Mod/FX owner confirmation, dirty guard, Save/Recall/reboot simulation, heap integrity`.
- **Framebuffer Artifact**: `build/editor011/effects.ppm` (800×480 RGB framebuffer verified).
