# Sound & Musicality M3 - FX Architecture, Persistence & Signal Chain

Status: implemented, host-validated. Not hardware-validated (no ESP32-S3 board in
the host CI). `kPatchFormatVersion == 6`.

This document is the M3 audit that the tuning and persistence changes are based
on. It records what the code actually does, not what the comments claimed.

## 1. FX control -> engine map (as implemented)

| UI control (Bank D / engine) | `FxControlState` / `ManualControlState` | `SynthPatch` v6 | AmyAdapter | AMY event |
| --- | --- | --- | --- | --- |
| CHO MODE | `fx_state_.chorus_mode`, `active_patch_.chorus_mode` | `chorus_mode` (u8) | `setChorus(depth, rate, level)` via `applyActiveChorusState()` | `chorus_level`, `chorus_depth`, `chorus_lfo_freq` |
| CHO DEPTH (engine knob 4) | `manual_state_.chorus_depth` -> `fx_state_.chorus_depth` | `chorus_depth` (0..1) | `setChorus` | as above |
| DELAY SYNC (Bank D knob 1) | `manual_state_.delay_time_ms` | `delay_time_ms` (10..1200 ms) | `setDelay` | `echo_delay_ms`, `echo_feedback`, `echo_level` |
| DELAY TIME (engine knob 5) | `manual_state_.delay_time_ms` | `delay_time_ms` | `setDelay` | as above |
| DELAY FB (Bank D knob 2) | `manual_state_.delay_feedback` (0..0.95) | `delay_feedback` | `setDelay` | `echo_feedback` |
| DELAY MIX (Bank D knob 3) | `manual_state_.delay_mix` (wet^2) | `delay_mix` (0..1) | `setDelay` | `echo_level` |
| REV SIZE (Bank D knob 4) | `manual_state_.reverb_size` | `reverb_size` (0..1) | `setReverb(size, damp, mix)` via `applyActiveReverbState()` | `reverb_liveness`, `reverb_damping`, `reverb_level` |
| REV MIX (Bank D knob 5 / engine 6) | `manual_state_.reverb_mix` (wet^2) | `reverb_mix` (0..1) | `setReverb` | as above |
| FREEZE (patch-level only) | `amy_adapter_->reverbFreeze()` | `reverb_freeze` (u8) | `setReverbFreeze` -> `config_reverb_freeze` | (direct call, not an event) |
| DRIVE (Bank D knob 6 / engine 8) | `manual_state_.drive` (drive^2) | `drive_level` (0..1) | `setDrive` -> `smk_bus_postprocess_hook` | (bus post-process saturator) |
| TONE (Bank D knob 7) | `manual_state_.master_tone` (-1..1) | `master_tone` (-1..1) | `setMasterTone` -> bus EQ | `eq_l`, `eq_h` |

Ranges and curves live in `control_mappings.h`:
`cutoff`/`envelope`/`delay` geometric, `resonance`/`drive`/`wet` squared,
`delay_sync` discrete.

## 2. Real FX signal chain (verified in `src/amy.c:amy_fill_buffer()`)

Per synth bus, per audio block:

```text
voices / synth output
    -> per-bus EQ               (setMasterTone writes eq_l/eq_h here)
    -> chorus                   (LFO-modulated variable delay, stereo inverted)
    -> echo / delay             (fixed delay + feedback)
    -> reverb                   (stereo reverb, freeze-capable)
    -> smk bus post-process     (drive saturation)
    -> bus volume + master gain
    -> AMY internal soft clip   (lookup table)
    -> I2S
```

Two consequences worth recording, because the API order is misleading:

* **master tone is pre-FX, not post-FX.** It is the bus parametric EQ and runs
  before chorus/delay/reverb.
* **drive is post-FX** (the bus post-process hook), so it saturates the wet
  chorus/delay/reverb mix as well as the dry voice. This is the intended
  "master saturation" behavior; it was not changed in M3.

The delay is a fixed-delay + feedback design; changing `echo_delay_ms` moves the
write/read distance across a block boundary. AMY does not resample the delay
line, so a large delay-time jump produces a buffer discontinuity (a click/pitch
edge). The control path therefore only changes delay time in discrete steps
(sync subdivisions or free knob moves); no tape-style pitch glide is claimed.

## 3. Patch format v6

`SynthPatch` gained six persisted fields after `reverb_freeze`:

```text
chorus_depth    float  0.0 .. 1.0
delay_time_ms   float  10.0 .. 1200.0
delay_feedback  float  0.0 .. 0.95
delay_mix       float  0.0 .. 1.0
reverb_size     float  0.0 .. 1.0
reverb_mix      float  0.0 .. 1.0
```

`drive_level`, `master_tone`, `chorus_mode` and `reverb_freeze` already existed
and are re-used, not duplicated. The old layout is frozen as `SynthPatchV5`
(plus `calculatePatchV5Crc32`), and the header `format_version` is 6.

Storage rules (`StorageManager::loadPatch`):

