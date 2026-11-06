# SMK-P4 Harmony Assist 0.9 - software candidate

Subsequent authorized application write/readback and idle boot: **PASS**.
See [physical follow-up](harmony_09_hardware_qualification.md). The software
candidate report below retains its preflash statuses as historical evidence;
musical smoke and sustained active-play qualification remain pending.

Branch: `feature/smk-p4-harmony-assist-09`. Separate worktree:
`build/worktrees/smk-p4-harmony-assist-09`. Baseline0.8 branch remains at
`f40f34b4cd74e90231b5c604786ca511121a6df7`. Original working tree unchanged.
Preserved audible0.8 application SHA256 independently checked read-only:
`a5b410c29b677fcdc894ee78b1f2a22c64d98594143f460edd7800a4965e8617`.
No reset, force-push, dependency upgrade, serial access or firmware upload.

## Implemented

Real eleven-scale quantization and fixed/diatonic triads/sevenths before final
voice allocation; inversions, close/basic octave opening; USB channel eligibility
and independent Touch eligibility. Safe boot has both features OFF. Five RAM
presets. Whole-group admission within eight gates, independent overlapping voices,
original-input ownership, frozen birth configuration, sustain/NoteOn0/retrigger,
Touch tokens and fixed lease, channel bend/CC1, Panic/disconnect/patch barriers.
Live Harmony controls and virtual keyboard, owner confirmation, predicted vs last
admitted result, capacity/rejection/error display. PCM fallback classification
and musical timing maxima. Full rules in [architecture](harmony_09_architecture.md)
and [ownership](harmony_09_note_ownership.md).

Planned: Add9/9th, Drop2/complex voicings, accidentals/velocity policies, Layer B,
NVS, MIDI OUT, sequencer/arpeggiator, extra patches or voice stealing. Their
availability is not implied by the new live controls. Third inversion requires
a tetrad. Diatonic configuration rejects non-seven-note scales. High chords fold
down as a whole by octaves; no individual-note clamp/wrap. The pentatonic-major
inherited mask error was corrected in the shared P4 table and tested independently.

## Architecture impact

No AMY shared core, S3, shared protocol, I2S/USB/display driver, electrical pin,
master gain, DSP, FX or dependency changes. The existing worker owns all group
state. Input queues and service budgets unchanged; notes are transformed once,
not multiplied into synthetic queued commands. UI receives copied applied state.
Changes span small commits for PCM diagnosis, math, ownership, integration/UI,
regressions and packaging. Code commits before this report:

```text
954d676d test(p4): assert corrected scale pitch reaches the AMY instrument
04e7729e test(p4): measure full-channel expressive control bursts
d8b5c0c3 test(p4): clarify complete chord release-tail assertion
62aba537 test(p4): verify harmony ownership PCM identity and real LVGL controls
8485aa89 feat(p4): connect applied Harmony state and live Touch keyboard
a1b189b3 feat(p4): admit bounded harmony groups with source and input ownership
b56c020e feat(p4): share deterministic scale and chord math with preview
93d575d5 feat(p4): classify PCM fallback by owner state and generation
```

Final documentation/candidate commit is available through
`git log f40f34b4..feature/smk-p4-harmony-assist-09`. The review manifest freezes
its precise source commit. Compiled project version may precede documentation-only
commits; manifest product source hashes and image freshness checks bind the actual
product files independently of that version string.

## Validation - executed commands

|Status|Command/scope|
|---|---|
|PASS|`./smk-p4/tests/run_harmony.ps1` then `-ReuseObjects` after C++ changes (AMY C unchanged): pure math, actual AMY group ownership/atomic capacity/sustain/retrigger/lease/bend/CC1, no service allocation, seven required patches, USB and retrigger suites|
|PASS|`./smk-p4/tests/compare_harmony_off.ps1 -Baseline ../smk-p4-unified-patch-engine-08`:128 blocks of single-note stereo PCM for128/135/138/142/4/8/31, byte-identical to0.8|
|PASS|`./smk-p4/tests/run_harmony_ui.ps1`: actual LVGL9.3 and AMY,14 screens, owner-confirmed config, real Harmony key and complete Off, unsupported options/stepper skip, channel16, canceled-config timeout; two images visually inspected|
|PASS|`./smk-p4/tests/run_unified_probe.ps1`: existing0.8 real AMY strategies/candidates/runtime2000 swaps, failure/retry/OOM injection, ownership, concurrent PCM10000 blocks and USB/retrigger regressions|
|PASS|`./smk-p4/tests/run_unified_ui.ps1`: existing15-patch/14-screen real LVGL integration, load failures/retry and Panic|
|PASS|`./smk-p4/tests/run_amy.ps1 -Voices 4`, `-Voices 6`, `-Voices 8`: legacy AMY/USB/ownership/polyphony/retrigger/backend/UI mock suites|
|PASS|`./smk-p4/ui-preview/run_preview.ps1`: original Blueprint contracts,700 mounts without LVGL leaks,20 frames|
|PASS|`./smk-p4/tests/run_host.ps1`: boot/display/PCM/driver tests and29 Python tests|
|PASS|`./smk-p4/tests/run_usb.ps1`: USB host descriptor/lifecycle/in-flight disconnect/reconnect mocks|
|PASS|`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-harmony-8 -j 8`|
|PASS|`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-harmony-8-bench -j 8`|
|PASS|`python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --environment guition-harmony-8` and bench environment with `--musical`|
|PASS|`git diff --check`; diff of `src/`, `smk-s3/`, `shared/` against0.8 empty|
|NOT RUN|Full upstream `make test`, S3 target rebuild, physical firmware upload/readback, physical timing/latency/memory/fragmentation and analog continuity|
|MANUAL / NOT RUN|Short operator smoke and Gate0 physical classification; prolonged qualification PENDING separately|

