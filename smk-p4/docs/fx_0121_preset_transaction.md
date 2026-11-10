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

### Commit Protocol Flow
1. **Target Bank Selection**: Inspect slot scan for both synthesis and FX to identify non-active/older banks in both namespaces (`target_synth_bank`, `target_fx_bank`).
2. **Phase 1A: Write Candidate FX**: Write FX configuration blob to `target_fx_bank` with incremented sequence number. Readback verify.
3. **Phase 1B: Write Candidate Synth**: Write synthesis preset blob to `target_synth_bank` with incremented sequence number. Readback verify.
4. **Phase 2: Commit Marker**: Write `CommitMarker` binding `synth_seq` and `fx_seq` to marker ping-pong bank (`c<slot>a` or `c<slot>b`). Readback verify.
5. **Success Confirmation**: Only when Phase 2 succeeds is `SaveResult::Status::Success` reported to the caller.

### Recovery Protocol
On recall or reboot:
1. Load candidate commit markers from banks A and B.
2. Select marker with valid CRC, matching magic, version, and highest monotonic sequence number (`seqAfter16`).
3. If marker is valid:
   - Read synthesis bank specified by marker (`synth_bank`). Verify sequence matches `marker.synth_seq`.
   - Read FX bank specified by marker (`fx_bank`). Verify sequence matches `marker.fx_seq`.
   - If both match and are intact: return committed pair.
4. If marker is missing or invalid (e.g. legacy 0.11 or 0.12 preset before coordinator was used):
   - Gracefully fallback to latest valid synthesis preset and latest valid FX preset.
   - If FX preset is absent, synthesize default neutral FX config.
5. If an interrupted write corrupted the candidate banks during Phase 1 or Phase 2:
   - The coordinator selects the previous valid marker, rolling back cleanly to the prior committed revision.

### Fault Injection Verification
All 10 failure points are verified under automated testing (`smk-p4/tests/gate_b_transaction_test.cpp`):
1. `BeforeFxWrite`: Clean rollback to previous pair.
2. `DuringFxWrite`: Clean rollback to previous pair.
3. `AfterFxWrite`: Clean rollback to previous pair.
4. `BeforeSynthWrite`: Clean rollback to previous pair.
5. `DuringSynthWrite`: Clean rollback to previous pair.
6. `AfterSynthWrite`: Clean rollback to previous pair.
7. `BeforeMarkerWrite`: Clean rollback to previous pair.
8. `DuringMarkerWrite`: Clean rollback to previous pair.
9. `AfterMarkerWrite`: Clean rollback to previous pair.
10. `DuringReadback`: Clean rollback to previous pair.
