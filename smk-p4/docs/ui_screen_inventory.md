# SMK-P4 UI Blueprint 1.0 — Complete 14-Screen Inventory

All 14 screens are implemented as real, interactive LVGL 9.3.0 `ScreenView` classes under `smk-p4/components/ui_blueprint/screens/`, constrained to the `800 × 480` horizontal frame (`StatusStrip` `y: 0..40`, `Content` `y: 40..434`, `TabBar` `y: 434..480`).

---

## Summary Table

| # | Screen ID | File | Primary / Secondary Access | Sub-Tabs / Modes | Scroll Policy |
|---|---|---|---|---|---|
| 01 | `Performance` | `performance_screen.cpp` | `TabBar` Primary (`PERF`) + `1-Tap PERF` | `COMPACT` / `DETAILED` stage toggle | Zero vertical scroll |
| 02 | `PatchBrowser` | `patch_browser_screen.cpp` | `TabBar` Secondary (`PATCHES`) + `PatchSelector` `BROWSER` | 4 Source Filters (`ALL`, `P4 AVAIL`, `S3/AMY PLAN`, `FAV`) + 12 Categories | Scrollable patch list only |
| 03 | `SoundEditor` | `sound_editor_screen.cpp` | `TabBar` Primary (`SOUND`) + `Performance` `SOUND EDIT` | 7 Sub-Tabs (`OSC`, `FM`, `FILTER`, `AMP`, `ENV`, `LFO`, `MOD`) | Zero vertical scroll |
| 04 | `Layers` | `layers_screen.cpp` | `TabBar` Primary (`LAYERS`) | `SINGLE`, `DUAL LAYER`, `SPLIT KEYBOARD` | Zero vertical scroll |
| 05 | `Mixer` | `mixer_screen.cpp` | `Performance` `MIXER` / `Layers` `OPEN MIXER` | 4 Channel Strips (`LAYER A`, `LAYER B`, `DRUMS`, `MASTER`) | Zero vertical scroll |
| 06 | `Modulation` | `modulation_screen.cpp` | `SoundEditor` (`MOD` tab / `FULL MATRIX`) | Paged 8-Slot Matrix (`SLOTS 1-4`, `SLOTS 5-8`) | Zero vertical scroll |
| 07 | `Effects` | `effects_screen.cpp` | `TabBar` Primary (`FX`) | 4 Sub-Tabs (`CHORUS`, `REVERB`, `DELAY`, `DRIVE`) | Zero vertical scroll |
| 08 | `Arpeggiator` | `arpeggiator_screen.cpp` | `Performance` `ARP` / `Sequencer` `OPEN ARP` | `UP`, `DOWN`, `UP/DOWN`, `RANDOM`, `AS PLAYED` | Zero vertical scroll |
| 09 | `Sequencer` | `sequencer_screen.cpp` | `TabBar` Primary (`SEQ`) | 16-Step Melodic Grid + Step Inspector (`1..16`) | Zero vertical scroll |
| 10 | `Drums` | `drums_screen.cpp` | `Sequencer` `DRUMS` / `Performance` `DRUMS` | 4-Track × 16-Step Drum Matrix (`KICK`, `SNARE`, `HIHAT`, `PERC`) | Zero vertical scroll |
| 11 | `Harmony` | `harmony_screen.cpp` | `TabBar` Primary (`HARMONY`) | Scale Quantizer + Chord Assist + 2-Octave Keyboard Preview | Zero vertical scroll |
| 12 | `Midi` | `midi_screen.cpp` | `TabBar` Secondary (`MIDI`) | SMK25 V2 Profile + 8-Row CC Table + Live MIDI Monitor | Zero vertical scroll |
| 13 | `Settings` | `settings_screen.cpp` | `TabBar` Secondary (`SETUP`) | Display, Audio Output, Persistence Policy, Factory Reset | Zero vertical scroll |
| 14 | `Diagnostics` | `diagnostics_screen.cpp` | `TabBar` Secondary (`DIAG`) | 6 Structured Cards (Firmware/UI, Real LVGL Heap, AMY, I2S, USB-MIDI, Board) | Zero vertical scroll |