| version | behavior |
| --- | --- |
| 6 | exact `sizeof(SynthPatch)` check, current CRC |
| 5 | `sizeof(SynthPatchV5)` check, v5 CRC, `migratePatchV5ToV6()` |
| 4 | legacy `SynthPatchV4Legacy`, migrate with FX defaults |
| 3 | `SynthPatchV3`, migrate with FX defaults |
| other | rejected |

A truncated or wrong-size payload is rejected before any partial read.
Unheadered legacy files are also probed (v6, then v5, then v3) with CRC.

## 4. v5 -> v6 migration

Deterministic, no inference from unrelated parameters:

```text
v5 chorus_mode   -> v6 chorus_mode
v5 drive_level   -> v6 drive_level
v5 master_tone   -> v6 master_tone
v5 reverb_freeze -> v6 reverb_freeze

new in v6:
chorus_depth   = 1.0 if chorus_mode != Off else 0.0   (v5 auto-enabled full depth)
delay_time_ms  = 350.0
delay_feedback = 0.4
delay_mix      = 0.0
reverb_size    = 0.7
reverb_mix     = 0.0
```

The chorus rule is the only compatibility special case: v5 never persisted depth
and enabled full depth the first time a non-Off mode was selected, so a migrated
v5 patch with a selected mode keeps sounding the same. Everything else uses the
v5 runtime defaults exactly.

Fixture/test: `test_storage_manager_real_files_and_migration()` writes a hand
built v5 header+payload and asserts the migrated values and CRC.

## 5. Manual vs. effective state

| State | Meaning | Persisted |
| --- | --- | --- |
| `manual_state_` | user detailed-control base (manual FX) | yes, as the v6 FX base |
| `fx_state_` | effective runtime FX after macro composition | no (derived) |
| `active_patch_` | effective musical state + macro positions | macro positions + metadata |

`buildPersistablePatch()` writes `manual_state_` for every macro-affected field
(when the relative model is active), and `fx_state_` for legacy/no-route patches
(which have no separate manual base and are never recomposed). `applyPatchToEngine()`
overwrites every `fx_state_` field from the loaded patch, so patch A can never
leave FX values behind in patch B.

## 6. Chorus character profiles (M3)

`PatchManager::applyActiveChorusState()` owns the canonical table. `depth` and
`level` are maxima scaled by the user depth multiplier; `rate` is the LFO in Hz.

| mode | base depth | rate | base level | character |
| --- | --- | --- | --- | --- |
| Off | 0.00 | 0.00 | 0.00 | bypass |
| Classic | 0.30 | 0.55 | 0.55 | subtle widening |
| Juno | 0.65 | 0.45 | 0.80 | lush |
| Ensemble | 0.95 | 0.85 | 0.95 | large / dense |
| Wide | 1.10 | 0.30 | 0.75 | slow, obvious width |
| Vibrato | 0.45 | 5.00 | 0.75 | clear pitch modulation |

The modes are separated in both rate and depth so they are audibly distinct. The
engine clamps depth to 0..2, rate to 0..10 Hz and level to 0..1.5 as a safety net.
`AmyAdapter::executeChorusMode()` retains a legacy fallback table but is only
reachable through the raw AMY console passthrough.

## 7. Reverb and drive character

* Reverb damping now tracks size (`damp = clamp(0.85 - 0.5*size, 0.30, 0.85)`)
  so small sizes are dark/absorptive (ambience, glue) and large sizes keep air
  (lush hall). Previously damping was a fixed 0.7, which made all sizes sound
  similar.
* Drive is a compensated saturator: `k = 1 + 3.5*drive`,
  `comp = 1/(1 + 0.45*drive)`, `y = comp * x*k / (1 + 0.5*|x*k|)`. At
  `drive <= 0.001` the hook returns without touching the buffer, so `drive = 0`
  is exactly unity/neutral. The compensation keeps the loudness change modest as
  drive rises.
* Master tone is neutral at 0 (eq_l = eq_h = 1.0), darkens for negative values and
  brightens for positive values; `BRTE`/`EDGE` compose their tone offsets on top
  of the manual tone.

## 8. Command efficiency

`recomputeMacroTargets()` emits a command only when the derived effective value
changed (`> 1e-4`), so identical macro positions and manual state are silent.
Moving one manual FX control also recomputes once; there is no per-knob-tick
command storm. Tests: `test_macro_command_throttle`,
`test_manual_state_change_command_throttle`, `test_fx_100_cycle_persistence`.

## 9. Known limitations

* FM Bank B runtime edits remain non-persistable (unchanged from M2; out of M3
  scope). Macro positions and the engine preset baseline still round-trip.
* No hardware audio measurement was performed; tuning is based on the code and
  host tests. Hardware listening is required to confirm the profiles.
* The delay line is not resampled, so very large delay-time jumps can click.
* Parameter smoothing: M3 did not add new ramps. AMY applies chorus/echo/reverb
  parameters per block; the control layer only issues discrete updates. No
  zipper-noise fix was added because no measured audio artifact was available to
  justify the added state.
