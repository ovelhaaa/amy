# SMK-P4 UI Blueprint 1.1 — Validation Results & Stage Polish Report

**Branch:** `feature/smk-p4-ui-integration-ready`\
**Base Branch:** `feature/smk-p4-ui-blueprint`\
**Target Display:** Guition JC-ESP32P4-M3 (`800 × 480` horizontal, RGB565, GT911 Capacitive Touch)\
**UI Engine:** LVGL 9.3.0\

---

## 1. Executive Summary

UI Blueprint 1.1 evolves the 14-screen, 132-parameter LVGL 9.3.0 UI architecture from a RAM-only prototype coupled to `UiDemoBackend` into an **integration-ready stage instrument frontend** decoupled via `IUiBackend`, with deterministic virtual keyboard touch lifecycle semantics and a redesigned **Performance Stage v2** layout (`MACRO FOCUS` and `KEYS FOCUS`).

All work was executed in strict isolation from the musical firmware (`src/`, `smk-s3/`, `smk-p4/components/amy_runtime/`, `smk-p4/components/audio_pcm5102/`, `smk-p4/components/usb_midi_host/`, `smk-p4/main/`) and without accessing the USB-connected Guition hardware.

---

## 2. Key Architectural & Visual Improvements (1.0 → 1.1)

### 2.1 Virtual Keyboard Touch Lifecycle Hardening (`P0`)
In Blueprint 1.0, `widgets/virtual_keyboard.cpp` handled both `LV_EVENT_PRESSED` and `LV_EVENT_CLICKED` as `Note On` triggers and invoked destructive `navigateTo()` inside the `HarmonyView` key callback while the pressed key object was still executing its LVGL event handler.

In Blueprint 1.1 (`widgets/virtual_keyboard.cpp`):
- **`LV_EVENT_PRESSED`** emits `backend.requestNoteOn(note, 100)` **at most once** per touch interaction (`if (!slot.pressed)`), storing both `slot.active_note` and `slot.active_token` on the key slot.
- **`LV_EVENT_RELEASED`** and **`LV_EVENT_PRESS_LOST`** emit `backend.requestNoteOff(slot.active_note, slot.active_token)` only if `slot.pressed` is true, clearing the slot before dispatching.
- **`LV_EVENT_CLICKED`** is explicitly ignored for note generation, eliminating duplicate `Note On` emissions on tap.
- **Octave-Shift Safety While Held:** Because `slot.active_note` is captured at press time, shifting `perf.octave_shift` while holding a key still sends `Note Off` for the exact original MIDI pitch and token when the finger lifts.
- **Non-Destructive Harmony Preview:** Pressing a key in `HarmonyView` updates `harm.preview_note` and refreshes key highlights + `nav->refreshActive()` in-place without calling `navigateTo()` inside the key event callback.
- **Panic & Screen Teardown Cleanup:** `LV_EVENT_DELETE` on the keyboard container automatically releases all active slots (`releaseActiveSlots(state, true)`), and `releaseVirtualKeyboardNotes()` clears held visual state on `PANIC` or screen transition without emitting mismatched `Note Off` events for stale tokens.

### 2.2 `IUiBackend` Decoupling & Command State Contract (`P0`)
- Created `include/ui_backend.h` defining `IUiBackend`, `UiCommandState` (`Confirmed`, `Requested`, `DemoApplied`, `Unsupported`, `Rejected`), `UiCommandResult`, `PresentationLevel` (`StageMinimal`, `BlueprintVerbose`), and `UiSignalMeters`.
- Refactored `UiBlueprint`, `UiNavigationManager`, all 11 widgets (`widgets/*.cpp`), and all 14 screens (`screens/*.cpp`) to depend exclusively on `IUiBackend&` rather than `UiDemoBackend&`.
- Parameter widgets (`parameter_slider.cpp`, `parameter_knob.cpp`, `parameter_card.cpp`, `patch_selector.cpp`) now submit command requests through `IUiBackend` (`requestParameterFloat/Int/Bool`, `requestSelectPatch`) and immediately sync or revert their visual state from `backend.model()` when a command returns `!res.accepted()` (`Unsupported` / `Rejected`).
- Documented the future firmware adapter `UiBlueprintPerformanceAdapter` in `smk-p4/docs/ui_backend_port_contract.md` and validated it via `MockPerformanceBackendAdapter` in `tests/blueprint_test_suite.cpp`.

