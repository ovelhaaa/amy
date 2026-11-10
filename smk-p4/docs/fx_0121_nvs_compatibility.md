# NVS Backward Compatibility and Partition Budget
## SMK-P4 0.12.1

### Partition Layout & Namespaces
The SMK-P4 firmware uses ESP-IDF NVS Flash for persistent storage without modifying partition tables:

| Namespace | Version Introduced | Contents | Record Size | Banks per Slot | Key Format |
|---|---|---|---|---|---|
| `sound011` | 0.11 | Synthesis Presets (V1 & V2) | 128 bytes | Bank A, Bank B | `s%02ua`, `s%02ub` |
| `sound012fx` | 0.12 | Audio FX Presets | 32 bytes | Bank A, Bank B | `f%02ua`, `f%02ub` |
| `sound012fx` | 0.12.1 | Transaction Commit Markers | 32 bytes | Bank A, Bank B | `c%02ua`, `c%02ub` |

### Entry Budget Calculation
In ESP-IDF NVS, memory is allocated in 32-byte entries:
- Standard NVS Page: 4096 bytes (126 entries per page available for data/headers).
- `sound011` Preset: 128 bytes payload + header = ~5 to 8 entries.
- `sound012fx` Preset: 32 bytes payload + header = ~2 entries.
- `sound012fx` Commit Marker: 32 bytes payload + header = ~2 entries.

For 16 preset slots using dual-bank ping-pong:
- FX presets: $16 \text{ slots} \times 2 \text{ banks} \times 2 \text{ entries} = 64 \text{ entries}$.
- Commit markers: $16 \text{ slots} \times 2 \text{ banks} \times 2 \text{ entries} = 64 \text{ entries}$.
- Total `sound012fx` usage: $128 \text{ entries}$ (~1 page).

The NVS partition provides over 700 free entries in production, leaving ample margin above the garbage-collection threshold (`gc_entries = 126`).

### Budget Guarding
Before performing any write operation, `NvsFxPresets::checkNvsBudget(4)` queries `nvs_get_stats`. If `stats.free_entries < 130`, the write is aborted cleanly with `ESP_ERR_NVS_NOT_ENOUGH_SPACE` before modifying Flash.

### Backward Compatibility Guarantees
1. **Preserving 0.11 V1/V2 Presets**:
   - `sound011` records are read and written using existing codecs (`user_preset_nvs.h` and `user_preset_nvs_v2.h`).
   - If an existing 0.11 preset has no corresponding commit marker in `sound012fx`, the coordinator detects this and loads the synthesis preset normally, assigning default neutral FX.
2. **Preserving 0.12 FX Presets**:
   - `sound012fx` records created during 0.12 are fully recognized and decoded by 0.12.1.
3. **Future Version Rejection**:
   - Any record with `version > 1` or mismatched `rom_compat` (`0x3e76efb6`) is rejected safely without crashes or buffer overflows.
