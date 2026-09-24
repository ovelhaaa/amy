# Patch format v4 compatibility

Status: decided for the Final FM & Patch Compatibility micro-milestone.

`format_version == 4` was produced by two different writers during a short
window, and the file format cannot distinguish them. This document records the
explicit decision so no heuristic is invented later.

## The two v4 writers

### 1. First Expansion v4 (officially supported)

Legacy enums, with the following *effective* firmware behavior:

| Field | Stored | Firmware behavior actually applied |
| --- | --- | --- |
| `wave_type` | 0 Sine, 1 SawDown, 2 SawUp, 3 Triangle, 4 Pulse | mapped to native AMY wave |
| `filter_type` | 0..3 | `if (filter_type > 0) e.filter_type = filter_type;` |
| `chorus_mode` | 0 Classic, 1 Juno, 2 Ensemble, 3 Wide, 4 Vibrato | mapped to v5 (0 Off, 1..5) |
| `drive_level` | 0..3 | clamped by the adapter to the audible 0..1 range |

Because the old adapter only forwarded a nonzero `filter_type`, the realized
filter mapping for this writer is:

```text
v4.filter_type 0 -> Inherit
v4.filter_type 1 -> LPF
v4.filter_type 2 -> BPF
v4.filter_type 3 -> HPF
```

The documented enum in old comments (`0 LPF24, 1 BPF, 2 HPF, 3 LPF12`) never
matched the DSP and must not be used for migration: migrating `0 -> LPF24`
would change the original timbre.

### 2. Intermediate stabilization v4 (not officially supported)

A later stabilization commit also wrote `format_version == 4`, but already
stored native AMY wave/filter enums and `drive_level` in `0..1`.

## Why they cannot be separated

Both writers used the same struct layout, so:

- `header.data_size` is identical (`sizeof(SynthPatchV4Legacy)` in both).
- `reserved[6]` was not populated distinctly by either writer.
- Field ranges overlap: e.g. a native filter enum in `0..8` and a legacy enum
  in `0..3` are not mutually exclusive, and legacy `filter_type == 0` is a valid
  native `SmkFilterType::None`/`Inherit` value.

No deterministic discriminator exists. A probabilistic or field-range heuristic
would silently mis-migrate some files, which this project forbids.

## Decision

- Officially supported legacy v4 is the **first Expansion v4**.
- Files written during the intermediate stabilization window have no reliable
  identification and are **not** migrated with 100% confidence. They are loaded
  using the first-Expansion interpretation. Users who still hold such files
  should re-save them (the writer always emits v5) from a known-good state.
- `data_size` is validated strictly before any payload read; a v4 header whose
  `data_size != sizeof(SynthPatchV4Legacy)` is rejected.
- Drive migration preserves the effective audible DSP level
  (`clamp(v4.drive_level, 0, 1)`), not the physical knob position.
