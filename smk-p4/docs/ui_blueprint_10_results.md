# SMK-P4 UI Blueprint 1.0 — Implementation & Verification Report

**Date:** 2026-11-03
**Branch:** `feature/smk-p4-ui-blueprint` (branched from `feature/smk-p4-standalone-usb-midi` at `0f2598f3`)
**Target Hardware:** Guition JC-ESP32P4-M3 (`ESP32-P4`, `ST7701S` MIPI-DSI `800 × 480` horizontal, `GT911` capacitive touch)
**Framework:** LVGL 9.3.0 / ESP-IDF 5.5.2

---

## 1. Executive Summary

This milestone delivers **SMK-P4 UI Blueprint 1.0**, a complete, navigable 14-screen LVGL 9.3.0 user interface architecture, canonical parameter registry (`132` parameters across `18` namespaces), demonstrative state backend (`UiDemoBackend`), and automated host verification & screenshot capture harness (`smk-p4/ui-preview`).

Work was executed in an isolated Git worktree on branch `feature/smk-p4-ui-blueprint` with **zero modifications** to protected audio, USB-MIDI, AMY runtime, or `smk-p4/main/` files.

---

## 2. Central Separation Principle Verification

| Layer | Component Files | Responsibility | Real-Time / Hardware Coupling |
|---|---|---|---|
| **1. Visual Interface** | `ui_theme.*`, `widgets/*.cpp`, `screens/*.cpp` | Industrial `800 × 480` landscape layouts, touch controls, visualizers, and status badges. | None (pure LVGL 9.3.0 widget tree). |
| **2. Parameter Model** | `ui_parameters.*`, `ui_model.*` | `132` canonical parameters with ranges, defaults, units, capabilities, dependency rules, and `FeatureStatus` (`AVAIL`, `PLAN`, `N/A`, `DEMO`). | None (deterministic in-memory model). |
| **3. Navigation** | `ui_navigation.*` | Single-active-screen lifecycle, 3-bank bottom navigation (`<= 6` destinations visible at once), sub-tab persistence, history back-stack, and 1-tap `PERF` return. | None. |
| **4. Demonstrative State** | `ui_demo_backend.*` | Interactive RAM-only state, Scale Quantizer & Chord Assist live preview, simulated signal meters (`[SIM DEMO]`), and genuine `lv_mem_monitor()` heap telemetry. | None (never touches I2S, USB, NVS, or AMY). |
| **5. Real Musical Backend** | `smk-p4/components/amy_runtime`, `audio_pcm5102`, `usb_midi_host`, `smk-p4/main/*` | Hardware synthesis, I2S DMA, and USB-MIDI Host. | **100% Untouched** (`git diff` empty across all protected paths). |

---

## 3. Automated Validation Results (`PASS` / `FAIL` / `MANUAL`)

| Test / Verification Suite | Command | Status | Summary of Measured Results |
|---|---|---|---|
| **LVGL 9.3.0 + UI Blueprint Host Build** | `powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1` | `PASS` | 240 LVGL 9.3.0 C objects + 26 UI Blueprint C++20 objects compiled cleanly with `-Wall -Wextra -Werror`. |
| **`[1/5]` Parameter Registry & Model** | `blueprint_test_suite.exe` | `PASS` | Verified 132 unique parameter descriptors, min/max clamping, type safety, read-only guard (`sys.sample_rate_hz`), dependency disables (`osc.1.pulse_width`), and FM vs Subtractive family gating. |
| **`[2/5]` 14-Screen Navigation & History** | `blueprint_test_suite.exe` | `PASS` | Verified all 14 screens mount/unmount cleanly, sub-tab state persists across screen switches, bank cycling (`MAIN`, `TOOLS`, `SYS`), `back()` stack, and 1-tap `returnToPerformance()`. |
| **`[3/5]` Interactive Widgets & Musical Logic** | `blueprint_test_suite.exe` | `PASS` | Verified slider/card/button LVGL event callbacks, Patch Browser filtering, Step Sequencer grid toggling, Scale Quantizer (`C#4 -> D4` in C Natural Minor), Chord Assist voicings, and Global Panic reset. |
| **`[4/5]` 800×480 Bounding-Box & Badge Audit** | `blueprint_test_suite.exe` | `PASS` | Traversed the live LVGL object tree across all 14 screens and sub-tabs; 100% of widgets fit strictly inside `800 × 480` with the persistent `UI DEMO` badge visible on every screen. |
| **`[5/5]` 700-Mount LVGL Memory Stress Test** | `blueprint_test_suite.exe` | `PASS` | Cycled through all 14 screens 50 times (`700` screen builds/teardowns): `baseline_free = 65784 B`, `after_free = 65784 B` (**`0 bytes` leaked**), `frag = 1%`, `post_shutdown_free = 117616 B`. |
| **20-Frame 800×480 RGB565 Screenshot Capture & Visual Review** | `preview_main.exe` | `PASS` | Rendered and visually inspected 20 PNG captures across all 14 screens and sub-tabs (`01_performance_macros.png` .. `14_diagnostics.png`). Verified contrast, touch ergonomics, badge spacing, and zero overlapping text. |
| **Existing P4 Host Unit Tests** | `smk-p4/tests/run_host.ps1`, `run_amy.ps1`, `run_usb.ps1` | `PASS` | Existing standalone firmware unit tests pass without regression. |
| **Guition JC-ESP32P4-M3 Physical Panel Touch Walkthrough** | Hardware flash (`esp_preview_main.cpp`) | `MANUAL` | Requires physical Guition JC-ESP32P4-M3 board connected over USB-Serial/JTAG. |