### 2.3 Performance Stage v2 & Visual Noise Reduction (`P0` / `P1`)
- Introduced two presentation levels (`PresentationLevel::StageMinimal` on Performance Stage v2 vs `PresentationLevel::BlueprintVerbose` on deep editors and diagnostics).
- **Zero Individual `PLAN` Badges on Performance Stage v2:** Removed per-widget `PLAN` pills from the stage view while keeping the honest global `SIMULATED UI (RAM)` badge in the Top Status Bar (`top_status_bar.cpp`) whenever `backend.isSimulated() == true`.
- **`MACRO FOCUS` (`01_performance_macros.png`, `subTab = 0`):**
  - Dominant Patch Banner (`P01 — JUPITER BRASS`, `montserrat_20`, category + DSP badges, `88 × 46 px` `PREV`/`NEXT`, `94 × 46 px` `BROWSER`).
  - Stage Control Bar (`LAYER`, `HARM`, `CMD: DEMO RAM`, bank toggle `BANK: MACROS 1-4` ↔ `BANK: MACROS 5-8` (`154 × 32 px`), mode switch `KEYS FOCUS >` (`134 × 32 px`)).
  - **2 × 2 Grid of 4 Large Macro Cards (`388 × 74 px` each)** with `montserrat_20` value readouts and `18 px` touch slider bars (`M1 CHARACTER`, `M2 BRIGHT`, `M3 MOTION`, `M4 SHAPE` or `M5 TIMBRE`, `M6 DETUNE`, `M7 SPACE`, `M8 DRIVE`).
  - Bottom Stage Dock with `MASTER LEVEL` (`232 × 72 px`), `OCT -` / `OCT +0` / `OCT +` (`82 × 44 px`, in-place refresh without destroying the keyboard), and 1-octave live touch keyboard (`544 × 120 px`).
- **`KEYS FOCUS` (`01b_performance_keys.png`, `subTab = 1`):**
  - Compact 4-Macro Knob Row (`130 × 106 px` each) + `MASTER LEVEL` (`240 × 64 px`) + `OCT -` / `OCT +0` / `OCT +` (`86 × 38 px`).
  - Full-width **2-Octave Playable Keyboard (`784 × 170 px`, `StageKeys2Oct`, 24 semitones)** with large white/black key touch targets.
- **Label Clarity & Zero Text Truncation:**
  - Expanded abbreviated macro labels (`BRTE` → `BRIGHT`, `MOTN` → `MOTION`, `SHAP` → `SHAPE`, `SPCE` → `SPACE`, `DRV` → `DRIVE`).
  - Eliminated text clipping across `01b_performance_keys.png`, `02_patch_browser.png`, `03b_sound_editor_fm.png`, `03c_sound_editor_filter.png`, `10_drums.png`, `12a_midi_channels.png` (`LAYER A CH` / `LAYER B CH`), and `14_diagnostics.png`.

---

## 3. Automated Test Suite Results (`PASS`)

Executed via `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1`:

| Test Group | Description | Result |
| :--- | :--- | :--- |
| `[1/7]` Catalog & Metadata Audit | Verifies 132 parameters across 18 namespaces, valid ranges, steps, defaults, and non-null metadata | `PASS` |
| `[2/7]` Navigation Walk (14 Screens + Sub-tabs) | Walks all 14 `ScreenId` screens + sub-tabs + `navigateBack()` history stack | `PASS` |
| `[3/7]` Virtual Keyboard Touch Lifecycle (`testVirtualKeyboardLifecycle`) | Verifies event count and exact order for: press+release, duplicate `PRESSED` suppression, `CLICKED` ignored, `PRESS_LOST` recovery, octave shift while key held, `PANIC` during held key, screen navigation during held key, `HarmonyView` non-destructive key press, and rapid repeated notes | `PASS` |
| `[4/7]` Mock PerformanceBackend Contract (`testMockPerformanceBackendContract`) | Verifies `IUiBackend` with `MockPerformanceBackendAdapter`: supported commands (`perf.patch_id` `0..3`, `perf.master_volume`, `noteOn`, `noteOff`, `panic`) return `Confirmed`; unsupported patches (`patch_id >= 4`) return `Rejected` and revert UI; planned parameters (`macro.*`, `filt.cutoff`) return `Unsupported` and revert slider visual state without mutating hardware state | `PASS` |
| `[5/7]` Functional Scenarios | Patch Browser select/revert, Layer Split/Dual enable, Sequencer step toggle & page switch, Drum step toggle, Harmony scale lock & chord pad, MIDI Learn toggle, Panic reset | `PASS` |
| `[6/7]` LVGL 9.3.0 700-Screen Stress Test | 50 full cycles across all 14 screens (700 screen builds/teardowns) measuring LVGL heap | `PASS` (`0 bytes` leaked) |
| `[7/7]` 20 Screenshot Captures (`800 × 480`) | Renders 20 BMP + PNG screens via headless LVGL 9.3.0 software renderer | `PASS` |

---

## 4. Real LVGL 9.3.0 Memory Measurements

Measured directly from `lv_mem_monitor(&mon)` in `tests/blueprint_test_suite.cpp` (128 KB LVGL internal heap pool):

