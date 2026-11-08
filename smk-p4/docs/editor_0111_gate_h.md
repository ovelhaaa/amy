# SMK-P4 0.11.1 — Gate H: Final Candidate, Target Qualification & Release Manifest

## 1. Objective
Gate H validates the complete `0.11.1` firmware candidate across host regression suites and ESP32-P4 target builds (`guition-editor-8` and `guition-editor-8-bench`), verifies effective `sdkconfig` build flags, documents binary SHA-256 hashes and resource usage, and provides the physical hardware qualification and rollback procedures.

---

## 2. Target Firmware Builds & Artifacts

### 2.1 Build Command
```powershell
C:\.platformio\penv\Scripts\pio.exe run -e guition-editor-8 -e guition-editor-8-bench
```

### 2.2 Candidate Binary Summary

| Environment | Binary Path | Size (Bytes) | Flash Usage | RAM Usage | SHA-256 | Status |
|---|---|---:|---:|---:|---|---|
| guition-editor-8 | .pio/build/guition-editor-8/firmware.bin | 1,206,688 | 7.2% (1,205,960 / 16,777,216 B) | 11.7% (38,308 / 327,680 B) | 7631D6F882E52827FC74A63E57CF9EF9684623B1B918E8E77D05564D1CB1FDF0 | PASS |
| guition-editor-8-bench | .pio/build/guition-editor-8-bench/firmware.bin | 1,307,664 | 7.8% (1,306,942 / 16,777,216 B) | 12.0% (39,248 / 327,680 B) | 2E0CA749E571A069BEF0999DE37ECF7A5486D196035EC5C50DA6C03653033CC2 | PASS |

---

## 3. Effective `sdkconfig` Verification

### 3.1 `guition-editor-8` (Default Safe Hardware Profile)
Verified in `.pio/build/guition-editor-8/config/sdkconfig.h` and `sdkconfig.guition-editor-8`:
- `CONFIG_SMKP4_UI_BLUEPRINT=y`
- `CONFIG_SMKP4_SOUND_LIBRARY=y`
- `CONFIG_SMKP4_FULL_FACTORY_LIBRARY=y`
- `CONFIG_SMKP4_SOUND_EDITOR=y`
- `CONFIG_SMKP4_HARMONY=y`
- `CONFIG_SMKP4_UNIFIED_ENGINE=y`
- `CONFIG_SMKP4_AMY_VOICES_8=y` (`CONFIG_SMKP4_AMY_VOICES=8`)
- `CONFIG_SMKP4_STANDALONE=y`
- `# CONFIG_SMKP4_PCM_WIRING_CONFIRMED is not set` (safe unconfirmed default)
- `# CONFIG_SMKP4_USB_WIRING_CONFIRMED is not set` (safe unconfirmed default)

### 3.2 `guition-editor-8-bench` (Confirmed Bench Profile)
Verified in `.pio/build/guition-editor-8-bench/config/sdkconfig.h` and `sdkconfig.guition-editor-8-bench`:
- Includes all `guition-editor-8` flags above, plus:
- `CONFIG_SMKP4_PCM_WIRING_CONFIRMED=y` (`BCLK=50`, `WS=51`, `DATA=52`)
- `CONFIG_SMKP4_USB_WIRING_CONFIRMED=y` (`CONFIG_SMKP4_USB_CONTROLLER=0`)
- `CONFIG_SMKP4_USB_DESCRIPTOR_LOG=y`
- `CONFIG_SMKP4_USB_MIDI_TRACE=y`

---

## 4. Software Regression Matrix

| Test Suite / Script | Scope | Result |
|---|---|---|
| `.\smk-p4\tests\run_sound_editor.ps1` | `user_presets_v2_test`, `sound_editor_test`, `gate_a_test`, `juno_filter_test`, `juno_components_test`, `gate_d_test`, `gate_e_test`, `factory_runtime_test` | `PASS` |
| `.\smk-p4\tests\run_sound_editor_ui.ps1` | `sound_editor_ui_test` (LVGL 9.3, 800×480, 7 tabs, Gate G0 `auditInventory`, owner-confirmed slider/pill/MOD/PRESETS roundtrip, heap integrity) | `PASS` |
| `.\smk-p4\ui-preview\run_preview.ps1` | `blueprint_test_suite.exe` (7 checks + 20 screen/tab PPM/PNG previews) | `PASS` |
| `.\smk-p4\tests\run_harmony.ps1` | Harmony engine exhaustive scales/keys/MIDI/quantizers, chords/degrees/inversions/range, 8-voice runtime integration | `PASS` |
| `.\smk-p4\tests\compare_editor_factory.ps1 -Baseline .` | Bit-exact PCM comparison between baseline (`SMKP4_SOUND_EDITOR=0`) and candidate (`SMKP4_SOUND_EDITOR=1` with zero overrides) across factory patches | `PASS` |

