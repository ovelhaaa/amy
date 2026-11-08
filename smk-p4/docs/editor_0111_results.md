# 0.11.1 investigation — incomplete

Baseline: dd491f039561771385b2672c86ca3a315adca8bf. This is not a 0.11.1 release report and no new synthesis parameter is qualified.

## Findings

* `UiBlueprintPerformanceAdapter::editIndex` exposes cutoff, resonance, attack, release, FM depth, operator level and ratio. The owner additionally checks the patch's materialized eight-voice map. The registry alone is not the runtime authority.
* Registry `amp.vel_sens`, `lfo.1.rate_hz` and `lfo.1.depth` claim Available but the adapter has no matching edit command. Modulation slot source and destination also claim Available in the registry although the matrix only changes `UiModel`.
* The live editor's MOD widget previously marked the first mod-wheel-to-pitch row Available based solely on model values and accepted model-only changes. The fix disables those buttons for the real editor and labels the routes Planned; the independent CC1 vibrato is unchanged. The demo backend retains its prior behavior.
* User preset v1 uses fixed 128-byte records, 17 float positions, dual keys, CRC and ROM discriminator. Existing 24 KiB NVS admission assumes seven entries per blob and a 126-entry GC reserve; actual free capacity has not been measured. Extending `Count` without changing this encoding overlaps its ROM and CRC fields, so no new persistent parameters should be enabled before a separately qualified codec and non-destructive storage strategy exist.
* The current Juno capture recognizes exactly three breakpoints but checks envelope topology on the first voice only. New ADSR controls require validating all voice breakpoints and native restoration, not merely adding a slider.
* Prior hardware receipt records a 0.11 image written to the identified board and successful readback, not the current device state. Existing rollback artifacts are in the 0.11 worktree's `build/editor011/flash`; their continued presence and hashes have not been verified in this investigation.

## Validation and remaining work

Baseline `run_harmony.ps1` and `run_sound_editor.ps1` passed in the preceding session. These cover existing controls, not the 0.11.1 objectives. A full inventory, P0/P1 integration, versioned persistence, functional tests, target builds, current backup and physical candidate are outstanding. No NVS, firmware, AMY core or hardware driver was modified. P4 hardware and 800x480 request takes precedence for this task over the older S3-specific specification; no S3 behavior is changed.

Current change validation: PASS direct compilation of `modulation_matrix.cpp` using g++ C++20, warnings as errors and the pinned LVGL headers. This is not a widget interaction test. Full preview and on-device interaction have not been qualified.
PASS `./smk-p4/ui-preview/run_preview.ps1` and `./smk-p4/tests/run_sound_editor_ui.ps1`: existing preview and live-editor host suites. They do not specifically assert the disabled MOD buttons or constitute physical validation.