| Metric | Value | Notes |
| :--- | :--- | :--- |
| **LVGL Heap Pool Size** | `131,072 bytes` (128 KB) | Configured in `lv_conf.h` |
| **Baseline Free (Initial Performance Screen)** | `64,896 bytes` | Persistent shell + active screen |
| **After 700 Screen Transitions Free** | `64,896 bytes` | Exact match (`diff = 0 bytes`) |
| **Memory Leaked Across 700 Transitions** | **`0 bytes`** | Verified via `assert( leaked <= 256 )` |
| **Peak Heap Used** | `98,552 bytes` (75.2%) | Leaves > 32 KB headroom inside the 128 KB pool |
| **Heap Fragmentation After Stress Test** | **`1%`** | Negligible fragmentation |

---

## 5. Rendered Screenshot Catalog (`800 × 480` PNG)

Generated in `build/ui_preview_shots/`:

1. `01_performance_macros.png` — Performance Stage v2 (`MACRO FOCUS`, Bank 1–4, 2×2 large macro cards, 1-oct keyboard)
2. `01b_performance_keys.png` — Performance Stage v2 (`KEYS FOCUS`, compact macro knobs, 2-oct stage keyboard)
3. `02_patch_browser.png` — Patch Browser (category pills, 8-patch list, macro summary preview, `REVERT` / `LOAD PATCH`)
4. `03a_sound_editor_osc.png` — Sound Editor: `OSC` tab
5. `03b_sound_editor_fm.png` — Sound Editor: `FM` tab (`ALGO`, `FEEDBACK`, `OP1-OP4`)
6. `03c_sound_editor_filter.png` — Sound Editor: `FILTER` tab (`CUTOFF`, `RES`, `ENV AMT`, `KEY TRK`, `DRIVE`)
7. `03d_sound_editor_env.png` — Sound Editor: `ENV` tab (`AMP ADSR` + `FILTER ADSR` + visual curve)
8. `03e_sound_editor_mod.png` — Sound Editor: `MOD` tab (`LFO 1`, `LFO 2`, 4-slot mod matrix)
9. `04_macros.png` — Macro Assign & Curves (Macros 1–8)
10. `05_layers_split.png` — Layers & Split (Layer A/B patch, level, pan, split point, zone visual bar)
11. `06_mixer.png` — Bus Mixer (Layer A, Layer B, Drum Bus, FX Return, Master channel strips)
12. `07_effects.png` — Effects Chain (`CHORUS`, `DELAY`, `REVERB`, `EQ / DRIVE`)
13. `08_arpeggiator.png` — Arpeggiator
14. `09a_sequencer_p1.png` — Step Sequencer (Steps 1–8)
15. `09b_sequencer_p2.png` — Step Sequencer (Steps 9–16)
16. `10_drums.png` — Drum Grid (4 lanes × 16 steps: `BD`, `SD`, `CH`, `OH`, `ACC`, `FLAM`)
17. `11a_harmony_scale.png` — Harmony Assist: Scale Lock & Keyboard Preview
18. `11b_harmony_chords.png` — Harmony Assist: Chord Pads 1–4 & Progression Builder
19. `12a_midi_channels.png` — MIDI & Mapping: Channels, Clock & Transport
20. `12b_midi_cc_map.png` — MIDI & Mapping: SMK25 Knob 1–8 CC Map & MIDI Learn
21. `13_global_settings.png` — Global Settings (Audio/UI/System)
22. `14_diagnostics.png` — Diagnostics & Resource Monitor

---

## 6. Quality Gate Verification (Gates A – F)

- **Gate A (Isolation):** `PASS` — Zero modifications outside `smk-p4/components/ui_blueprint/`, `smk-p4/ui-preview/`, and `smk-p4/docs/ui_*.md`. Guition USB hardware untouched.
- **Gate B (Virtual Keyboard Safety):** `PASS` — Single `Note On` per press, deterministic `Note Off` on `RELEASED` / `PRESS_LOST` / `DELETE` / `PANIC`, `CLICKED` ignored, octave shift safe while held, non-destructive `HarmonyView` update.
- **Gate C (`IUiBackend` Decoupling):** `PASS` — Widgets and screens communicate solely via `IUiBackend`; `UiCommandResult` distinguishes `Confirmed`, `Requested`, `DemoApplied`, `Unsupported`, and `Rejected`; rejected/unsupported edits revert cleanly.
- **Gate D (Performance Stage v2 & Ergonomics):** `PASS` — Both `MACRO FOCUS` and `KEYS FOCUS` implemented in `StageMinimal` mode, expanded labels (`BRIGHT`, `MOTION`, `SHAPE`, `SPACE`, `DRIVE`), zero individual `PLAN` badge clutter on Stage view, honest top-bar `SIMULATED UI (RAM)` indicator preserved, touch targets ≥ `38–74 px` height.
- **Gate E (Host Preview & Memory):** `PASS` — 7/7 test groups pass, `0 bytes` leaked across 700 screen transitions, 22 BMP/PNG captures verified.
- **Gate F (Documentation):** `PASS` — `ui_backend_port_contract.md`, `ui_stage_design_v2.md`, `ui_blueprint_11_results.md`, `ui_architecture.md`, `ui_feature_matrix.md`, `ui_screen_inventory.md`, and `ui_navigation_map.md` updated and consistent.
