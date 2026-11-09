# Native AMY FX Suite (0.12) - Hardware Candidate Manifest & Qualification

## 1. Candidate Firmware Binaries

| Target Environment | Binary Path | Size (Bytes) | SHA-256 Hash |
|---|---|---|---|
| **Production Editor** (`guition-editor-8`) | `smk-p4/.pio/build/guition-editor-8/firmware.bin` | 1,213,728 | `F7942C46C8A1BCC3255F7A50D04BF0589C3EA2A4E83EC9122959D95125B7E59C` |
| **Benchmark & Diagnostics** (`guition-editor-8-bench`) | `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin` | 1,314,336 | `873F4FE93376D361DF1EBECE8032EC57EC2995636609D4DD964FE631F0E0564A` |
| **Bootloader** | `smk-p4/.pio/build/guition-editor-8/bootloader.bin` | 31,568 | `8E2E5EC7014809596E19F40989DC7B6F818A87311EFE56352775E42604423E65` |
| **Partition Table** | `smk-p4/.pio/build/guition-editor-8/partitions.bin` | 3,072 | `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501` |

---

## 2. Resource Utilization (PlatformIO ESP-IDF 5.5.2)

- **Target Chip**: ESP32-P4 RISC-V Dual-Core @ 360 MHz
- **Internal RAM Usage**: 12.1% (39,720 bytes used of 327,680 bytes)
- **Flash Memory Usage**: 7.8% (1,313,622 bytes used of 16,777,216 bytes)
- **PSRAM Allocation**:
  - Echo delay line: 65,536 samples (131,072 bytes) preallocated at boot.
  - Reverb comb/allpass delay buffers: ~160 KB preallocated at boot.
  - Frame buffers & LVGL 9.3 display pool.

---

## 3. Physical Smoke Test Procedure (Guition ESP32-P4 Hardware)

> [!IMPORTANT]
> **Operator Authorization Rule**: In strict compliance with project governance, firmware must NOT be flashed to the physical Guition hardware without explicit operator authorization for a designated binary path and matching SHA-256 hash.

### Smoke Checklist for Operator:
1. **Hardware Setup**:
   - Guition ESP32-P4 (800×480 display).
   - PCM5102A DAC connected over I²S to stereo headphones or monitor.
   - M-VAVE SMK25 V2 connected directly to ESP32-P4 native USB host port.
2. **Boot & Audio Neutrality**:
   - Power on device. Verify display initializes cleanly into patch 128 / patch 4 without audio pops or clicks.
   - Play notes on SMK25 V2: confirm 8 polyphonic voices, velocity response, sustain, and pitch bend.
   - Confirm dry unedited factory patches sound bit-identical to 0.11.1.
3. **Effects Touchscreen Navigation**:
   - Touch the "EFFECTS" tab on the bottom bar.
   - Verify top banner: `"NATIVE AMY FX CHAIN: EQ -> CHORUS -> DELAY -> REVERB  |  HARD REAL-TIME BUS 0"`.
   - Verify `BUS DRIVE`, `FX CHAIN`, and `BUS FX SEND` cards display `PLAN` status badge.
   - Verify the 8 active cards show numerical sliders/knobs with live parameter feedback.
4. **Interactive FX Qualification**:
   - **Chorus**: Change mode from OFF to JUNO I, then JUNO II, then ENSEMBLE. Confirm immediate stereo widening and animated spatial chorus texture. Test DEPTH slider.
   - **Echo / Delay**: Raise MIX to 40% and adjust TIME from 200 ms to 500 ms. Confirm discrete rhythmic repetitions. Set FEEDBACK to 80%; confirm stable repeats without volume runaway.
   - **Reverb**: Raise REVERB MIX to 35% and REVERB SIZE to 80%. Release keyboard keys; confirm natural reverb tail continues smoothly after Note-Off without abrupt cut.
   - **Tilt Tone**: Move TILT TONE slider between -60% (Dark) and +60% (Bright); confirm smooth frequency contouring.
5. **User Preset Persistence with FX**:
   - On an edited patch with active chorus and reverb, tap `USER / SAVE AS` and save to Slot 0 (`MY AMY FX`).
   - Switch to factory patch 135 (DX7 Piano, default neutral dry).
   - Open User Presets modal and recall Slot 0 (`MY AMY FX`).
   - Confirm both synthesis parameters (cutoff/envelopes) AND chorus/reverb settings restore simultaneously.
6. **Panic & Reconnect Resilience**:
   - Disconnect the USB cable of the SMK25 V2 during note play. Verify instant Panic: all audio voices terminate with zero stuck notes.
   - Reconnect the USB cable. Verify USB MIDI enumerates and plays normally without requiring a board reboot.
