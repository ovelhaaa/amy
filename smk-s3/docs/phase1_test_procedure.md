# SMK-S3 Synth: Phase 1 Hardware Test Procedure

This document outlines the procedure to verify the Phase 1 firmware implementation on the hardware. 

## 1. Prerequisites
- **Hardware**: 
  - ESP32-S3 DevKitC N16R8.
  - PCM5102A I²S DAC module.
  - M-VAVE SMK25 V2 keyboard.
  - Headphones or speakers with a 3.5mm line-in, connected to the PCM5102A.
  - USB OTG cable / adapter to connect the SMK25 V2 to the ESP32-S3.
- **Software**: 
  - The Phase 1 firmware built and flashed to the ESP32-S3.
  - Serial monitor (e.g., `idf.py monitor`) set to 115200 baud.

## 2. Hardware Connections
Please ensure your connections match the configurable placeholders in `app_config.h` (or update `app_config.h` before building):
- **I²S BCLK**: GPIO 15
- **I²S LRCLK (WS)**: GPIO 16
- **I²S DATA (DIN)**: GPIO 17
- **USB Host**:
  - **D-**: GPIO 19
  - **D+**: GPIO 20
- **Power**: 
  - Supply 5V to the ESP32-S3 and the PCM5102A module.
  - Provide a stable 5V to the USB port for the SMK25 V2 (via VBUS or an external hub if necessary, since the S3 might struggle to power the keyboard directly via its 3.3V LDO if wired incorrectly).

## 3. Test Steps

### Test 1: Boot & Initialization
1. Power on the ESP32-S3 and open the serial monitor.
2. **Expected Observation**: 
   - You should see the project header (`SMK-S3 Synth`), version, and system information (flash size, PSRAM size, etc.).
   - You should see "Initialization complete" without any `[E]` (Error) logs regarding I²S, AMY, or USB Host failures.

### Test 2: Audio & AMY Engine Initialization
1. Connect headphones/speakers to the PCM5102A.
2. **Expected Observation**: 
   - No loud pops or crackles should occur during boot (due to the zero-fill initialization logic in the audio output).
   - A soft hiss may be audible, indicating the DAC is receiving a clock signal.

### Test 3: USB MIDI Host Enumeration
1. Connect the M-VAVE SMK25 V2 via the USB port.
2. **Expected Observation**:
   - The serial console should log the detection of a new USB device.
   - It should log the Vendor ID (VID) and Product ID (PID) of the SMK25 V2.
   - It should report that the MIDI streaming interface was found and successfully claimed.

### Test 4: Note Playback
1. Press various keys on the SMK25 V2.
2. **Expected Observation**:
   - Audio should play immediately (default patch is Juno-6).
   - Polyphony should work (try playing chords).
   - Audio should be clean (no stuttering, underruns, or artifacts).
   - Releasing keys should stop the sound (Note Off handling).

### Test 5: Velocity & Controls
1. Strike the keys with varying force.
   - **Expected Observation**: The volume and timbre (depending on the patch) should change based on velocity.
2. Use the Pitch Bend touch strip.
   - **Expected Observation**: The pitch of held notes should smoothly bend up and down, returning to the center when released.
3. Use the Modulation touch strip.
   - **Expected Observation**: Modulation (usually vibrato or filter cutoff) should be applied to the sound.

### Test 6: Hot-Plug & Panic
1. While holding a chord on the keyboard, disconnect the USB cable.
2. **Expected Observation**:
   - The audio must instantly silence (the Panic mechanism should trigger on disconnect).
   - The serial console should log a disconnect event and the Panic trigger.
3. Reconnect the USB cable.
   - **Expected Observation**: The device should re-enumerate and you should be able to play notes again without rebooting the ESP32.

### Test 7: Diagnostics Monitoring
1. Let the system idle, or play continuously for 10-20 seconds.
2. Observe the periodic (5-second) status updates in the serial monitor.
3. **Expected Observation**:
   - `Underruns` should remain at `0`.
   - `Audio Load` should be a stable decimal fraction (`< 1.0`), well below `0.95`.

## 4. Reporting Issues
If any test fails, note the step number, the serial console output at the time of failure, and the observable hardware behavior. This will guide the debugging phase.
