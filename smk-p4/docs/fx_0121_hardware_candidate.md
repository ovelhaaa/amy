# Physical Candidate Qualification & Build Hashes
## SMK-P4 0.12.1

### Target Platform
- **Hardware**: Guition ESP32-P4 DevKit (ESP32-P4 ES / pre-rev.300 generic).
- **CPU**: Dual-core RISC-V @ 360 MHz.
- **Display**: 800 × 480 7-inch MIPI-DSI LCD with capacitive touchscreen (GT911).
- **DAC / Audio**: PCM5102A over I²S (48 kHz, stereo, 256 frames DMA block).
- **MIDI**: USB Type-A Host (M-VAVE SMK25 V2 controller).
- **RAM**: 320 KB Internal SRAM + 16 MB external PSRAM (DIRAM).
- **Flash**: 16 MB SPI Flash.

### Binary Targets & Hashes

#### 1. Production Firmware: `guition-editor-8`
- **Configuration**: 8 voices polyphony, 64 oscillators, Sound Editor, Factory Library (687 patches), Native Stereo FX Suite, Touch UI Blueprint 1.1, USB MIDI Host, I²S PCM5102A.
- **Binary Path**: `smk-p4/.pio/build/guition-editor-8/firmware.bin`
- **File Size**: 1,223,776 bytes (~1.17 MB)
- **Flash Consumption**: 7.3% of 16 MB (1,223,042 / 16,777,216 bytes)
- **RAM Consumption**: 12.2% of 320 KB (39,828 / 327,680 bytes)
- **SHA-256**:
  ```
  5271035298494895626C1C88764967635126DC33650EEC9E9CDD324DFB823A98
  ```

#### 2. Benchmark Firmware: `guition-editor-8-bench`
- **Configuration**: Production build with automated DSP / render cycle diagnostic counters and performance benchmark instrumentation.
- **Binary Path**: `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- **File Size**: 1,324,400 bytes (~1.26 MB)
- **Flash Consumption**: 7.9% of 16 MB (1,323,686 / 16,777,216 bytes)
- **RAM Consumption**: 12.3% of 320 KB (40,216 / 327,680 bytes)
- **SHA-256**:
  ```
  96A009E33F8AE37ED0D226956AC9FAACB6774B3FB5F63F5371C051DE866DDDB4
  ```

#### 3. System Images
- **Bootloader**: `smk-p4/.pio/build/guition-editor-8/bootloader.bin`
  - SHA-256: `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90`
- **Partitions**: `smk-p4/.pio/build/guition-editor-8/partitions.bin`
  - SHA-256: `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501`

### Rollback Plan
- **Baseline Hardware-Approved Commit**: `27a87865cf2e6b939e2cf5a5884e7e61046183bf` (SMK-P4 0.12)
- In the event of any hardware regression, the operator can immediately re-flash the 0.12 release image:
  ```powershell
  git checkout 27a87865cf2e6b939e2cf5a5884e7e61046183bf
  pio run -d smk-p4 -e guition-editor-8-bench -t upload
  ```

### Physical Verification Instructions (Gate H / Smoke Test)
*Notice: Under project rules, the automated agent does NOT flash or upload to physical hardware.*

To flash the binary manually via PlatformIO once operator authorization is granted:
```powershell
# Flash benchmark candidate for diagnostic observation
pio run -d smk-p4 -e guition-editor-8-bench -t upload

# Monitor diagnostic output
pio device monitor -b 115200
```

#### Verification Steps for Operator:
1. **Boot**: Confirm boot log displays `NvsFxPresets::begin() SUCCESS`, AMY initialized at 48 kHz, 8 voices allocated in PSRAM, and UI Blueprint loaded.
2. **Preset Save As**:
   - Touch EDIT screen, navigate to FX tab.
   - Adjust Chorus to Juno II, Delay to 350 ms, Reverb to 40%.
   - Touch SAVE AS, select Slot 1, enter name "TEST 0121".
   - Confirm screen displays `SAVED`.
3. **Power Cycle**:
   - Turn off power to the Guition ESP32-P4 board.
   - Restore power.
   - Select Slot 1 and touch RECALL.
   - Confirm patch parameters, chorus, delay, and reverb settings match the saved preset exactly.
4. **Panic / Disconnect**:
   - Unplug USB MIDI controller during active chords.
   - Confirm Panic triggers instantly without hung notes or stuck reverb feedback.
