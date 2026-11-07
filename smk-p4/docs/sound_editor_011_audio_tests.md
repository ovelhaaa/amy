# Real-AMY audio tests0.11

Run from repository root (existing gcc/g++/PowerShell toolchain):

```powershell
./smk-p4/tests/run_harmony.ps1
./smk-p4/tests/run_sound_editor.ps1
./smk-p4/tests/compare_editor_factory.ps1 -Baseline ../smk-p4-full-factory-library-010
./smk-p4/ui-preview/run_preview.ps1
./smk-p4/tests/run_sound_editor_ui.ps1
```

Logs/artifacts: build/editor011. Native AMY C objects are built by the first
runner; no miniaudio/physical audio in host. Heap wrappers reject malloc/free/
realloc/calloc in guarded apply/render. Eight-voice state tests validate mapped
oscillators, native OP1..6 order, bounded queues, stale generation/NaN rejection,
1000 edits without service allocation, Harmony chord-group identity and release.
The inherited Harmony/USB/factory regression runs with SOUND_EDITOR enabled,
including687 swaps of all229 admitted IDs, OOM recovery and balanced teardown.

PCM fixture renders deterministic seeded Moving Strings4 / Piano135 at1 and8
voices. It changes controls at block32, releases at64, retriggers at96, releases
at127. Cutoff/Q affect held notes; Attack affects active/future onset; Release
changes release PCM. FM brightness/OP2 level/ratio each produce changed PCM.
Compare/Revert applied before a fresh onset produce byte-identical original PCM.
No clipped samples were observed in these22 representative fixture renders.
This is not exhaustive qualification of every possible override combination.

Seven no-override factory PCM files are byte-identical to baseline0.10:
128 Brass,135 Piano,138 E.Piano,142 Bass,4 Moving Strings,8 Organ,31 Synth Bass.
Fixed conditions and runtime rendering harness are shared by both builds.
No ROM/core/voice-allocation/FX changes. Edited sounds are not automatically added
to the factory qualification list;27 blocked sources stay blocked.

An early state-only Release test missed the envelope flag requirement. The PCM
fixture caught it; bp_is_set now accompanies partial segment writes and physical
voice checks were strengthened. A second test found lossy cutoff Hz/log restore;
captured native deltas now restore exactly. Early failures are not counted PASS.
One inherited test uses compact unbraced formatting; only its misleading-indent
warning is nonfatal in the expanded regression runner. Product/new tests retain
-Wall -Wextra -Werror.

Host wall-clock timing is not P4 render timing. EDIT011 apply_max_us and existing
render/compute/gap/memory/stack counters support hardware measurement. No target
CPU/load/latency/DMA-underrun result or active-play RAM comparison is claimed.