---

## 4. LVGL 9.3.0 Memory & Performance Metrics

Measured using LVGL 9.3.0's built-in TLSF allocator (`LV_MEM_SIZE = 128 KB` matching `smk-p4/sdkconfig.defaults`):

- **Steady-State Free Heap (with `Performance` screen mounted)**: `65,784 bytes` free (`largest_block = 65,336 bytes`)
- **Peak Heap Used Across All 14 Screens & Sub-Tabs**: `74,920 bytes` (`57.1%` of the 128 KB LVGL pool)
- **Memory Leak Delta After 50 Full 14-Screen Cycles (`700` Screen Transitions)**: **`0 bytes`** (`65,784 B -> 65,784 B`)
- **Heap Fragmentation After 700 Screen Transitions**: **`1%`**
- **Post-Shutdown Free Heap**: `117,616 bytes` (`frag = 1%`)

Key design choices responsible for `1%` fragmentation and `0-byte` leak delta:
1. **Single Active Screen Tree**: `UiNavigationManager::unmountCurrent()` destroys the active screen's widget subtree via `lv_obj_clean(content_root_)` before mounting the next screen.
2. **Pre-Mount Global Chrome Refresh**: `StatusStrip` and `BottomNavBar` update their labels/styles while `content_root_` is empty, preventing small label/style allocations from splitting the top of the heap after a screen mounts.
3. **Static Label Buffers in `StatusStrip`**: `patch_buf_` and `voices_buf_` use `lv_label_set_text_static()` + `lv_obj_invalidate()`, performing zero heap allocations during periodic status refreshes.
4. **Automatic Event Context Teardown (`attachLvContext<T>`)**: Every callback struct allocated with `lv_malloc(sizeof(T))` registers an `LV_EVENT_DELETE` hook that calls `lv_free(data)` automatically when its owning `lv_obj_t*` is deleted.

---

## 5. Non-Invasive Integration Proposal for Main Firmware Agent

Per Section 1 ("Arquivos protegidos"), `smk-p4/main/performance_ui.cpp`, `performance_ui.h`, `performance_backend.h`, and `standalone_main.cpp` were **not modified**.

When the main firmware agent is ready to connect `ui_blueprint` to the standalone runtime:

### Step 1 — Link `ui_blueprint` in `smk-p4/main/CMakeLists.txt`
Add `ui_blueprint` to `PRIV_REQUIRES` in `smk-p4/main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS ...
    INCLUDE_DIRS "."
    PRIV_REQUIRES board amy_runtime audio_pcm5102 usb_midi_host ui_blueprint
)
```

### Step 2 — Bridge `AVAILABLE` Parameters to `PerformanceBackend`
Create a thin adapter in `smk-p4/main/` that synchronizes the `AVAILABLE` subset of `UiModel` with `PerformanceBackend`:
- **`perf.patch_id` (`128` / `135`)** $\rightarrow$ `backend.selectProgram(program)`
- **`perf.master_volume` (`0..127`)** $\rightarrow$ `backend.setMasterVolume(vol)`
- **`perf.octave_shift` (`-2..+2`)** $\rightarrow$ `backend.setOctaveShift(oct)`
- **Virtual Keyboard Note On/Off** $\rightarrow$ `backend.noteOn(note, vel)` / `backend.noteOff(note)`
- **Global `PANIC` Button** $\rightarrow$ `backend.panic()`
- **Telemetry Snapshot (`PerformanceState`)** $\rightarrow$ copy `usb_midi_online`, `audio_ready`, `active_voices`, `underruns`, `render_max_us`, `free_internal_ram`, and `free_psram` into `ui.backend().model().diagnostics()` inside the low-priority UI service loop (`10–20 Hz`).

---

## 6. Gate Verification Checklist (Gates A – E)

- **Gate A — Absolute Isolation**: `PASS`
  - Branch `feature/smk-p4-ui-blueprint` created from `feature/smk-p4-standalone-usb-midi` (`0f2598f3`).
  - Zero edits to `src/`, `smk-s3/`, `smk-p4/components/amy_runtime/`, `smk-p4/components/audio_pcm5102/`, `smk-p4/components/usb_midi_host/`, `smk-p4/main/*`, or `smk-p4/platformio.ini`.
  - Main worktree (`feature/smk-p4-standalone-hardware-validation`) completely untouched.
- **Gate B — Complete 14-Screen Navigable Inventory (`800 × 480`)**: `PASS`
  - All 14 screens and sub-tabs implemented, navigable, and verified within `800 × 480` with 1-tap return to `PERFORMANCE`.
- **Gate C — Parameter Model & Feature Honesty**: `PASS`
  - 132 canonical parameters with explicit `AVAIL`, `PLAN`, `N/A`, and `DEMO` badges; simulated meters labeled `[SIM DEMO]`; persistent `UI DEMO` badge in `StatusStrip`.
- **Gate D — Interactive Controls & Zero Memory Leaks**: `PASS`
  - Automated interaction tests + 700-transition LVGL memory stress test (`0 bytes` leaked, `1%` fragmentation) + visual inspection of 20 rendered `800 × 480` PNG screenshots.
- **Gate E — Documentation & Clean Incremental Commits**: `PASS`
  - All 5 required documentation files (`ui_architecture.md`, `ui_screen_inventory.md`, `ui_feature_matrix.md`, `ui_navigation_map.md`, `ui_blueprint_10_results.md`) completed and verified.
