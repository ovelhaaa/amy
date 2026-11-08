# SMK-P4 0.11.1 — Gate F Formal Qualification: Versioned User Presets (`user_presets_v2.h`, Extended 64-Bit Mask Codec & NVS Admission)

- **Repository:** `ovelhaaa/amy`
- **Branch:** `feature/smk-p4-sound-editor-completion-0111`
- **Baseline:** `feature/smk-p4-live-sound-editor-011`
- **Software Qualification Status:** `PASS`
- **Physical Hardware Validation Status:** `MANUAL / PENDING HARDWARE`

---

## 1. Storage Architecture & Compatibility Guarantees

### 1.1 Record Formats and Namespaces
| Version | NVS Namespace | Record Size | Banks per Slot | Slots | Max NVS Entries (Dual-Bank) | Parameter Capacity |
|---|---|---|---|---|---|---|
| **V1 (`0.11`)** | `sound011` | `128 B` | `2` (`sXXa`, `sXXb`) | `16` | `32 * 5 = 160` entries | `17` fixed IDs (`0..16`) |
| **V2 (`0.11.1`)** | `sound011v2` | `160 B` | `2` (`vXXa`, `vXXb`) | `16` | `32 * 6 = 192` entries | Up to `57` IDs (`0..56`, 64-bit `Mask`) |

- **Partition Budget (`24 KiB` = `6` pages * `126` entries = `756` raw entries; conservative threshold `<= 390` entries):**
  - Retaining all `32` V1 records (`160` entries) + all `32` V2 records (`192` entries) + `2` namespace entries + `1` write buffer = **`358` entries (`<= 390`)**, Fitting comfortably inside the existing `24 KiB` NVS partition without creating a third NVS copy set or deleting V1 records.

### 1.2 V2 160-Byte Extended Payload Encoding (`smk-p4/components/amy_runtime/user_presets_v2.h`)
To support the 64-bit `edit::Mask` (`Count = 57`) within the existing 160-byte `sound011v2` record without breaking existing V2 records or future-version detection (`version == 2` at `b+4`):
- `b[0..3]`: Magic `0x31314553` (`"SE11"`)
- `b[4..7]`: Version `2` (records with `version > 2` abort `scanSlot` to protect future firmware data)
- `b[8..11]`: Monotonic `sequence` (`uint32_t`)
- `b[12..13]`: `patch` (`uint16_t`, `<= 255`)
- `b[14]`: `uint8_t(mask >> 56)` (`0` when high mask bits are unused)
- `b[15]`: Payload `mode`:
  - `mode = 0` (`popcount(mask) <= 20`): exact legacy V2 sparse `(uint8_t id, float value)` 5-byte pairs (`<= 100` bytes). Any V2 record with `<= 20` active parameters and `mask < 2^32` is **100% byte-for-byte identical** to original V2.
  - `mode = 1` (`21 <= popcount(mask) <= 25`, covering all `25` Juno + LFO + MOD parameters): mask-ordered 4-byte IEEE-754 `float` values (`<= 100` bytes, bit-exact).
  - `mode = 2` (`popcount(mask) > 25`, covering up to all `42` FM + LFO + MOD parameters): mask-ordered compact values (`4`-byte IEEE-754 `float` for `{Depth, OpRatio0..5, LfoRate}`, `1`-byte `uint8_t` for enum parameters `{LfoWave, LfoDest, ModEnable, ModSource, ModDest, ModPolarity}`, `2`-byte `1/16 ms` fixed-point for `{OpAttack0..5, OpDecay0..5}`, and `2`-byte `0.01%` fixed-point for percentage parameters; maximum `94` bytes `<= 100` bytes).
- `b[16]`: `family` (`0 = FM`, `1 = Subtractive`)
- `b[17..19]`: `uint32_t((mask >> 32) & 0xFFFFFFu)`
- `b[20..23]`: `uint32_t(mask & 0xFFFFFFFFu)`
- `b[24..48]`: Null-terminated ASCII preset `name` (`25` bytes)
- `b[49..51]`: `macros[0..2]` (`3` bytes)
- `b[52..151]`: `100`-byte parameter payload
- `b[152..155]`: `kRomCompatibility` (`0x3e76efb6`)
- `b[156..159]`: CRC32 over `b[0..155]`

---

## 2. Automated Qualification Evidence (`user_preset_nvs_v2_test.cpp`, `sound_editor_test.cpp`, `gate_e_test.cpp`)

| Test Case | Suite | Result |
|---|---|---|
| 16 slots dual-bank V2 save, load, and simulated reboot | `user_preset_nvs_v2_test` | `PASS` |
| Legacy V1 (`sound011`, 128B) read compatibility & non-destructive migration to V2 | `user_preset_nvs_v2_test`, `sound_editor_test` | `PASS` |
| Torn write / interrupted overwrite recovery (fallback to previous bank/V1) | `user_preset_nvs_v2_test`, `sound_editor_test` | `PASS` |
| CRC32 corruption rejection on both banks | `user_preset_nvs_v2_test`, `sound_editor_test` | `PASS` |
| Storage read error (`BankRead::Error`), stats failure, and commit failure handling | `user_preset_nvs_v2_test` | `PASS` |
| Conservative free-entry admission (`kRequiredNewSlotEntries = 13`) | `user_preset_nvs_v2_test` | `PASS` |
| Future format version (`version > 2`) survival without overwrite | `sound_editor_test` | `PASS` |
| Full 25-parameter Juno+LFO+MOD roundtrip in 160B V2 record | `gate_e_test` | `PASS` |
| Full 42-parameter FM+LFO+MOD roundtrip in 160B V2 record | `gate_e_test` | `PASS` |
| Quiet audio barrier (`storage_quiet`) before Flash/NVS write | `gate_a_test`, `gate_c_test`, `gate_d_test`, `gate_e_test` | `PASS` |

---

## 3. Physical Hardware Power-Cycle Validation Procedure (`MANUAL`)

Because host tests use `smk-p4/tests/nvs_mocks/fake_nvs.h` to simulate ESP-IDF NVS Flash pages, physical NVS qualification on the Guition ESP32-P4 requires executing the following procedure (see also Gate H script):
1. Boot `guition-editor-8` on ESP32-P4, load Patch `4` (`Moving Strings`), edit Juno ADSR (`Decay`, `Sustain`), Filter (`FilterEnv`, `FilterKeytrack`), LFO (`Rate`, `Depth`), and MOD Slot 1 (`Amount`), and save to Slot `01`.
2. Load Patch `135` (`DX7 PIANO 1`), edit `FmFeedback`, `OpAttack1`, `OpDecay1`, `OpVel1`, and save to Slot `02`.
3. Physically remove USB/5V power from the ESP32-P4 for 10 seconds, reconnect power, and recall Slots `01` and `02`, verifying all confirmed parameters and audio output match pre-power-cycle state.
