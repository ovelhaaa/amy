# Preset Transaction Coordination
## SMK-P4 0.12.1

### Architecture Overview
In SMK-P4, a user preset is defined as a cohesive logical entity:
$$\text{User Preset} = \text{Identity} + \text{Synthesis Parameters} + \text{Performance Macros} + \text{Audio FX}$$

Prior to 0.12.1, synthesis and FX presets were saved through independent mechanisms. A power failure or write error between synthesis and FX storage could leave the system in an inconsistent state (e.g., new synthesis with old FX, or partial configuration).

`PresetTransactionCoordinator` solves this by introducing a 2-phase coordinated commit protocol across independent dual-bank storage partitions without altering partition tables or breaking backward compatibility.

### Commit Marker (`MK12`)
To bind the synthesis bank and FX bank together, a 32-byte packed, CRC-32 protected commit marker is written to namespace `sound012fx`:

```cpp
#pragma pack(push, 1)
struct CommitMarker {
    uint32_t magic;          // 0x32314b4d ("MK12")
    uint16_t version;        // 1
    uint16_t sequence;       // Monotonic coordinator sequence (1..65535)
    uint8_t  slot;           // Preset slot (0..15)
    uint8_t  synth_bank;     // Committed synthesis bank (0 = A, 1 = B)
    uint16_t synth_seq;      // Committed synthesis sequence number
    uint8_t  fx_bank;        // Committed FX bank (0 = A, 1 = B)
    uint8_t  reserved[3];    // Zero-padding
    uint16_t fx_seq;         // Committed FX sequence number
    uint32_t patch_rom;      // Origin patch ROM identifier
    uint32_t rom_compat;     // 0x3e76efb6
    uint32_t crc32;          // IEEE 802.3 CRC-32 of bytes 0..27
};
#pragma pack(pop)
```

### Commit Protocol Flow & Logical Commit Boundary
1. **Legacy Slot Inspection (Gate A)**:
   - Before writing any candidate bank, `inspectLegacySlot()` scans `sound011` (V1 64B, V2 128B) and `sound012fx` (FX 32B).
   - If no `CommitMarker` exists but a valid legacy preset is present, a baseline marker pointing to the active legacy revision is written first, guaranteeing that any aborted candidate write rolls back cleanly to the legacy preset.
   - Candidate writes target inactive banks without assuming Bank A is free.
2. **Phase 1A: Write Candidate FX**: Write FX configuration blob to `target_fx_bank` with incremented sequence number. Readback verify.
3. **Phase 1B: Write Candidate Synth**: Write synthesis preset blob to `target_synth_bank` with incremented sequence number. Readback verify.
4. **Phase 2: Commit Marker (Commit Boundary)**:
   - Write `CommitMarker` binding `synth_seq` and `fx_seq` to marker ping-pong bank (`c<slot>a` or `c<slot>b`).
   - The valid write of the marker constitutes the formal logical commit point.
5. **Phase 3: Readback Verification & Outcome Semantics (Gate B)**:
   - If a failure occurs before the marker write: status is `SaveResult::Status::FailedBeforeCommit` (`"SAVE FAILED: PRE-COMMIT"`). The previous committed revision is active.
   - If marker write succeeds and readback confirms it: status is `SaveResult::Status::Success` (`"SAVED"`).
   - If readback encounters hardware I/O errors preventing verification: status is `SaveResult::Status::CommitOutcomeUnknown` (`"SAVE INDETERMINATE: VERIFY PRESET"`). UI and backend prevent destructive patch changes until resolved.

### Recovery Protocol
On recall or reboot:
1. Load candidate commit markers from banks A and B.
2. Select marker with valid CRC, matching magic, version, and highest monotonic sequence number (`seqAfter16`).
3. If marker is valid:
   - Read synthesis bank specified by marker (`synth_bank`). Verify sequence matches `marker.synth_seq`.
   - Read FX bank specified by marker (`fx_bank`). Verify sequence matches `marker.fx_seq`.
   - If both match and are intact: return committed pair.
   - If candidate write was interrupted during Phase 1, the prior marker cleanly points to the undisturbed prior banks.
4. If marker is missing (unmigrated legacy preset):
   - Gracefully fallback to latest valid synthesis preset (V2 or V1) and latest valid FX preset.
   - If FX preset is absent, synthesize default neutral FX config.

### Fault Injection Verification
All failure points verified across automated test suites:
- `gate_b_transaction_test`: Basic 2-phase coordinator rollback across 10 injection points.
- `gate_j_legacy_migration_test`: 10 real legacy scenarios across all 8 fault injection points with reboot verification.
- `gate_k_commit_semantics_test`: Strict pre-commit vs post-commit vs indeterminate boundary matrix with reboot and patch switch protection.