Final logs in `build/harmony09/`. The matrix describes scope and corrected first
attempts: [test matrix](harmony_09_test_matrix.md). Intentional injected OOM messages
in regression logs are expected and followed by assertions of safe failure/recovery.
No test is promoted from compiled software to physical PASS.

## Host measurements and memory

Last recorded host run: two-triad apply maximum27us, render maximum84us;
transform maximum11us (microsecond resolution), admission maximum147us across the
expanded scenarios. A full USB eight-voice channel with32 CC1 or bend commands per
block measured1179us max apply. Distinct per-instrument note updates are necessary
to preserve channel/source pitch; no second event transform. These are desktop
wall-clock observations, not P4 CPU load, deadline, latency or timing guarantees.

1000 two-triad cycles: zero tracked allocation-call delta and stable requested
heap215068B. Full32-command Touch tetrad retrigger and eight-voice expressive
bursts also pass allocation guards. Runtime object5540B on the host ABI. Existing
unified-profile run reports165770B requested AMY heap (3000-node reserve); Harmony
uses6072 nodes (three blocks), trading an extra preallocated block for bounded
service. These are allocator-request points, not total resident process memory or
P4 heap readings. Target free/largest internal and PSRAM under changes remain
MANUAL. Physical boot admission still fails safely if contiguous/free guards fail;
new reserve reduces spare heap and must be checked in the smoke.

## Concrete review images

|Profile|Application bytes|SHA256|Static DRAM bytes vs0.8|IRAM bytes|
|---|---:|---|---:|---:|
|guition-harmony-8|1117712|`9794834c12954fd9f2443b7a06e595f72251eb17d2f780afad7775be35a6b8bb`|172076 (+644)|96640 (unchanged)|
|guition-harmony-8-bench|1218592|`ebcbc081d287958e466a22fb063c22d4dd2542f59c9cc7758c4f1c17ed6f3f9c`|172508 (+644)|101888 (unchanged)|

Safe image is7008B larger; bench image7200B larger than preserved0.8.
Both retain128KiB LVGL,48kHz stereo PCM16,256 frames, four DMA descriptors,
eight gates, USB64 storage and32 USB commands/block. Safe electrical gates OFF;
bench uses the previously documented GPIO50/51/52 and USB HS0. Those are existing
bench assumptions, not newly assigned final hardware. Reconfirm board/port,
VBUS/power, PCM straps and installed firmware before any authorization/write.

## Gaps, limits and acceptance

Gate0 instrumentation PASS; physical cause attribution MANUAL / NOT RUN. Old145
fallback blocks/66 barriers cannot be retroactively classified;79 remain without
individual evidence. New GAP09 separates boot, patch, Panic, stale, unexpected
READY-active starvation and OTHER. Zero I2S errors never establishes gaplessness.
See [gap classification](output_08_gap_classification.md).

|Gate|Decision|
|---|---|
|A Quantizer|PASS software: real AMY pitches and exhaustive math|
|B Chords|PASS software: fixed and real diatonic triads/sevenths|
|C Musical safety|PASS host ownership/sustain/source/channel/Panic/generations; physical MANUAL|
|D Polyphony|PASS host eight slots, complete admission/rejection, no stealing|
|E Interface|PASS real LVGL/AMY applied state and keyboard; physical MANUAL|
|F Real time|PASS allocation guards/PCM OFF equivalence; physical gaps/deadlines/latency NOT RUN|
|G Hardware|MANUAL / NOT RUN; prolonged qualification PENDING|

This completes a software candidate for evaluation, not physical acceptance of
all gates. No demonstrated sustained starvation cause was available to fix;
no speculative scheduler change was introduced. Gate0 physical and operator
listening must pass before personal rehearsal classification. Release tails are
not counted as occupied gates (same0.8 policy); LAST ADMITTED is historical,
keyboard colors remain prediction, and snapshots can coalesce fast transitions.
No professional-stage qualification, gapless patch switch or latency claim.

## Candidate bundle, rollback and next smallest step

`python smk-p4/tests/package_harmony.py` requires a clean source tree, reruns both
build audits, checks product-file freshness, hashes source/images/config/ELF/map,
collects raw logs/PCM/screenshots and freezes a source ZIP/change patch/manifest.
`flash_authorized=false`, `device_backup_verified_in_this_task=false`, hardware
smoke NOT RUN and prolonged qualification PENDING are explicit. The preserved0.8
application is included after exact SHA verification; original full-flash/rollback
artifacts remain in the0.8 worktree. Do not substitute this historical image for
a fresh backup of the actual installed device. Nothing is pushed or uploaded.

Next smallest step: operator reviews the exact bench SHA above, board/port and a
fresh verified backup, then explicitly authorizes that application image if desired.
Run the [short smoke](harmony_09_hardware_smoke.md), recording each result and
GAP09 deltas. If unexpected READY-active gaps appear, isolate that demonstrated
cause before declaring the candidate usable. A thirty-minute campaign is not
required automatically; prolonged qualification stays separately PENDING.
