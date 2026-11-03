# SMK-P4 UI Blueprint 1.0 — Isolated Preview & Test Harness

This directory provides an isolated verification and visual preview environment for `smk-p4/components/ui_blueprint` without modifying any protected firmware, audio, USB-MIDI, or `smk-p4/main/` files.

## 1. Host Verification & 800×480 Screenshot Capture (`run_preview.ps1`)

Compiles genuine **LVGL 9.3.0** (`managed_components/lvgl__lvgl`) and `smk-p4/components/ui_blueprint` on the host with `-Wall -Wextra -Werror`, runs the automated test suite (`blueprint_test_suite.exe`), and renders 20 `800×480` RGB565 screenshots across all 14 screens and sub-tabs into `smk-p4/ui-preview/screenshots/*.png`.

```powershell
powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1
```

### What `blueprint_test_suite.exe` Verifies
1. **Parameter Registry & Model Integrity**:
   - All 132 canonical parameters (`perf.*`, `osc.*`, `fm.*`, `filter.*`, `amp.*`, `env.*`, `lfo.*`, `mod.*`, `layer_a.*`, `layer_b.*`, `mixer.*`, `fx.*`, `arp.*`, `seq.*`, `drums.*`, `harmony.*`, `midi.*`, `sys.*`) have valid ranges, defaults, units, and explicit `FeatureStatus` (`AVAILABLE`, `PLANNED`, `UNAVAILABLE`, `DEMO`) backed by repository evidence.
   - Clamping, normalization, resetting to default, and family-dependent capability status (`FM` vs `Subtractive` vs `Additive/Wavetable` vs `Bypass`) work deterministically.
2. **Musical Assist & Patch Catalog**:
   - Distinguishes P4 Available ROM patches (`128: Juno A60 Strings`, `135: DX7 Rhodes Mark I`) from S3/AMY Planned ROM patches and User Demo slots.
   - Verifies Scale Quantizer and Chord Assist calculations and RAM-only Panic reset.
3. **14-Screen Navigation & 1-Tap Return**:
   - Navigates through all 14 screens, verifies sub-tab persistence (`SOUND EDITOR` 7 tabs, `EFFECTS` 4 tabs), history stack (`navigateBack()`), and 1-tap `returnToPerformance()`.
4. **800×480 Bounding-Box Audit**:
   - Traverses the active LVGL widget tree on every screen and sub-tab to ensure zero non-scrollable widgets exceed the `800×480` horizontal viewport.
5. **LVGL Heap Leak Stress Test**:
   - Cycles through all 14 screens 50 times (`700` screen builds/teardowns) and verifies via `lv_mem_monitor()` that `0 bytes` are leaked (`used_after == used_before`).

## 2. Target Hardware Preview Entry Point (`esp_preview_main.cpp`)

`esp_preview_main.cpp` shows how `smk::p4::ui::UiBlueprint` mounts on the Guition JC-ESP32P4-M3 (`ST7701S + GT911`, `800×480` RGB565) reusing `smk::p4::beginDisplayTest(false)` without duplicating the display/touch HAL or initializing audio/USB.
