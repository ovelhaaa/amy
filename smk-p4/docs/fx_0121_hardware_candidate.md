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
- **File Size**: 1,222,288 bytes (~1.17 MB)
- **Flash Consumption**: 7.3% of 16 MB
- **RAM Consumption**: 12.2% of 320 KB
- **SHA-256**:
  ```
  62C19A06756E6F39D65030285A4BF7E40E61164BAE43C13FA6A80D0B037DC522
  ```

#### 2. Benchmark Firmware: `guition-editor-8-bench`
- **Configuration**: Production build with automated DSP / render cycle diagnostic counters and performance benchmark instrumentation.
- **Binary Path**: `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin`
- **File Size**: 1,322,912 bytes (~1.26 MB)
- **Flash Consumption**: 7.9% of 16 MB
- **RAM Consumption**: 12.1% of 320 KB
- **SHA-256**:
  ```
  6667A8F7FCB215CCE2795F78BFC306CA26395CC75B5EB97B3455C9AE3B65348C
  ```

### Physical Verification Instructions
*Notice: Under project rules, the automated agent does NOT flash or upload to physical hardware.*

To flash the binary manually via PlatformIO:
```powershell
# Flash production candidate
pio run -d smk-p4 -e guition-editor-8 -t upload

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
