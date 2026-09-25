# Patch Identity vs. Storage Slot (Sound & Musicality M2.3)

Status: implemented, host-validated. Current patch format is
`kPatchFormatVersion == 6` (M3 added full FX persistence with v5 -> v6
migration). This document defines the contract between a patch's identity and
where a user copy is stored.

## 1. The two domains

```text
Patch ID (SynthPatch::id)
    Identity / origin of a patch. For factory patches this is the factory
    table id (0..255): 0..127 Juno/subtractive, 128..255 DX7/FM, 256+ PCM.
    It travels with the patch and is preserved when it is saved.

Storage Slot
    A user storage position on Flash SPIFFS: 0..127 (StorageManager::kMaxSlots).
    It is not part of the patch and only appears in the .s3p filename.
```

They are deliberately independent. Before M2.3 the long-hold Save passed
`activePatchId()` as the slot and `StorageManager::savePatch()` overwrote the
stored `id` with the slot. That made any factory patch with id >= 128
unsavable (slot out of range) and silently turned "factory patch 135 stored in
slot 20" into "factory patch 20".

## 2. Rules

* `StorageManager::savePatch(slot_id, patch)` validates `slot_id` against
  `kMaxSlots` and writes `patch_<slot>.s3p`. It does **not** modify
  `SynthPatch::id`; the family/engine identity (`engine_patch`, `wave_type`,
  macros, ...) is stored verbatim.
* `StorageManager::loadPatch(slot_id, patch_out)` returns the identity stored in
  the file. It does not stamp the slot onto the patch either.
* `PatchManager` keeps the selected user slot as runtime state
  (`active_storage_slot_`, default `kDefaultStorageSlot == 0`).
* `PatchManager::applyLoadedPatch(patch, slot)` records the slot a stored patch
  was loaded from (`active_patch_source_ == PatchSource::Storage`).
* `PatchManager::selectPatch(factory_id)` sets provenance to
  `PatchSource::Factory` and **leaves the selected slot untouched**, so browsing
  the factory bank cannot retarget a user Save.
* `PatchManager::saveActivePatch(storage)` writes to the selected slot.
  `PatchManager::saveActivePatch(storage, slot)` writes to an explicit slot and,
  on success, selects it so a following unqualified Save repeats there.

## 3. Save paths

| Path | Slot used |
| --- | --- |
| Hardware long-hold (Pad 16, Bank B) | `activeStorageSlot()` |
| Console `patch_save <slot>` | explicit `<slot>`, then selected |
| Console `patch_load <slot>` | records `<slot>` as selected |

The long-hold and `patch_save` both go through `PatchManager::saveActivePatch()`
and therefore produce the same snapshot (`buildPersistablePatch()`) and the same
CRC handling. A failure returns false and the UI reports `SAVE FAILED`; a
success reports `SAVED / PATCH SLOT NN`.

## 4. Default slot

When no slot has been selected, `active_storage_slot_` defaults to slot `0`
(`PatchManager::kDefaultStorageSlot`). This is a deterministic, documented,
always-valid user slot. It is intentionally **not** derived from the factory
patch id, so a factory FM patch with id 135/246/255 can still be saved without
selecting a slot first.

## 5. Acceptance coverage

* Factory ids `0`, `127`, `128`, `135`, `246`, `255` can be saved to any user
  slot `0..127`.
* `slot = factory patch id` is never used implicitly.
* A stored patch reloads with the correct selected slot and a subsequent
  unqualified Save overwrites that same slot.
* Factory selection preserves the selected user slot.
* `tests/run_smk_synth_expansion.ps1` (mocked engine) and
  `tests/run_smk_patch_integrity.ps1` (real AMY) cover the above, including
  `135 -> 17` and `255 -> 127`.