---

## Detailed Screen Specifications

### TELA 01 — `PERFORMANCE` (`performance_screen.cpp`)
- **Stage Hero Patch Selector (`y: 4..82`)**: Displays current program (`128: Juno A60 Strings` or `135: DX7 Rhodes Mark I`), synthesis family badge (`SUBTRACTIVE` / `FM 6-OP`), category, `AVAILABLE` / `PLANNED` / `DEMO` status badge, `< PREV` / `NEXT >` program buttons, `BROWSER` shortcut, `RESET` button, and a `COMPACT` / `DETAILED` view toggle.
- **Stage Mode & Master Strip (`y: 86..204`)**:
  - Layer mode selector (`SINGLE`, `DUAL LAYER`, `SPLIT`), `OCT -` / `OCT +` buttons, `SUSTAIN` toggle, and quick launchers (`SOUND EDIT`, `MIXER`, `ARP`, `DRUMS`).
  - Master Volume slider (`AVAILABLE`), Expression CC11 slider (`AVAILABLE`), and DEMO stereo bus meter explicitly labeled `OUT L/R (DEMO SIM)`.
- **Contextual Macro Strip (`y: 208..304`)**:
  - `COMPACT` mode: 4 large touch cards (`TIMBRE / CUTOFF`, `COLOR / RES`, `SPACE / FX`, `MOTION / ENV`).
  - `DETAILED` mode: 8 contextual macro cards (`M1 TIMBRE`, `M2 COLOR`, `M3 SPACE`, `M4 MOTION`, `ATTACK`, `RELEASE`, `CHORUS`, `REVERB`).
- **Interactive Stage Performance Deck (`y: 308..388`)**:
  - Pitch Bend / Mod Wheel display + `DEMO CHORD` and `ALL NOTES OFF` triggers + interactive 2-octave `VirtualKeyboard` (`C3..B4`) with live split-point and harmonic quantization visualization.

### TELA 02 — `PATCH BROWSER` (`patch_browser_screen.cpp`)
- **Source & Bank Filter Bar (`y: 4..44`)**: Filter chips for `ALL (13)`, `P4 AVAIL (2)`, `S3/AMY PLAN (9)`, and `FAVORITES`, plus quick text filter cycle (`SEARCH: ALL` / `JUNO` / `DX7` / `BASS` / `PAD`).
- **Category Matrix (`x: 8..228, y: 48..386`)**: 12 category buttons (`ALL`, `Piano`, `Electric Piano`, `Organ`, `Bass`, `Brass`, `Strings`, `Pads`, `Leads`, `Plucks`, `Percussion`, `FX`).
- **Scrollable Patch List (`x: 234..544, y: 48..386`)**: Displays program number, patch name, `[FAV]` tag, and explicit source badge (`AVAILABLE`, `PLANNED`, `DEMO`).
- **Patch Inspector Panel (`x: 550..792, y: 48..386`)**: Shows synthesis family, default macro profile, polyphony target, favorite toggle (`FAV *`), `DEMO AUDITION NOTE` trigger, and `OPEN IN SOUND EDITOR` action.

