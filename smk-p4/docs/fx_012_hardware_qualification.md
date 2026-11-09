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

## 4. Physical Flash Telemetry & Hardware Report

- **Date / Time**: 2026-11-09 19:51:39
- **Serial Port**: COM13 (ESP32-P4 revision v1.3 @ 400 MHz, MAC: `80:f1:b2:d3:cc:86`)
- **Flashed Binary**: `smk-p4/.pio/build/guition-editor-8/firmware.bin` (SHA-256: `F7942C46C8A1BCC3255F7A50D04BF0589C3EA2A4E83EC9122959D95125B7E59C`)
- **Pre-Flash Backup**: Secured at `build/backups/app_pre_012.bin` (SHA-256: `ADAA4E21EC98D52A92C4C6DA56A87FA2DA4B2B86C430445D1FFBA1A54AB3F5F3`)
- **esptool Result**:
  ```text
  Auto-detected flash size: 16MB
  Wrote 23120 bytes at 0x00002000 in 0.3s (bootloader)
  Wrote 3072 bytes at 0x00008000 in 0.1s (partitions)
  Wrote 1213728 bytes at 0x00010000 in 6.9s (firmware)
  Hash of data verified. Hard resetting via RTS pin. SUCCESS.
  ```
- **Live Serial Boot Telemetry**:
  ```text
  P4_AMY: ADMISSION free=383011 largest=245760 required=376832 admitted=1
  P4_AMY: AUDIO RAM_ONLY blocks_rendered=1064 render_avg_us=296 render_max_us=371 render_deadlines=0
  P4_AMY: ENGINE ready=1 patch=128 notes=0 peak=0 clipped=0 oom=0 compute_deadlines=0
  P4_AMY: EDIT011 patch=128 epoch=1 revision=1 available=e401fff0 errors=0
  SMK_P4_GATE0: heap internal free=176175 min=176175 largest=102400; PSRAM free=30493192
  DISPLAY_TEST: LVGL free=95432 touch_errors=0 flush_errors=0 refresh=895
  ```
- **Operational Verdict**: Hardware initialized normally, zero deadline misses, render headroom > 93% (371 µs max vs 5,333 µs budget), internal RAM and PSRAM headroom intact. Ready for physical smoke interaction by operator.


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