---

## 5. Rollback & Recovery Procedure

### 5.1 NVS Compatibility & Non-Destructive Coexistence
- **V1 Namespace (`sound011`)**: Never modified or erased by `0.11.1`. If the operator rolls back firmware to `0.11`, all original V1 presets remain intact in `sound011` with keys `s00a`..`s15b`.
- **V2 Namespace (`sound011v2`)**: Uses `160`-byte double-buffered blobs (`v00a`/`v00b` .. `v15a`/`v15b`, max `390` NVS entries <= `480` entry budget in the `24 KiB` partition).
- **Forward/Backward Safety between `0.11` and `0.11.1` builds**:
  - `0.11` firmware only opens and accesses `sound011`. The presence of `sound011v2` entries is completely ignored by `0.11`.
  - Older 32-bit V2 records (`version = 2`, `mode = 0`) and extended 64-bit V2 records (`version = 2`, `mode = 1` or `2`) validate with CRC-32 and preserve the dual-copy A/B generation commit model.

### 5.2 Firmware Flash & Verifiable Rollback Plan (`MANUAL` — requires explicit operator authorization)
**Baseline Artifact for Rollback:**
- Source worktree: `build/worktrees/smk-p4-live-sound-editor-011`
- Baseline binary path: `build/worktrees/smk-p4-live-sound-editor-011/smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- Baseline SHA-256: `0BD1FD6CE2F6C72679C17CECF9E96CA6E61CEFC8993349463E70701F5C3F2911`

**Step 1: Backup existing NVS partition (optional safety measure)**:
```powershell
esptool.py --chip esp32p4 -p COMx read_flash 0x9000 0x6000 nvs_backup_pre_0111.bin
```

**Step 2: Flash `0.11.1` candidate (only when operator explicitly authorizes)**:
```powershell
# Writes application partition at 0x10000; bootloader (0x0) and partition table (0x8000) remain intact:
esptool.py --chip esp32p4 -p COMx write_flash 0x10000 .pio/build/guition-editor-8-bench/firmware.bin
```

**Step 3: Verifiable Rollback to `0.11` Baseline**:
```powershell
# Restore application partition (0x10000) using verified 0.11 baseline binary:
esptool.py --chip esp32p4 -p COMx write_flash 0x10000 ../smk-p4-live-sound-editor-011/smk-p4/.pio/build/guition-editor-8-bench/firmware.bin

# (Optional) Restore NVS partition if pristine pre-test state is desired:
esptool.py --chip esp32p4 -p COMx write_flash 0x9000 nvs_backup_pre_0111.bin
```

---

## 6. Physical Hardware Qualification Plan (`MANUAL`)

Because physical flashing and bench execution were not performed in this software gate (`NO HARDWARE FLASH EXECUTED`), the following tests are marked `MANUAL` for bench sign-off on the Guition ESP32-P4 + PCM5102A + SMK25 V2:

1. **Physical NVS Power-Cycle Test (`MANUAL`)**:
   - Save user presets in Slots 1, 2, and 16 covering Juno (`patch 0`), DX7 (`patch 128` / `135` with OP1–OP6 edits and `fm.feedback`), LFO (`lfo.1.*`), and Mod Matrix (`mod.slot1.*`).
   - Power-cycle the Guition ESP32-P4 completely, boot `guition-editor-8-bench`, recall Slots 1, 2, and 16, and verify parameter values and audio output match pre-reboot state.
2. **Real-Time Audio & Latency Telemetry (`MANUAL`)**:
   - Play 8-voice chords on SMK25 V2 with Harmony Assist enabled while sweeping Juno filter (`cutoff_hz`, `env_amt`, `kbd_track`), DX7 `fm.feedback`, LFO depth/rate, and CC1 Mod Wheel.
   - Verify `underruns == 0`, `event_overflows == 0`, and `max_render_us < 5333 us` (256 frames @ 48 kHz).
3. **800×480 Touch Display Contextual Verification (`MANUAL`)**:
   - Step through all 7 Sound Editor tabs (`OSC`, `FM`, `FILTER`, `AMP`, `ENV`, `LFO`, `MOD`) and `PRESETS` modal on the physical panel; verify contextual `N/A` badges and live Compare/Revert behavior.
