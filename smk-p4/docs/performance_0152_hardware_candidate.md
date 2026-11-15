# SMK-P4 0.15.2 — Hardware Candidate & Flashing Guide

## 1. Candidate Images

The following candidate binaries were built on Windows using PlatformIO with the Espressif ESP32-P4 platform:

### Profile 1: `guition-editor-8` (Standard Release Candidate)
- **Binary Path:** `smk-p4/.pio/build/guition-editor-8/firmware.bin`
- **Binary Size:** 1,333,616 bytes
- **Flash Utilization:** 1,332,884 B / 16,777,216 B (7.9%)
- **Internal RAM Utilization:** 42,128 B / 327,680 B (12.9%)
- **SHA-256:**
  ```text
  89CDF844C3B1939299C776078D72710795B50FA927504C620C8B24F3C8EEC00C
  ```

### Profile 2: `guition-editor-8-bench` (Hardware Bench Candidate with Physical Pins)
- **Binary Path:** `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- **Binary Size:** 1,435,536 bytes
- **Flash Utilization:** 1,434,820 B / 16,777,216 B (8.6%)
- **Internal RAM Utilization:** 42,512 B / 327,680 B (13.0%)
- **SHA-256:**
  ```text
  F90DF8D9D840DE42B6B98D9016CC6EFB39044B2319B1E810DB7D79CE4713CCB3
  ```

---

## 2. Flashing & Verification Log · 2026-11-15

- **Authorization:** Explicit user authorization received for `guition-editor-8-bench` with SHA-256 `F90DF8D9D840DE42B6B98D9016CC6EFB39044B2319B1E810DB7D79CE4713CCB3`.
- **Target Device:** Guition JC4880P433 (ESP32-P4 revision v1.3) on `COM13`.
- **Pre-Flash Backup:** Full 16 MiB flash backup captured to `build/backups/p4-0152-com13-current/flash-before.bin` in 133.7 seconds.
- **Flashing:** Wrote 1,435,536 bytes (784,762 compressed) at `0x00010000` via esptool v5.1.0 in 8.0 seconds (1433.8 kbit/s).
- **Readback Verification:** Read 1,435,536 bytes from `0x00010000` to `flash-after-app.bin`. SHA-256 confirmed byte-identical:
  ```text
  F90DF8D9D840DE42B6B98D9016CC6EFB39044B2319B1E810DB7D79CE4713CCB3 (PASS)
  ```
- **Live Boot & Telemetry Observation:**
  - **RAM Admission:** 357,763 B internal RAM free (5,507 B margin above 352,256 B required threshold).
  - **Audio Owner Stack:** `stack_free_bytes = 3584 B` (recovered from 932 B in 0.15.1, an increase of 2,652 B free margin).
  - **I2S Audio Output:** `AUDIO I2S_PRODUCING frames_written=1981952 blocks_rendered=7745 render_avg_us=239 render_max_us=522 render_deadlines=0 write_errors=0 timeouts=0`.
  - **Display / LVGL:** `LVGL free=88512 largest=88152 peak_used=38440 fragmentation=1%`.
  - **USB Host:** `USB state=1 connects=0 disconnects=0 errors=0 parse_errors=0`.

---

## 3. Physical Verification Checklist on Guition JC4880P433

1. **Boot & Telemetry:**
   - Display shows Guition 800×480 UI with status strip.
   - Owner stack free margin >= 4,000 B reported in diagnostics.
   - Internal RAM admission free >= 352,256 B.
2. **Delay Transitions:**
   - Tap tempo / division changes during active delay playback maintain continuous echo tails without audible clicks or runaway feedback.
3. **Performance Lock in Patch Browser:**
   - Open Patch Browser and tap a patch: popup opens showing 4 checkboxes (Keep FX, Keep Harmony, Keep Arpeggiator, Keep Macros).
   - Tapping "LOAD WITH LOCKS" preserves active FX and Arp settings onto the new patch.
   - Tapping "LOAD PATCH" resets to default patch settings.
4. **Fast Stepping with PREV / NEXT:**
   - Stepping patches preserves session locks smoothly when clean.
   - If patch has unsaved edits, opens confirmation dialog with "DISCARD & LOAD" buttons.
