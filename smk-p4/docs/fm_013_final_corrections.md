# CODEX — SMK-P4 0.13: FM Algorithm Selector — Final Corrections Qualification Report

**Repository:** `ovelhaaa/amy`  
**Active Branch:** `feature/smk-p4-fm-algorithm-selector-013`  
**Base Commit:** `85b68f206f9bdbce1b6d49bb3f08eccff3f123ed` (`feature/smk-p4-fx-state-hardening-0121`)  
**Target Hardware:** Guition ESP32-P4 DevKit (RISC-V dual-core @ 360/400 MHz, 320 KB SRAM, 16 MB Flash, PSRAM, 800×480 touchscreen, PCM5102A I²S, USB MIDI Host)  
**Audio Engine:** AMY, 48 kHz, 256 frames/block (5.333 ms budget), 8 polyphonic voices, 6 operators per voice (48 oscillators)

---

## 1. Executive Summary & Mission Scope

This report documents the resolution and qualification of the four code review deficiencies identified in the SMK-P4 0.13 FM Algorithm Selector feature branch:

1. **Gate A — Indefinite Rejection of Real-Time Operator Edits During Notes:**
   - *Defect:* `touched & edit::bit(edit::FmAlgorithm)` remained set after an initial algorithm edit, causing subsequent live edits to Operator Level, Ratio, Attack, Decay, or Brightness to be rejected during sustained notes even when no algorithm change was requested.
   - *Correction:* Disentangled `algorithm_present_in_edit`, `algorithm_target`, `algorithm_current`, and `algorithm_change_required`. Active notes and sustain pedal now strictly block *real topology changes*, while freely allowing continuous operator editing, Brightness adjustment, and re-transmission of the active algorithm. Compare and Revert reject atomically without partial parameter corruption.

2. **Gate B — UI Algorithm Selector Confirmation & Error Feedback:**
   - *Defect:* The touchscreen algorithm modal closed immediately on click before backend confirmation, and errors were reported with generic notices.
   - *Correction:* The modal remains open upon immediate backend rejection, tracks `pending_algo`, reflects owner confirmation on `LV_EVENT_REFRESH`, and displays the specific diagnostic notice (`"RELEASE NOTES TO CHANGE FM ALGORITHM"`).

3. **Gate C — Real Graphical LVGL Operator Connections:**
   - *Defect:* Operator boxes were displayed in a line with textual formulas, lacking visual routing lines.
   - *Correction:* Implemented `fm::generateLayout()` and preallocated 20 reusable `lv_line` objects in LVGL 9.3 rendering modulation connections (Amber), audio routes (Cyan), and feedback loops (Orange) with zero per-frame heap allocations.

4. **Gate D & E — End-to-End Persistence & Deterministic PCM Qualification:**
   - *Defect:* Missing comprehensive Save -> Reboot -> Recall test verifying 8 voices in AMY, and PCM diff assertion needed strict bit-exact determinism.
   - *Correction:* Verified complete NVS v2 persistence with 8 voices restored in AMY (Algorithm 15, Op Level 85%, Op Ratio 3.0, and Multi-FX), legacy V2 preset compatibility (inheriting ROM algorithm without defaulting to 1), and bit-exact Compare/Revert PCM determinism (`revert_diff == 0`).

---

## 2. Gate Qualification Status Matrix

| Gate | Description | Status | Verification Command / Artifact |
|:----:|:------------|:------:|:--------------------------------|
| **Gate 0** | Clean git reference & baseline validation | `PASS` | `git status`, `git branch --show-current`, baseline test run |
| **Gate A** | Active note guard correction (live op edits allowed, real topology change blocked) | `PASS` | `powershell -File smk-p4/tests/run_sound_editor.ps1 -FmOnly` (Suite 8) |
| **Gate B** | UI algorithm selector owner confirmation & exact diagnostic notice | `PASS` | `powershell -File smk-p4/tests/run_sound_editor_ui.ps1` |
| **Gate C** | Real graphical LVGL 9.3 operator wiring (Modulation, Audio, Feedback) | `PASS` | `powershell -File smk-p4/tests/run_sound_editor.ps1 -FmOnly` (Suite 7), `run_sound_editor_ui.ps1` |
| **Gate D** | End-to-end FM parameter persistence (Save -> Reboot -> Recall, 8 voices in AMY) | `PASS` | `powershell -File smk-p4/tests/run_sound_editor.ps1 -FmOnly` (Suite 8) |
| **Gate E** | Deterministic Compare & Revert PCM assertions (`revert_diff == 0`, `diff_algo10 > 50000`) | `PASS` | `powershell -File smk-p4/tests/run_sound_editor.ps1 -FmOnly` (Suites 5 & 6) |
| **Gate F** | Audio benchmarks with clear host vs. target hardware distinction | `PASS` | `smk-p4/docs/fm_013_audio_benchmarks.md`, `run_sound_editor.ps1 -BenchOnly` |
| **Gate G** | Full regression suite (Sound Editor, UI Blueprint, FX All) | `PASS` | `run_sound_editor.ps1`, `run_sound_editor_ui.ps1`, `run_fx_tests.ps1 all` (all exit code 0) |
| **Gate H** | PlatformIO firmware candidate build (`guition-editor-8` & `guition-editor-8-bench`) | `PASS` | `smk-p4/docs/fm_013_hardware_candidate.md` |
| **Hardware** | Physical flashing and live bench telemetry verification (COM13) | Historical report | Git records cite COM13 telemetry. See the evidence note below. |

---

## 3. Firmware Candidate Artifacts & Checksums

Both candidate firmware environments compiled cleanly using PlatformIO with ESP-IDF 5.5.2:

| Environment | Binary File Path | File Size | Flash Usage | RAM Usage | SHA-256 Digest |
|:------------|:-----------------|:---------:|:-----------:|:---------:|:---------------|
| **`guition-editor-8`** | `smk-p4/.pio/build/guition-editor-8/firmware.bin` | 1,234,016 B | 1,233,290 B (7.4%) | 39,896 B (12.2%) | `B33A8CF17F38A9752DAE06A91AB116BBA880893EB93C50222EA32F7F8464117F` |
| **`guition-editor-8-bench`** | `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin` | 1,334,656 B | 1,333,940 B (8.0%) | 39,736 B (12.1%) | `DC41479E5A2E33F7E3FAD436505D00080FB96B52A31A53A3C1F0E96B313FB1E5` |
| **Bootloader** | `smk-p4/.pio/build/guition-editor-8/bootloader.bin` | 23,120 B | — | — | `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90` |
| **Partitions** | `smk-p4/.pio/build/guition-editor-8/partitions.bin` | 3,072 B | — | — | `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501` |

---

## 4. Upstream AMY Integrity & Invariants

1. **Upstream AMY Core:** Zero modifications. `src/algorithms.c`, `src/oscillators.c`, `src/amy.c`, and all public headers remain 100% upstream pristine.
2. **Preset Safety:** NVS partition tables and user presets v1/v2 remain intact. Legacy presets retain their patch's native ROM algorithm.
3. **Hardware evidence:** The 0.13 COM13 report is historical. See the evidence note below.

Historical 0.13 figures are scoped to the artifacts cited in this document. See the [hardware evidence record](stability_0131_hardware_evidence.md) for provenance and the current 0.13.1 smoke status.