### TELA 03 — `SOUND EDITOR` (`sound_editor_screen.cpp`)
- **Context Banner (`y: 4..40`)**: Displays active patch name, synthesis family selector (`SUBTRACTIVE`, `FM (DX7)`, `ADDITIVE`, `WAVETABLE`, `PCM`), and `OPEN FULL MOD MATRIX` button.
- **Sub-Tab Bar (`y: 44..78`)**: 7 synthesis tabs:
  1. **`OSC`**: Waveform (`SAW`, `PULSE`, `SINE`, `TRI`, `NOISE`, `WT`), Coarse Pitch, Fine Tune, Pulse Width (`UNAVAILABLE` when waveform $\neq$ `PULSE`), Osc Level, Pan, Unison Voices, Unison Detune.
  2. **`FM`**: Algorithm (`ALG 1..8`), Active Operator (`OP 1..6`), Op Ratio, Op Level, Feedback, Velocity Sensitivity (`AVAILABLE` for FM family; automatically marked `UNAVAILABLE` and disabled when Subtractive family is selected).
  3. **`FILTER`**: Filter Type (`LPF 24dB`, `LPF 12dB`, `HPF`, `BPF`, `BYPASS`), Cutoff Hz, Resonance, Drive, Key Tracking, Env Amount (automatically marked `UNAVAILABLE` when Filter Type is `BYPASS`).
  4. **`AMP`**: Master Volume (`AVAILABLE`), Expression (`AVAILABLE`), Layer A Volume (`AVAILABLE`), Layer A Pan (`PLANNED`).
  5. **`ENV`**: Interactive graphical `EnvelopeEditor` (`lv_line` ADSR curve with Attack, Decay, Sustain, Release point markers) + 4 rotary knobs (`ATTACK`, `DECAY`, `SUSTAIN`, `RELEASE`).
  6. **`LFO`**: Waveform, Rate Hz, Depth, Tempo Sync toggle, Phase, and Primary Destination selector.
  7. **`MOD`**: Quick 4-slot modulation matrix view + direct button to `TELA 06 — MODULATION`.

### TELA 04 — `LAYERS` (`layers_screen.cpp`)
- **Mode & Split Point Header (`y: 4..48`)**: Selects `SINGLE`, `DUAL LAYER`, or `SPLIT KEYBOARD`, adjusts Split Note (`C3 / 60`) with `-` / `+` buttons, and provides an `OPEN MIXER` shortcut.
- **Side-by-Side Layer Cards (`y: 52..276`)**:
  - **Layer A (`x: 8..396`)**: Enable toggle (`AVAILABLE`), Patch selector, Volume (`AVAILABLE`), Pan (`PLANNED`), Octave (`AVAILABLE`), Semitone Transpose, Key Range (`C1..B5`), and `MUTE` / `SOLO` buttons.
  - **Layer B (`x: 404..792`)**: Enable toggle (`PLANNED`), Patch selector, Volume, Pan, Octave, Semitone Transpose, Key Range, and `MUTE` / `SOLO` buttons.
- **Visual Split / Layer Keyboard Zone Map (`y: 280..388`)**: Interactive 2-octave keyboard color-coding Layer A (`#67C4A5`), Layer B (`#E4A44D`), and Split Divider (`#EAB65B`).

### TELA 05 — `MIXER` (`mixer_screen.cpp`)
- **4 Vertical Channel Strips (`y: 42..386`)**: `LAYER A`, `LAYER B`, `DRUMS`, and `MASTER BUS`.
- Each strip includes:
  - Explicit `FeatureStatus` badge (`AVAILABLE` on Master, `PLANNED` on sub-channels).
  - Stereo `SignalMeter` explicitly labeled `DEMO SIM` (never represented as real DSP audio peaks).
  - Volume slider, Pan slider, FX Send / Expression slider, and touch-friendly `MUTE` / `SOLO` buttons (or `PANIC / RESET` on Master).

### TELA 06 — `MODULATION` (`modulation_screen.cpp`)
- **Paged 8-Slot Matrix (`SLOTS 1-4` / `SLOTS 5-8`)**: Prevents cramped tables on `800 × 480` by displaying 4 large `56 px`-tall touch rows per page (`ENABLE`, `SOURCE` cycle button, `DESTINATION` cycle button, `-100..+100%` amount slider, and `BIPOLAR` / `UNIPOLAR` toggle).
- **Live Expressive Source Monitor (`y: 294..386`)**: Displays live values for `MOD WHEEL (CC1)`, `PITCH BEND`, `EXPRESSION (CC11)`, `SUSTAIN (CC64)`, `LFO RATE`, and `FILTER ENV`.

