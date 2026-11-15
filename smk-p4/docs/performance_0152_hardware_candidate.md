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

## 2. Flashing Authorization Protocol

Per repository rules:
> **Do not flash firmware without explicit user authorization tied to the exact binary SHA-256.**

Once the user approves flashing with the desired profile's SHA-256, the following command can be executed to flash the Guition board:

```powershell
# Example for guition-editor-8-bench (replace COMx with actual port):
python -m esptool --chip esp32p4 -p COMx -b 460800 write_flash 0x10000 smk-p4/.pio/build/guition-editor-8-bench/firmware.bin
```

Or via PlatformIO:
```powershell
& C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -t upload --upload-port COMx
```

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
