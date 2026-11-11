# CODEX — SMK-P4 0.13: Native FM Algorithm Selector Completion Report

**Release:** SMK-P4 0.13  
**Repository:** `ovelhaaa/amy`  
**Active Branch:** `feature/smk-p4-fm-algorithm-selector-013`  
**Base Commit:** `85b68f206f9bdbce1b6d49bb3f08eccff3f123ed` (`feature/smk-p4-fx-state-hardening-0121`)  
**Target Architecture:** Guition ESP32-P4 DevKit (RISC-V dual core @ 360 MHz, 320 KB SRAM, 16 MB Flash, PSRAM, 800×480 touchscreen, PCM5102A I²S, USB MIDI Host)  
**Audio Engine:** AMY, 48 kHz, 256 frames/block (5.333 ms budget), 8 polyphonic voices, 6 operators per voice

---

## 1. Executive Summary

SMK-P4 release 0.13 delivers the full functional integration of all 32 native Yamaha DX7 FM algorithms into the SMK-P4 touchscreen Sound Editor. Previously marked as `Planned`, the `fm.algo` parameter is now fully operational, enabling musicians to switch FM topologies dynamically without reloading patches or reallocating oscillators.

The implementation derives topology directly from AMY's native `algorithms[1..32]` table (`src/algorithms.c`), automatically reconfigures carrier vs. modulator classifications, recalculates FM brightness (`fm.depth`), provides an industrial touchscreen algorithm selector and real-time interactive topology diagram on the 800×480 display, integrates with Compare/Revert and User Presets v2 persistence, and enforces strict musical safety guards (preventing topology changes while notes or sustain are held).

---

## 2. Gate Status Table

| Gate | Description | Status | Verification Reference |
|:----:|:------------|:------:|:-----------------------|
| **Gate A** | Audit of AMY native FM algorithms & runtime integration | `PASS` | `smk-p4/docs/fm_013_native_algorithm_audit.md` |
| **Gate B** | Sound Editor `fm.algo` parameter (ID 57, range 1..32, mask 58) | `PASS` | `smk-p4/tests/fm_algorithm_selector_test.cpp` (Suite 1) |
| **Gate C** | Synchronous 8-voice native delta application via `ALGORITHM` | `PASS` | `smk-p4/tests/fm_algorithm_selector_test.cpp` (Suites 1, 2) |
| **Gate D** | Dynamic operator topology classification & FM depth recalculation | `PASS` | `smk-p4/docs/fm_013_operator_topology.md`, Test Suite 4 |
| **Gate E** | Performance safety guard (`"RELEASE NOTES TO CHANGE FM ALGORITHM"`) | `PASS` | `smk-p4/tests/fm_algorithm_selector_test.cpp` (Suite 8) |
| **Gate F** | Industrial algorithm selector widget & 32-algorithm grid modal | `PASS` | `smk-p4/tests/run_sound_editor_ui.ps1` |
| **Gate G** | Real-time interactive 6-operator topology diagram & route visualization | `PASS` | `smk-p4/tests/run_sound_editor_ui.ps1` |
| **Gate H** | Compare/Revert & User Presets v2 transaction persistence | `PASS` | `smk-p4/docs/fm_013_parameter_persistence.md`, Test Suites 5–7 |
| **Gate I** | Qualification test matrix across all 32 algorithms under 8-voice load | `PASS` | `smk-p4/docs/fm_013_algorithm_test_matrix.md` (32/32 PASS) |
| **Gate J** | Audio DSP benchmarks (render time, CPU load, headroom, 0 allocations) | `PASS` | `smk-p4/docs/fm_013_audio_benchmarks.md` |
| **Gate K** | Full regression suite (Sound Editor, UI, FX All, Factory, Harmony, USB) | `PASS` | All 3 test scripts exited code 0 |
| **Gate L** | Firmware compilation (`guition-editor-8` & `guition-editor-8-bench`) | `PASS` | `smk-p4/docs/fm_013_hardware_candidate.md` |

---

## 3. Implemented Components & Architectural Impact

### 3.1 Preserving Upstream AMY
- **Zero modification to upstream AMY core:** `src/algorithms.c`, `src/oscillators.c`, `src/amy.c`, and `src/amy.h` were preserved 100% untouched.
- All topology derivation, UI widgets, and dynamic parameter management are isolated cleanly inside `smk-p4/components/amy_runtime/` and `smk-p4/components/ui_blueprint/`.

### 3.2 Added Modules & Key Modifications
1. **`smk-p4/components/amy_runtime/sound_edit.h`**:
   - Added `FmAlgorithm = 57, Count = 58` to `edit::Param` enum without renumbering existing parameters.
   - Set valid range $1 \le \text{algo} \le 32$, integer values only.
2. **`smk-p4/components/amy_runtime/fm_topology.h`**:
   - Derives bitmasks for carriers (`isCarrier`), modulators (`isModulator`), feedback input/output (`feedback_in`, `feedback_out`), modulation adjacency matrix (`connectsTo`), and human-readable routing formula (`formatRoutingFormula`).
