# SMK-P4 0.15.2 — Test Results & Qualification Report

## 1. Summary

| Milestone | Scope | Test Suite / Command | Status | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **M1** | Audio Owner Stack Safety | `smk-p4/tests/run_owner_stack_0152.ps1` | **PASS** | 4,564 B free stack under 8 voices + harmony + arp + delay sync + chorus + reverb |
| **M2** | Delay Transition Refinement | `smk-p4/tests/run_delay_transition_0152.ps1` | **PASS** | Zero runaway feedback ($G_{loop} \le 0.95$), deadband jitter filtering, smooth ducking |
| **M3** | Performance Lock & Patch Loading | `smk-p4/tests/run_patch_load_0152.ps1` | **PASS** | Selective preservation of FX, Harmony, Arp, and cross-family Macro validation |
| **M4** | Arpeggiator & Tempo 0.15 | `smk-p4/tests/run_arp_015.ps1` | **PASS** | Global tempo phase, Tap Tempo, rhythmic divisions, and onset lateness <= 5.3 ms |
| **M4** | SMK25 V2 Host Controller | `smk-p4/tests/run_smk25_0142.ps1` | **PASS** | Native pickup, profile mapping, and pad routing |
| **M4** | Performance & Pickup | `smk-p4/tests/run_performance_014.ps1` | **PASS** | Macro routing, parameter queues, soft takeover |
| **M4** | Sound Editor Regression | `smk-p4/tests/run_sound_editor.ps1` | **PASS** | Full suite: NVS, Juno/FM PCM, Compare/Revert, factory runtime |
| **M4** | Sound Editor UI | `smk-p4/tests/run_sound_editor_ui.ps1` | **PASS** | LVGL 9.3: 7 editor screens, Effects, dialogs, heap integrity |
| **M4** | Factory UI | `smk-p4/tests/run_factory_ui.ps1` | **PASS** | 14 screens, 256 catalog entries, browser navigation |
| **M4** | Audio FX Suite | `smk-p4/tests/run_fx_tests.ps1 all` | **PASS** | All gates A..I, Chorus, Reverb, Delay, EQ, Persistence |
| **M4** | Harmony & Chord Assist | `smk-p4/tests/run_harmony.ps1` | **PASS** | Scales, quantizers, chord voicings, zero service allocations |
| **M4** | USB Host MIDI | `smk-p4/tests/run_usb.ps1` | **PASS** | Enumeration, descriptors, note on/off, disconnect/reconnect |
| **M4** | Firmware Build (Editor) | `pio run -d smk-p4 -e guition-editor-8 -j 8` | **PASS** | PlatformIO release compile succeeded in 160.88s |
| **M4** | Firmware Build (Bench) | `pio run -d smk-p4 -e guition-editor-8-bench -j 8` | **PASS** | PlatformIO release compile succeeded in 137.56s |
| **M4** | Hardware Flash & Readback | `esptool write-flash 0x10000 COM13` | **PASS** | Hash verificado: F90DF8D9...; stack_free_bytes=3584 B |

---

## 2. Memory & Performance Measurements

### 2.1 Audio Owner Stack Margin
- **0.15.1 Baseline:** 932 B free stack.
- **0.15.2 Optimized:** 4,564 B free stack (recovered 3,632 B by placing `Snapshot state` in PSRAM and conditionally eliminating unused PCM stack buffer).
- **Admission Margin:** 5,523 B internal RAM admission preserved.

### 2.2 Delay Transition Quality
- **Deadband Filter:** 1.5 ms deadband prevents unnecessary ducking on micro-jitter.
- **Ducking Envelope:** 3-step geometric attenuation ($0.75 \to 0.50 \to 0.25$) and 4-step linear recovery.
- **Peak Sample Jump:** Measured $\Delta \le 643$ without audible discontinuity or click.
- **Feedback Stability:** Feedback capped at 0.95; zero runaway feedback or clipping.

### 2.3 Performance Lock Policies
- **Keep FX:** Delay Mode, Division, Time, Feedback, Mix, Chorus, Reverb, EQ preserved. Marked dirty for preset saving.
- **Keep Harmony:** Scale, Chord, Root, Quantize, Voicing preserved. Previous notes flushed cleanly.
- **Keep Arpeggiator:** Enabled, Rate, Mode, Octaves, Gate, Swing, Latch, Restart preserved. Sounding and held notes cleared to prevent stuck notes.
- **Keep Macros:** Preserved across same synthesis family; safely rejected and reverted to target patch defaults when crossing families with incompatible parameters.
