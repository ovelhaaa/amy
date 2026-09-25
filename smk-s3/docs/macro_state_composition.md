# Macro State Composition (Sound & Musicality M2.1)

Status: implemented, host-validated. Patch format is unchanged (`kPatchFormatVersion == 5`).

## 1. Problem

Before M2.1 the macro engine recomputed every target from a baseline captured at
patch load. Editing a detailed control (Bank B/C/D) wrote straight into the
final engine state, but the next `recomputeMacroTargets()` started again from
the load-time baseline. A manual edit was therefore discarded as soon as any
macro moved.

## 2. Layered state model

```text
PATCH BASELINE
    -> MANUAL STATE          (ManualControlState, runtime only)
    -> MACRO CONTRIBUTIONS
    -> FINAL ENGINE STATE    (active_patch_ / fx_state_ / fm_state_)
```

`ManualControlState` (in `macro_profile.h`) is the user's detailed-control base.
On patch load it is initialized equal to the patch/FX state. It is never stored
in the patch and never overwritten by the applied result, so there is no path
back from the final state into the manual base (no drift, no feedback loop).

### Ownership of state

| State | Meaning |
| --- | --- |
| `active_patch_` | Final effective musical state (what the engine is playing). Also carries the persisted macro *positions*. |
| `fx_state_` | Final effective FX state. |
| `fm_state_` | Final effective FM runtime state. |
| `manual_state_` | Detailed-control base state. Runtime only; never persisted. |

## 3. Composition rules

Banks B/C/D and the engine knobs now update `manual_state_` and then call
`recomputeMacroTargets()`. The final state is derived only from the manual base
and the current macro positions:

```text
final_cutoff  = manual_cutoff  * exp2(macro_cutoff_octaves)
final_res     = manual_res     * macro_res_factor
final_attack  = manual_attack  * macro_attack_factor
final_detune  = manual_detune  + macro_detune_offset
final_chorus  = manual_chorus  + macro_chorus_offset
final_reverb  = manual_reverb  + macro_reverb_offset
final_delay   = manual_delay   + macro_delay_offset
final_drive   = manual_drive   + macro_drive_offset
final_tone    = manual_tone    + macro_tone_offset
```

Non-macro controls (osc mix/wave/sub/noise/transpose, key tracking, delay time,
delay feedback, reverb size, chorus mode) apply directly; they are not part of
the macro composition.

### FM

```text
final_mod       = clamp(manual_mod * macro_mod, 0, 8)
final_ratio     = manual_ratio * manual_freq_mult * macro_ratio
final_detune    = manual_detune_cents + macro_detune_offset
final_feedback  = clamp(manual_feedback + macro_feedback_offset, 0, 0.16)
algorithm       = manual only; macros never change it
routing         = never changed by any macro
```

The manual FM feedback/algorithm base is seeded from the real loaded preset the
first time the preset has materialized (`syncFmStateFromBaseline()`), so a
neutral macro restores the preset's own feedback rather than assuming zero.
`freq_mult` and the continuous ratio are folded in exactly once.

## 4. Determinism

* Macro order independence is preserved: contributions are accumulated from the
  macro positions only, never applied incrementally.
* Neutral restoration returns to the manual base, not the factory baseline.
* Repeating `manual edit -> macro extreme -> neutral` does not drift.
* The recompute emits an engine command only when the derived final value
  changes, so the same manual state and macro positions are silent.

## 5. Legacy and mixed macro mappings

`classifyMacroMappings()` classifies the persisted routes:

* `LegacyOnly` (`param_type` 0..7): original absolute handler, unchanged.
* `RelativeOnly`: the M2 family-aware model.
* `Mixed`: both kinds in one patch. This is unsupported; the relative model is
  applied and the legacy routes are reported with a warning instead of being
  silently ignored.

## 6. Save behavior (unchanged in this milestone)

The patch format stays v5. `StorageManager::savePatch()` persists the whole
`SynthPatch`, which means:

* macro positions (`SynthPatch::macros[].current_val`) are persisted;
* the effective parameter values currently in `active_patch_` are persisted;
* `manual_state_` is runtime-only and is **not** persisted.

Reloading while macros are active is therefore not yet a fully defined
round-trip: the saved effective values are loaded as the new baseline and macro
positions are restored, but the load path does not re-run the composition. This
milestone only avoids corruption; a versioned "manual + macro positions" save
model is deferred to a later milestone and must not be added without a format
version bump.

## 7. Validation

* `tests/run_smk_synth_expansion.ps1`: manual/macro composition, order
  independence with manual state, 100-cycle no-drift, FM manual+macro
  composition and ratio composition (`1.5 x 3 x 2 = 9`), mixed mapping
  classification, command throttle, legacy/relative profiles, v5 format.
* `tests/run_smk_patch_integrity.ps1`: real AMY + `PatchManager` FM integration;
  manual FM controls compose with macros and neutral restores the manual state
  with algorithm/routing unchanged.
* `tests/run_smk_amy_boot.ps1`, `tests/run_smk_safety.ps1`: unchanged and
  passing.
