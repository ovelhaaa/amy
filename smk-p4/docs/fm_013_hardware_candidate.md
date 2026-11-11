# SMK-P4 0.13: Hardware Firmware Candidate & Validation Manifest

**Release:** SMK-P4 0.13  
**Target Hardware:** Guition ESP32-P4 DevKit (RISC-V dual-core @ 360 MHz, 320 KB internal SRAM, 16 MB SPI Flash, external PSRAM, 800×480 touchscreen, PCM5102A I²S DAC, native USB MIDI Host)  
**Branch:** `feature/smk-p4-fm-algorithm-selector-013`  
**Base Commit:** `85b68f206f9bdbce1b6d49bb3f08eccff3f123ed` (`feature/smk-p4-fx-state-hardening-0121`)

> [!CAUTION]
> **FLASHING AUTHORIZATION REQUIRED**  
> Coding agents are strictly forbidden from programming firmware to the physical Guition ESP32-P4 hardware without explicit, written operator authorization citing the exact binary SHA-256 hash.

---

## 1. Candidate Firmware Artifacts

### 1.1 Production Firmware: `guition-editor-8`
- **Purpose:** Full production standalone synthesizer (8 voices, 48 kHz, 256 frames, full 229 factory library, Sound Editor with FM Algorithm Selector, Stereo Chorus, Reverb, Delay, 3-Band EQ, Transactional User Presets v2, Harmony Assist, USB MIDI Host).
- **Binary Path:** `smk-p4/.pio/build/guition-editor-8/firmware.bin`
- **File Size:** `1,234,016 bytes`
- **Flash Utilization:** `1,233,290 / 16,777,216 bytes (7.4%)`
- **Internal RAM Utilization:** `39,896 / 327,680 bytes (12.2%)`
- **SHA-256 Digest:**
  ```text
  B33A8CF17F38A9752DAE06A91AB116BBA880893EB93C50222EA32F7F8464117F
  ```

### 1.2 Benchmark & Diagnostics Firmware: `guition-editor-8-bench`
- **Purpose:** Identical to production but with real-time test hooks, diagnostic instrumentation counters, and audio benchmark harness enabled.
- **Binary Path:** `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- **File Size:** `1,334,656 bytes`
- **Flash Utilization:** `1,333,940 / 16,777,216 bytes (8.0%)`
- **Internal RAM Utilization:** `39,736 / 327,680 bytes (12.1%)`
- **SHA-256 Digest:**
  ```text
  DC41479E5A2E33F7E3FAD436505D00080FB96B52A31A53A3C1F0E96B313FB1E5
  ```

### 1.3 Supporting ESP-IDF Bootloader & Partition Table
- **Bootloader (`bootloader.bin`):**
  - Path: `smk-p4/.pio/build/guition-editor-8/bootloader.bin`
  - File Size: `23,120 bytes`
  - SHA-256: `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90`
- **Partition Table (`partitions.bin`):**
  - Path: `smk-p4/.pio/build/guition-editor-8/partitions.bin`
  - File Size: `3,072 bytes`
  - SHA-256: `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501`

---

## 2. Flashing Procedure (Manual Operator Execution Only)

When explicit authorization is granted by the user, the firmware can be flashed using PlatformIO or `esptool.py`:

### PlatformIO Method:
```bash
# Production image:
pio run -d smk-p4 -e guition-editor-8 -t upload

# Benchmark image:
pio run -d smk-p4 -e guition-editor-8-bench -t upload
```

### Direct `esptool.py` Command:
```bash
python -m esptool --chip esp32p4 -b 460800 --before default_reset --after hard_reset write_flash \
    0x10000 smk-p4/.pio/build/guition-editor-8/firmware.bin
```

---

## 3. Rollback Procedure

If any unexpected regression occurs on hardware, the operator can immediately revert to the prior stable release:
- **Prior Release:** `feature/smk-p4-fx-state-hardening-0121`
- **Prior Binary:** `smk-p4/.pio/build/guition-editor-8/firmware.bin` (from commit `85b68f206f9bdbce1b6d49bb3f08eccff3f123ed`)
- **Rollback Flash Command:**
  ```bash
  git checkout feature/smk-p4-fx-state-hardening-0121
  pio run -d smk-p4 -e guition-editor-8 -t upload
  ```

---

## 4. Hardware Verification Checklist (On Bench)

### 4.1 Candidate Binary Qualification Status
- **Candidate Flashing Target:** `guition-editor-8-bench` (`1,334,656 bytes`, SHA-256 `DC41479E5A2E33F7E3FAD436505D00080FB96B52A31A53A3C1F0E96B313FB1E5`)
- **Hardware Flashing Status:** `NOT RUN — MANUAL HARDWARE GATE` (Awaiting explicit operator authorization)
- **Baseline Prior Flash Reference (0.13 Early Pass):**
  - Prior image (`1,329,968 bytes`, SHA-256 `D1A4AEF87655927D500FFEC0B4813586C2FBE267C263A8E329F3077CA786E3AD`) was verified live on ESP32-P4 COM13 with 0 DMA underruns, `render_avg_us=214`, `render_max_us=351`, zero display flush errors, and active USB host.
  - The current corrected candidate image incorporates the real-time active note guard fix, owner-confirmed UI algorithm selector, real graphical LVGL operator wiring, and end-to-end NVS persistence recovery.

### 4.2 Manual Bench Test Procedure
1. **Power-On & Display Boot:**
   - Verify LVGL boots into the 800×480 panoramic UI without flicker.
   - Navigate to Sound Editor -> FM Tab.
2. **FM Algorithm Selector Control:**
   - Confirm Algorithm selector displays `ALGO xx / 32`.
   - Tap `<` and `>` buttons; verify value decrements and increments between 1 and 32.
   - Tap center badge; verify 32-algorithm grid modal opens. Tap algorithm tile (e.g. 5); verify modal closes and algorithm 5 is selected.
3. **Interactive Topology Diagram:**
   - Confirm 6-operator diagram renders carriers with cyan background (`kActive`) and modulators with amber outline (`kAccentPrimary`).
   - Confirm routing lines and feedback badges reflect selected topology.
   - Tap OP1..OP6 buttons in diagram; verify selected operator updates in editor.
4. **Audible Timbre Change:**
   - Play notes on M-VAVE SMK25 V2 keyboard.
   - Switch from Algorithm 1 (complex 2-carrier brass) to Algorithm 32 (pure 6-sine additive organ); verify striking audible transformation in real audio output.
5. **Safety During Performance:**
   - Hold a chord on the keyboard.
   - Tap `<` or `>` or an algorithm tile in the modal.
   - Verify notice banner displays `"RELEASE NOTES TO CHANGE FM ALGORITHM"`.
   - Verify no audible click, audio underrun, stuck note, or panic occurs.
   - Release chord; verify algorithm switch is now accepted.
6. **Compare & Revert:**
   - Edit algorithm and operator levels.
   - Tap `COMPARE`; verify sound returns to original patch algorithm.
   - Tap `REVERT`; verify patch algorithm and edits are restored to factory ROM defaults.
7. **User Presets v2 Persistence:**
   - Save custom patch with edited algorithm to Slot 01.
   - Reboot hardware; recall Slot 01.
   - Verify algorithm, operator levels, ratios, envelopes, and topology are restored identically.
