# NVS Backward Compatibility and Partition Budget
## SMK-P4 0.12.1

### Partition Layout & Namespaces
The SMK-P4 firmware uses ESP-IDF NVS Flash for persistent storage without modifying partition tables:

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| `nvs` | `0x9000` | `0x6000` (24 KiB) | Presets, commit markers, physical calibration |

| Namespace | Version Introduced | Contents | Record Size | Banks per Slot | Key Format |
|---|---|---|---|---|---|
| `sound011` | 0.11 | Synthesis Presets (V1 & V2) | 64 B (V1) / 128 B (V2) | Bank A, Bank B | `s%02ua`, `s%02ub` |
| `sound012fx` | 0.12 | Audio FX Presets | 32 bytes | Bank A, Bank B | `f%02ua`, `f%02ub` |
| `sound012fx` | 0.12.1 | Transaction Commit Markers | 32 bytes | Bank A, Bank B | `c%02ua`, `c%02ub` |

### Entry Budget Calculation & Capacity Audit (Gate F)
In ESP-IDF NVS, memory is allocated in 32-byte entries:
- Standard NVS Page: 4096 bytes.
  - Page header: 32 bytes (1 entry).
  - Entry state bitmap: 32 bytes (1 entry).
  - Usable data/blob entries: 126 entries of 32 bytes each ($126 \times 32 = 4032$ bytes).
- Total Partition Size: 24 KiB = 6 physical flash sectors (pages).
- Reserved for Garbage Collection: 1 sector (126 entries).
- Total Usable Capacity: $5 \times 126 = 630$ usable entries (or 756 gross entries).

#### Full Saturation Budget (All 16 Slots Dual-Bank Committed):
1. **Synthesis V2 Blobs (`sound011`)**:
   - 128 bytes payload + 1 blob header entry = 5 entries per bank.
   - 16 slots $\times$ 2 banks = 32 blobs = $32 \times 5 = 160$ entries.
2. **Audio FX Blobs (`sound012fx`)**:
   - 32 bytes payload + 1 blob header entry = 2 entries per bank.
   - 16 slots $\times$ 2 banks = 32 blobs = $32 \times 2 = 64$ entries.
3. **Commit Markers (`sound012fx`)**:
   - 32 bytes payload + 1 blob header entry = 2 entries per bank.
   - 16 slots $\times$ 2 banks = 32 blobs = $32 \times 2 = 64$ entries.

$$\text{Total Used Entries (Full Dual-Bank Saturation)} = 160 + 64 + 64 = 288 \text{ entries}$$

$$\text{Remaining Free Entries} = 630 - 288 = 342 \text{ entries (over 2.7 full pages free)}$$

### Fail-Closed Budget Guarding (Gate D)
Before any write operation, `NvsFxPresets::checkNvsBudget(4)` queries `nvs_get_stats()`:
1. **Hardware / Read Error Guard**: If `nvs_get_stats()` returns any error (`error_ != ESP_OK`), the check strictly **fails closed**, returning `ESP_ERR_NVS_NOT_ENOUGH_SPACE` immediately before modifying flash.
2. **Capacity Guard**: If `stats.free_entries < 130` (guaranteeing at least 1 full sector safety margin above GC threshold), the check strictly fails closed.

### Backward Compatibility Guarantees
1. **Preserving 0.11 V1/V2 Presets (Gate A)**:
   - `sound011` records are read and written using existing codecs (`user_preset_nvs.h` and `user_preset_nvs_v2.h`).
   - On the first transaction over legacy presets, `PresetTransactionCoordinator::inspectLegacySlot()` identifies whether legacy presets exist and writes a baseline commit marker pointing to the active legacy revision before candidate writes, guaranteeing clean rollback.
2. **Preserving 0.12 FX Presets**:
   - `sound012fx` records created during 0.12 are fully recognized and decoded by 0.12.1.
3. **Future Version Rejection**:
   - Any record with `version > 1` or mismatched `rom_compat` (`0x3e76efb6`) is rejected safely without crashes or buffer overflows.