### TELA 07 — `EFFECTS` (`effects_screen.cpp`)
- **Signal Chain Header (`y: 4..40`)**: Visualizes `SYNTH -> CHORUS -> DELAY -> REVERB -> DRIVE -> OUT` (reflecting AMY `src/chorus.cpp`, `reverb.cpp`, `echo.cpp` DSP capabilities).
- **4 Dedicated Sub-Tabs (`y: 44..78`)**:
  - `CHORUS`: Enable, Rate Hz, Depth, Mix.
  - `REVERB`: Enable, Liveness, Damping, Crossover Hz, Mix.
  - `DELAY`: Enable, Time ms, Feedback, Filter Cutoff, Mix.
  - `DRIVE`: Enable, Drive Amount, Output Level.

### TELA 08 — `ARPEGGIATOR` (`arpeggiator_screen.cpp`)
- **Transport & Clock Bar (`y: 4..48`)**: `ARP ENABLE`, `LATCH HOLD`, `BPM -/+`, and `OPEN SEQUENCER` shortcut.
- **Mode Selector Cards (`y: 54..110`)**: `UP`, `DOWN`, `UP/DOWN`, `RANDOM`, `AS PLAYED`.
- **Rotary Controls (`y: 116..286`)**: Division (`1/4..1/32`), Gate %, Octave Span (`1..4`), Swing %.
- **Held Notes & Pattern Visualizer (`y: 292..386`)**: Shows latched chord input notes and generated octave-stepped pattern.

### TELA 09 — `SEQUENCER` (`sequencer_screen.cpp`)
- **Transport Header (`y: 4..48`)**: `PLAY` / `STOP` transport, `REC` indicator, `BPM -/+`, Swing %, and navigation buttons to `ARP` and `DRUMS`.
- **Interactive 16-Step Melodic Grid (`y: 54..224`)**: 16 touch step columns (`46 px` wide each) with velocity bar, note label, tie marker, and playhead cursor. Tapping a step toggles active/selects it for inspection.
- **Selected Step Inspector (`y: 230..386`)**: Adjusts selected step (`1..16`) Note (`NOTE -/+`), Velocity slider, Gate % slider, Tie toggle, and Pattern Clear/Reset.

### TELA 10 — `DRUMS` (`drums_screen.cpp`)
- **Transport & Kit Header (`y: 4..48`)**: `PLAY` / `STOP`, `KIT: AMY PCM-808`, `BPM -/+`, and `MELODIC SEQ` shortcut.
- **4 × 16 Drum Matrix (`y: 54..254`)**: Interactive 16-step triggers for `KICK`, `SNARE`, `HIHAT`, and `PERC` with Normal → Accent (`ACC`) → Off cycling and per-track `MUTE` buttons.
- **Selected Track Mixer (`y: 260..386`)**: Track selector (`KICK`, `SNARE`, `HIHAT`, `PERC`), Track Level knob, Track Pitch knob, and `CLEAR TRACK` / `DEFAULT GROOVE` actions.

### TELA 11 — `HARMONY` (`harmony_screen.cpp`)
- **Header (`y: 4..48`)**: Root Key selector (`KEY -/+` across `C..B`), Scale Lock toggle, and Chord Assist toggle (`smk-s3/components/midi/include/scale_quantizer.h` & `chord_memory.h` parity).
- **Scale Mode Selector (`y: 54..152`)**: Touch buttons for `CHROMATIC`, `MAJOR`, `MINOR`, `DORIAN`, `MIXOLYDIAN`, `PENTATONIC MAJ`, `PENTATONIC MIN`, `BLUES`.
- **Chord Assist Voicing Selector (`y: 158..252`)**: `OFF`, `MAJ TRIAD`, `MIN TRIAD`, `SUS4`, `DOM 7TH`, `MAJ 7TH`, `MIN 7TH`, `POWER 5TH` + Voicing Spread (`CLOSE`, `OPEN`, `DROP-2`, `OCTAVE`).
- **Interactive Harmonic Preview Keyboard (`y: 258..386`)**: Tapping any key computes the quantized root and full chord voicing via `UiModel::computeHarmonyPreview()` and highlights the resulting notes on the keyboard in real time.

