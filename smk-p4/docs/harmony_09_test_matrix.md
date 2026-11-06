# Harmony 0.9 validation matrix

Commands run from the isolated worktree root. Logs/artifacts in `build/harmony09/`.

|Status|Command|Evidence/scope|
|---|---|---|
|PASS|`./smk-p4/tests/run_harmony.ps1` (later `-ReuseObjects` for unchanged AMY C)|Independent scale-degree tables;11 scales x12 keys x128 notes x6 quantizers; chords/degrees/inversions/range; gap classes; actual AMY source/channel/sustain/Touch lease/bend/CC1/atomic capacity;1000 two-triad cycles; full32 Touch tetrad retriggers; allocation guards; seven required patches; USB/retrigger regressions|
|PASS|`./smk-p4/tests/compare_harmony_off.ps1 -Baseline ../smk-p4-unified-patch-engine-08`|Seven patch PCM outputs,128 blocks each, byte-identical to preserved0.8 runtime (same unchanged AMY sources)|
|PASS|`./smk-p4/tests/run_harmony_ui.ps1`|Actual LVGL9.3 + AMY;14 screens; requested vs confirmed config; live key duplicate-press/Click guard and Off; planned-value rejection/stepper skip; channel16; canceled command timeout; two screenshots inspected|
|PASS|`./smk-p4/ui-preview/run_preview.ps1`|Original Blueprint contract,700 mounts without LVGL leaks,20 rendered previews|
|PASS|`./smk-p4/tests/run_host.ps1`|Boot, display geometry/dirty regions, PCM conversion/driver errors,29 Python tests|
|PASS|`./smk-p4/tests/run_usb.ps1`|Mock IDF host descriptors/enumeration/reconnect/in-flight cleanup|

Final target builds/audits and legacy profile results are recorded in the results
report and candidate manifest. Tests are software evidence only; target timing,
heap fragmentation, physical reconnect, analog continuity and all auditory rows
remain MANUAL. The complete sustained stability window remains PENDING.

A first host harness build missed `<initializer_list>`; a temporary preprocessor
newline typo was corrected. First target UI build hit `-Werror=format-truncation`;
summary field precisions were bounded. The successful final reruns supersede
those failures; no warning was disabled. Raw first target failure log is retained.
