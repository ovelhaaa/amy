# SMK-P4 UI Blueprint 1.1 — Complete 14-Screen Inventory

All 14 screens are implemented as real, interactive LVGL 9.3.0 `ScreenView` classes under `smk-p4/components/ui_blueprint/screens/`, operating over the generic [`IUiBackend`](file:///C:/Users/devx/Documents/PlatformIO/Projects/amy/build/worktrees/smk-p4-ui-integration-ready/smk-p4/components/ui_blueprint/include/ui_backend.h) contract and constrained to the `800 × 480` horizontal frame (`StatusStrip` `y: 0..42`, `Content` `y: 42..434`, `BottomNavBar` `y: 434..480`).

---

## Summary Table

| # | Screen ID | File | Presentation Level | Sub-Tabs / Modes | Scroll Policy |
|---|---|---|---|---|---|
| 01 | `Performance` | `performance_screen.cpp` | `StageMinimal` | `0 = MACRO FOCUS` (Bank 1–4 / 5–8) \| `1 = KEYS FOCUS` (2-Octave Keys) | Zero vertical scroll |
| 02 | `PatchBrowser` | `patch_browser_screen.cpp` | `BlueprintVerbose` | 5 Bank Filters (`ALL`, `P4 ROM`, `S3/AMY ROM`, `USER`, `FAV`) + 12 Categories | Paginated (6 rows/page) |
| 03 | `SoundEditor` | `sound_editor_screen.cpp` | `BlueprintVerbose` | 7 Sub-Tabs (`OSC`, `FM`, `FILTER`, `AMP`, `ENV`, `LFO`, `MOD`) | Zero vertical scroll |
| 04 | `Layers` | `layers_screen.cpp` | `BlueprintVerbose` | `SINGLE A`, `SINGLE B`, `DUAL LAYER A+B`, `SPLIT A/B` | Zero vertical scroll |
| 05 | `Mixer` | `mixer_screen.cpp` | `BlueprintVerbose` | 4 Channel Strips (`LAYER A`, `LAYER B`, `DRUMS`, `MASTER`) | Zero vertical scroll |
| 06 | `Modulation` | `modulation_screen.cpp` | `BlueprintVerbose` | Paged 8-Slot Matrix (`SLOTS 1-4`, `SLOTS 5-8`) + LFO 1 Strip | Zero vertical scroll |
| 07 | `Effects` | `effects_screen.cpp` | `BlueprintVerbose` | Chorus, Delay, Reverb, Bus Drive, Tilt Tone, FX Chain | Zero vertical scroll |
| 08 | `Arpeggiator` | `arpeggiator_screen.cpp` | `BlueprintVerbose` | Enable, Latch, BPM, Rate, Direction, Octaves, Gate, Swing + Step Grid | Zero vertical scroll |
| 09 | `Sequencer` | `sequencer_screen.cpp` | `BlueprintVerbose` | 16-Step Melodic Grid + Transport + Step Inspector | Zero vertical scroll |
| 10 | `Drums` | `drums_screen.cpp` | `BlueprintVerbose` | 8 Drum Pads (`1 KICK`..`8 RIM`) + 16-Step Grid + Voice Controls | Zero vertical scroll |
| 11 | `Harmony` | `harmony_screen.cpp` | `BlueprintVerbose` | `0 = SCALE QUANTIZER` \| `1 = CHORD ASSIST & VOICING` + 2-Octave Preview | Zero vertical scroll |
| 12 | `MidiSettings` | `midi_screen.cpp` | `BlueprintVerbose` | `0 = MIDI CHANNELS & FILTERS` \| `1 = SMK25 CC MAPPING & LEARN` | Zero vertical scroll |
| 13 | `GlobalSettings` | `settings_screen.cpp` | `BlueprintVerbose` | Display, Theme, Sample Rate (`48000 Hz` RO), Tuning, Boot State, Storage | Zero vertical scroll |
| 14 | `Diagnostics` | `diagnostics_screen.cpp` | `BlueprintVerbose` | 6 Structured Cards (Firmware 1.1, Real LVGL Heap, AMY, I2S, USB-MIDI, Board) | Zero vertical scroll |

---

## Key Screen Refinements in UI Blueprint 1.1

### TELA 01 — `PERFORMANCE STAGE V2` (`performance_screen.cpp`)
- **Dominant Patch Banner (`y: 4..62`, `StageMinimal`)**: Displays active patch (`128 DX7 BRASS 1` or `135 DX7 PIANO 1`), category, synthesis family (`FM 6-OP` / `SUBTRACTIVE`), bank (`ROM P4`), and enlarged touch buttons `< PREV`, `NEXT >`, `FAV`, `BROWSER`, and `EDIT`.
- **Stage Control Bar (`y: 66..104`)**:
  - `LAYER: SINGLE A` button, `HARM: C NAT MINOR | MINOR` button, Macro Bank toggle (`BANK: MACROS 1-4` / `BANK: MACROS 5-8` in `MACRO FOCUS`) or command state indicator (`CMD: DEMO RAM` in `KEYS FOCUS`), and mode switch button (`KEYS FOCUS >` / `< MACRO FOCUS`).
- **Two Stage Modes**:
  - **`MACRO FOCUS` (`subTab = 0`)**: 4 large macro cards per page (`388 × 74 px` in a 2×2 grid) with expanded labels (`M1 CHAR`, `M2 BRIGHT`, `M3 MOTION`, `M4 FDBK/SHAPE`, `M5 RATIO/ATTACK`, `M6 DETUNE/RELEASE`, `M7 SPACE`, `M8 DRIVE`), `MASTER LEVEL` (`232 × 72 px`), `OCT -` / `OCT +0` / `OCT +` (`82 × 44 px`), and 1-octave stage keyboard (`544 × 120 px`).
  - **`KEYS FOCUS` (`subTab = 1`)**: 4 compact macro knobs (`130 × 106 px`), `MASTER LEVEL` (`240 × 64 px`), `OCT -` / `OCT +0` / `OCT +` (`86 × 38 px`), and a full-width 2-octave touch keyboard (`784 × 170 px`, `StageKeys2Oct`).
- **In-Place Refresh**: Changing octave (`OCT -` / `OCT +`), triggering `PANIC`, or playing keys updates labels and key colors in-place via `refresh(IUiBackend&)` without destroying widgets during held notes.

### TELA 02 — `PATCH BROWSER` (`patch_browser_screen.cpp`)
- Unifies P4 ROM patch names (`128 DX7 BRASS 1` and `135 DX7 PIANO 1`) with `smk-p4/components/amy_runtime/runtime_config.h`.
- Right-hand `PATCH INSPECTOR` displays `M1-4: CHAR|BRIGHT|MOTION|FDBK` and `P4 RUNTIME: AMY ROM READY` without text clipping.
- `AUDITION DEMO NOTE (RAM)` triggers a clean Note On + Note Off pulse via `IUiBackend`.

### TELA 03 — `SOUND EDITOR` (`sound_editor_screen.cpp`)
- 7 synthesis sub-tabs (`OSC`, `FM`, `FILTER`, `AMP`, `ENV`, `LFO`, `MOD`) operating over `IUiBackend`.
- Concise topology notes in `FM` and `FILTER` tabs fit strictly within `756 px` with zero right-edge text clipping.

### TELA 10 — `DRUMS` (`drums_screen.cpp`)
- 8 pad selectors + 16-step grid + drum voice controls + concise `808 PCM DRUM ENGINE` status card (`"PCM 808 in src/pcm_tiny.h (PLANNED)."`).

### TELA 11 — `HARMONY ASSIST` (`harmony_screen.cpp`)
- Tapping any key on the 2-octave `HarmonyView` keyboard updates the `IN -> QUANT -> CHORD` inspector strip and key colors **in-place** via `HarmonyScreen::refresh(IUiBackend&)` without calling `navigateTo()` or destroying the active widget tree during key callbacks.

### TELA 12 — `MIDI & MAPPING` (`midi_screen.cpp`)
- Explicitly labels `LAYER A CH` (`layer_a.channel`) and `LAYER B CH` (`layer_b.channel`) in Row 2 so channel cards are unambiguous.
