# Native AMY FX Suite (0.12) - Safe FX Preset Persistence

## 1. Overview

Gate H implements safe non-volatile persistence for the Native AMY FX Suite on the Guition ESP32-P4.
When user presets (slots 0..15) are saved or recalled through the sound editor, their active FX configuration is persisted and restored atomically alongside synthesis parameters without compromising real-time audio stability.

---

## 2. Storage Architecture

FX preset persistence is implemented in `smk-p4/main/user_preset_fx_nvs.h` (`smk::p4::fx::NvsFxPresets`), conforming to the `smk::p4::fx::FxPresetStore` contract:

- **NVS Namespace**: `sound012fx` (independent from synthesis namespaces `sound011` and `sound011v2`).
- **Ping-Pong Dual-Bank Topology**:
  - Each of the 16 user preset slots uses two alternating storage keys: Bank A (`f%02ua`) and Bank B (`f%02ub`).
  - Total resident keys: 32 blobs.
  - Footprint: 32 bytes per blob (~2 NVS entries each, requiring ~64 entries total across all 16 slots).
- **Ping-Pong Selection Logic**:
  1. Scan Bank A and Bank B for valid checksum and headers.
  2. The bank with the higher sequence is considered active.
  3. Writes target the invalid/missing bank, or the bank with the older sequence number.
  4. After write and commit, the record is read back and verified byte-for-byte before confirming success.

---

## 3. Record Layout (32 Bytes Fixed)

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic (0x32315846)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Version (1)          |           Sequence            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Chorus Mode  |  Chorus Depth |         Delay Time ms         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Delay Fdbk   |   Delay Mix   |  Reverb Size  |  Reverb Mix   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Master Tone  |   EQ Low dB   |   EQ Mid dB   |  EQ High dB   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Bypass All   |                  Reserved                     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   ROM Compatibility (0x3e76efb6)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     CRC-32 (Bytes 0..27)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

`static_assert(sizeof(Record) == 32)` guarantees compile-time packing without compiler-dependent padding.

---

## 4. Real-Time & Audio Safety

- **Task Isolation**: Flash and NVS operations are strictly confined to the Control/UI task. Never invoked from the audio render callback or I²S DMA refill path.
- **Quiet Barrier Protocol**: The existing quiet barrier (`commands_.storage_requested` / `snapshot_.storage_quiet`) synchronizes storage operations during quiescent audio frames.
- **Zero Heap Allocations**: All buffers are stack-allocated or statically sized.
- **Torn-Write Resilience**: If power is cut mid-write, the incomplete write fails CRC validation and the engine automatically rolls back to the previous intact bank.
- **NVS Partition Protection**: Checks `stats.free_entries` against the garbage collection margin (126 entries) before attempting writes.

---

## 5. Backward Compatibility & Fallback

- **Legacy Presets (v1 / v2)**: When recalling a user preset created prior to 0.12 that has no record in `sound012fx`, the backend detects the missing FX record and automatically applies neutral default parameters (`FxConfig{}`): Chorus Off, Delay Mix 0%, Reverb Mix 0%, Tilt Tone 0 dB.
- **0.12 Presets**: Both synthesis and FX settings are saved and restored in lockstep.
- **Compare & Revert**: When a preset is recalled, `original_fx_` is updated to match the recalled preset's FX settings, allowing seamless Compare/Revert during subsequent editing.

---

## 6. Verification

The test suite `smk-p4/tests/fx_preset_persistence_test.cpp` was executed and passed all verification checks:

1. **Dual-Bank Ping-Pong**: Alternating writes between `f00a` and `f00b` with incrementing sequence IDs.
2. **Reboot Persistence**: Independent store re-initialization and successful state restoration.
3. **CRC Corruption & Rollback**: Single-byte payload and CRC corruption triggers immediate rollback to the alternate bank.
4. **Torn Writes**: Truncated writes fail cleanly without corrupting the active bank.
5. **NVS Space Limits**: Out-of-space conditions reject writes safely without altering existing records.
6. **16-Slot Saturation**: All 16 slots populated and verified independently.
7. **End-to-End Pipeline**: `LocalAmyPerformanceBackend` save and recall cycle confirms synchronous restoration of synthesis and FX parameters in the AMY runtime.
8. **Legacy Fallback**: Recall of synthesis-only preset defaults FX to dry neutral state with zero audio glitches.
