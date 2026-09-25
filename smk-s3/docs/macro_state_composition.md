# Macro State Composition (Sound & Musicality M2.1 + M2.2)

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

## 6. Save/reload contract (Sound & Musicality M2.2)

The patch format stays v5 (`kPatchFormatVersion == 5`); no new fields, no v6.

A persisted patch must represent:

```text
MANUAL STATE + MACRO POSITIONS
```

and never `FINAL MACRO-PROCESSED STATE + MACRO POSITIONS`. Otherwise the macro
is applied twice on reload (manual 3000 Hz + CHAR +1 octave would save 6000 and
reload to 12000).

`PatchManager::buildPersistablePatch()` produces the semantic snapshot:

* starts from `active_patch_` (metadata, engine patch, non-macro controls,
  macro names/defaults/positions/mappings);
* substitutes the manual base into the macro-affected v5 fields:
  `filter_cutoff`, `filter_res`, `filter_env_amount`, `amp_attack`,
  `amp_decay`, `amp_sustain`, `amp_release`, `osc_detune`, `drive_level`,
  `master_tone`;
* leaves `crc32 == 0`; `StorageManager::savePatch()` is the single place that
  recomputes and stamps the CRC (no CRC inconsistency is produced). As of M2.3
  `savePatch()` no longer rewrites `id`: identity is preserved and the slot
  lives only in the filename. See `patch_storage_semantics.md`;
* never exposes `manual_state_` to `StorageManager`.

`PatchManager::applyLoadedPatch()` is the single apply path shared by factory
selection (`selectPatch`) and stored-patch reload. After applying the snapshot
it re-runs the relative macro composition, so a snapshot with non-neutral macro
positions reaches the same effective state it had before save. Legacy absolute
macros (`param_type` 0..7) are persisted as their applied value and are not
recomposed. Stored-patch reload uses the slot-aware overload
(`applyLoadedPatch(patch, slot)`), which records the source slot separately from
the patch identity.

## 6b. Patch identity vs. storage slot (Sound & Musicality M2.3)

`SynthPatch::id` is the patch's own identity and is never used as a storage
address. The user storage slot (0..127) is runtime metadata held by
`PatchManager::active_storage_slot_` and encoded only in the `.s3p` filename.
Selecting a factory patch does not change the selected slot, and the long-hold
Save targets the selected slot, so factory DX7 patches with id >= 128 remain
savable. The full contract, including the default slot and the console behavior,
is in `patch_storage_semantics.md`.

### Legacy and mixed mappings

`buildPersistablePatch()` only substitutes the manual base for `RelativeOnly`
and `Mixed`. `LegacyOnly` and `None` are persisted as-is; legacy behavior and
legacy mappings are unchanged. Loading a legacy patch still yields a valid
persistable snapshot.

### FX runtime-only fields in v5

Only `drive_level`, `master_tone`, `chorus_mode` and `reverb_freeze` exist in
the v5 `SynthPatch`. Therefore the following runtime FX controls do **not**
round-trip yet: chorus depth, delay time, delay feedback, delay mix, reverb
size, reverb mix. `DriveRelative` and `MasterToneRelative` are persisted
(`drive_level`, `master_tone`); `ChorusDepth`, `ReverbMix` and `DelayMix` are
runtime-only. Full FX patch persistence is deferred to Sound & Musicality M3.

### FM runtime-only fields in v5

`SynthPatch` has no FM operator/runtime fields. Bank B FM runtime edits
(`manual_state_.fm_*`, seeded from the preset) are **not** persistable in v5.
Macro positions still round-trip and are applied exactly once; the preset
operator baseline is reloaded from the engine preset, and algorithm/routing are
never changed. Explicit limitation: **FM Bank B runtime edits are not yet
persistable in v5**.

## 7. Validation

* `tests/run_smk_synth_expansion.ps1`: manual/macro composition, order
  independence with manual state, 100-cycle no-drift, FM manual+macro
  composition and ratio composition (`1.5 x 3 x 2 = 9`), mixed mapping
  classification, command throttle, legacy/relative profiles, v5 format, and
  M2.2 persistence: persistable snapshot uses the manual base, 3000+1 octave
  round-trips to 6000 (never 12000), neutral macro round-trip, multiple-macro
  round-trip, 100-cycle save/reload no-drift, subtractive roundtrip and legacy
  snapshot validity. M2.3 storage-slot semantics: factory FM 135 -> slot 17 and
  factory 255 -> slot 127 save/load with identity preserved, factory selection
  keeps the active slot, and user-slot overwrite follows the slot rather than
  the patch id.
* `tests/run_smk_patch_integrity.ps1`: real AMY + `PatchManager` FM integration;
  manual FM controls compose with macros and neutral restores the manual state
  with algorithm/routing unchanged; FM save/reload through the real
  `StorageManager` (patches 128 and 135) keeps algorithm/routing/operator
  baseline and applies macros once; M2.3 factory 135 -> slot 17 and 255 ->
  slot 127 through the shared save path.
* `tests/run_smk_amy_boot.ps1`, `tests/run_smk_safety.ps1`: unchanged and
  passing.