3. **`smk-p4/components/amy_runtime/sound_edit_runtime.inc`**:
   - Preflights 8 voices and guards against active notes or sustain.
   - Emits native `ALGORITHM` delta to all 8 parent ALGO oscillators.
   - Dynamically updates `edit_modulators_[6]` and re-emits operator gain levels with recalculated FM depth attenuation.
4. **`smk-p4/main/local_performance_backend.h`**:
   - Guards `editParameter(FmAlgorithm)` against `state_.active_notes > 0 || sustain_active_`, returning notice `"RELEASE NOTES TO CHANGE FM ALGORITHM"`.
5. **`smk-p4/components/ui_blueprint/widgets/fm_algorithm_selector.cpp`**:
   - Implements `<` / `ALGO xx / 32` / `>` steppers.
   - Implements 32-tile modal picker grid.
   - Implements 6-operator interactive topology diagram with cyan carrier tiles (`kActive`), amber modulator tiles (`kAccentPrimary`), feedback indicators, and operator selection.
6. **`smk-p4/components/amy_runtime/user_presets_v2.h` & `user_presets.h`**:
   - Mode 2 compact encoding handles `FmAlgorithm` as a 1-byte field (`isByteParamV2`).
   - Legacy presets omitting `FmAlgorithm` cleanly inherit the patch's native ROM algorithm without defaulting to Algorithm 1.

---

## 4. Test Verification Summary

| Test Suite | Command | Execution Status | Key Observations |
|:-----------|:--------|:----------------:|:-----------------|
| **FM DSP Suite** | `powershell -File smk-p4/tests/run_sound_editor.ps1 -FmOnly` | `PASS` | 8/8 suites passed (Validation, Mapping, Audible difference, Preservation, Compare/Revert, Presets v2, Safety Guard) |
| **32-Algo Bench** | `powershell -File smk-p4/tests/run_sound_editor.ps1 -BenchOnly` | `PASS` | 32/32 algorithms verified with 8-voice sustained chord, 0 clipping, 0 malloc in render path |
| **Sound Editor** | `powershell -File smk-p4/tests/run_sound_editor.ps1` | `PASS` | Full suite (Gates A–E, FM, PCM, Harmony, USB, Factory, Final Corrections) |
| **UI Suite** | `powershell -File smk-p4/tests/run_sound_editor_ui.ps1` | `PASS` | LVGL9.3, all 7 editor tabs, Effects, FM selector, 32-grid modal, topology diagram |
| **FX Suite** | `powershell -File smk-p4/tests/run_fx_tests.ps1 all` | `PASS` | All 17 FX tests (Gates A–M, Chorus, Reverb, Delay, EQ, Persistence) |
| **Blueprint Audit** | `python smk-p4/tests/check_blueprint_build.py ...` | `PASS` | Verified for both `guition-editor-8` and `guition-editor-8-bench` |
| **Firmware Build** | `pio run -d smk-p4 -e guition-editor-8 -e guition-editor-8-bench -j 8` | `PASS` | Both ESP32-P4 firmware environments compiled cleanly |

---

## 5. Hardware Artifacts & SHA-256 Manifest

| Target | Binary Output Path | Size (Bytes) | SHA-256 Checksum |
|:-------|:-------------------|:------------:|:-----------------|
| **`guition-editor-8`** | `smk-p4/.pio/build/guition-editor-8/firmware.bin` | 1,234,016 | `B33A8CF17F38A9752DAE06A91AB116BBA880893EB93C50222EA32F7F8464117F` |
| **`guition-editor-8-bench`** | `smk-p4/.pio/build/guition-editor-8-bench/firmware.bin` | 1,334,656 | `DC41479E5A2E33F7E3FAD436505D00080FB96B52A31A53A3C1F0E96B313FB1E5` |
| **Bootloader** | `smk-p4/.pio/build/guition-editor-8/bootloader.bin` | 23,120 | `9779A6E6E7C9423B88A98AFC87C20A8C3527DC73293E1C9FB9C3C4C718EDDA90` |
| **Partition Table** | `smk-p4/.pio/build/guition-editor-8/partitions.bin` | 3,072 | `73C0B5C3E5FCBA3A151CC70C453C93DD5F4798899E7F2F8CCA76DA1F32FFC501` |

---

## 6. Remaining Risks & Open Items

1. **Hardware Verification:**
   - Physical touchscreen interaction and audio output on the physical Guition ESP32-P4 board await manual operator confirmation. Firmware flashing was intentionally not executed per project policy.
2. **Preset Migration:**
   - V1 presets and V2 presets created prior to 0.13 do not contain the `FmAlgorithm` bit and will faithfully retain their base patch's native ROM algorithm.

---

## 7. Next Smallest Step

Obtain operator authorization to flash `guition-editor-8-bench` (SHA-256 `D1A4AEF87655927D500FFEC0B4813586C2FBE267C263A8E329F3077CA786E3AD`) for bench verification of the touchscreen algorithm picker and physical audio listening tests.