### TELA 12 — `MIDI` (`midi_screen.cpp`)
- **Controller Profile Header (`y: 4..48`)**: Displays `M-VAVE SMK25 V2` profile, Input Channel selector (`OMNI`, `CH 1..16`), Clock Sync mode, Velocity Curve, and `MIDI LEARN` toggle.
- **CC Mapping Table (`x: 8..488, y: 54..386`)**: 8-row mapping table (`CC 1`, `CC 7`, `CC 11`, `CC 64`, `CC 74`, `CC 71`, `CC 91`, `CC 93`) showing target parameter, range, last value, and `AVAILABLE` vs `PLANNED` status.
- **Live MIDI Monitor (`x: 496..792, y: 54..386`)**: Displays the last 5 MIDI / Demo events with channel, status, data bytes, and timestamp, plus test injection buttons (`INJECT NOTE`, `INJECT CC74`, `PANIC`).

### TELA 13 — `SETTINGS` (`settings_screen.cpp`)
- **Display & Ergonomics Card (`x: 8..396, y: 42..210`)**: Brightness slider (`AVAILABLE`), UI Theme Mode, Stage High-Contrast Mode, Touch Beep toggle.
- **Audio Output Card (`x: 404..792, y: 42..210`)**: Output Trim dB, Stereo/Mono mode, and explicit Guition JC-ESP32P4-M3 PCM5102A candidate wiring note (`BCK=50, WS=51, DIN=52`, 48 kHz 16-bit stereo).
- **Persistence & System Safety Card (`x: 8..792, y: 218..386`)**: Documents the deferred-save CRC32 policy (`AGENTS.md` §16 — zero Flash writes on knob movement) and provides safe RAM-only actions (`RESTORE FACTORY DEFAULTS`, `RESET CONTROLLERS`, `OPEN DIAGNOSTICS`).

### TELA 14 — `DIAGNOSTICS` (`diagnostics_screen.cpp`)
- **Explicit Telemetry Honesty Banner (`y: 4..38`)**: Clearly states `UI DEMO BACKEND ACTIVE: HARDWARE TELEMETRY IS NOT FABRICATED (REAL LVGL POOL SHOWN)`.
- **6 Structured Diagnostic Cards (`3 × 2` grid, `y: 42..386`)**:
  1. **Firmware & UI**: Blueprint version, uptime, navigation switch count, demo panic count, registered parameter count (`132`).
  2. **LVGL 9.3.0 Heap (`AVAILABLE`)**: Real-time `lv_mem_monitor()` readings (`FREE`, `LARGEST BLOCK`, `PEAK USED`, `FRAGMENTATION %`).
  3. **AMY Synthesis (`DEMO`)**: Explicitly reports `UI DEMO (NOT CONNECTED)` and `DSP RENDER: UNMEASURED (DEMO)`.
  4. **I2S & PCM5102A (`DEMO`)**: Reports `UI DEMO (NO HARDWARE I2S)` and candidate pins `BCK=50 WS=51 DIN=52`.
  5. **USB-MIDI Host (`DEMO`)**: Reports `UI DEMO (NO USB HOST)` and opt-in gate status.
  6. **System & Board (`DEMO`)**: Reports `GUITION JC-ESP32P4-M3`, `800x480 HORIZONTAL`, and `NVS WRITES: 0 (STRICT RAM)`.
