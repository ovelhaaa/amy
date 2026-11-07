# Factory library 0.10 — software candidate results

Subsequent authorized application write/readback and observed boot/idle: **PASS**. See [physical follow-up](factory_library_010_hardware_qualification.md). Musical smoke remains MANUAL / NOT RUN. The report below preserves its original preflash software-candidate statuses.

Branch `feature/smk-p4-full-factory-library-010`, isolated worktree `build/worktrees/smk-p4-full-factory-library-010`, baseline Harmony0.9 `2551d0d0`. The original worktree, earlier branches, binaries and recovery bundles remain intact. No destructive reset, force-push, automatic merge, dependency upgrade or firmware write. The explicit ESP32-P4/800x480 task supersedes the older S3/284x76 specification only in this isolated P4 application.

## Implemented and architecture impact

- 256 unique original ROM IDs inventoried and tested with actual AMY;229 admitted experimental host candidates,27 blocked (24 hard-clipping cases and three release-tail reviews). All256 are visible; only admitted IDs are selectable. Every exclusion is named in the generated matrix/JSON. This is not256 functional timbres or completed physical qualification.
- Offline deterministic generator validates ROM hash, ID uniqueness/completeness, names, actual family/cost, local oscillator/operator references and the reviewed command alphabet. Const metadata and generated runtime/UI projections allocate no instruments. State flags distinguish ROM/host/support/unavailable/failure/hardware evidence. Historical DX7 ROM1–4 provenance is unresolved; AMY DX7 is used instead of inventing bank identities.
- Eight reusable AMY instruments, eight shared musical voices and64 oscillators; same worker-only reconstruction/barrier/verification, independent PCM task, boot guards,128KiB LVGL and source/channel ownership. AMY shared core, S3, shared protocol, Harmony math, USB/I2S/display drivers, FX, master gain and pin definitions have no diff.
- Existing industrial Blueprint browser, all14 screens, six visible rows/page, banks/categories/RAM favorites and filtered live Prev/Next. Applied patch stays confirmed until owner success; busy requests are rejected in both adapter and local backend. No crossfade, new UI subsystem, NVS, samples or resident-bank multiplication.

## Validation — commands actually executed

|Status|Command/scope|
|---|---|
|PASS|`./smk-p4/tests/run_harmony.ps1` baseline and `-ReuseObjects` final: real AMY groups, eight voices, sustain, ownership, channel bend/CC1, capacity, no service allocations, USB/retrigger|
|PASS|`./smk-p4/tests/run_unified_probe.ps1` baseline and final: reusable strategies,2000 runtime swaps, partial/OOM/cancel/disconnect recovery, concurrent PCM10000 blocks and retrigger|
|PASS|`./smk-p4/tests/run_factory_probe.ps1`:256 IDs,1/4/8 notes, four registers, three velocities, releases/Panic/maps/OOM/guarded render and768 loads with stable warmed memory|
|PASS|`./smk-p4/tests/run_factory_probe.ps1 -Observe`: real AMY build-only pre-conversion observer for all256 IDs; production core unchanged|
|PASS|`./smk-p4/tests/run_factory_probe.ps1 -StressOnly`:512 additional interleaved loads after64-osc warmup|
|PASS|`./smk-p4/tests/run_factory_probe.ps1 -TimingOnly`:58 otherwise-safe wall-clock outliers rechecked with no concurrent firmware build, maximum2314us; original measurements retained|
|PASS|`python smk-p4/tests/summarize_factory_library.py`; `python smk-p4/tests/generate_factory_library.py --check`; `python -m unittest discover -s smk-p4/tests -p test_factory_library.py` (five integrity/negative tests)|
|PASS|`./smk-p4/tests/run_factory_runtime.ps1`:all229 admitted patches,687 swaps, Harmony/sustain/old-group barriers, eight notes, disconnect/reconnect, busy/invalid/failed IDs, Panic-during-load and disconnect-during-load, injected OOM recovery, no cumulative growth, balanced teardown|
|PASS|`./smk-p4/tests/run_factory_ui.ps1`:real LVGL9.3/AMY, all14 screens/all229 selections,256-entry catalog,44 forward/back page actions, bank/category model/favorites/filtered PrevNext, requested/applied/busy/failure/retry and heap integrity; two screenshots visually inspected|
|PASS|`./smk-p4/tests/run_unified_ui.ps1`; `./smk-p4/tests/run_harmony_ui.ps1`; `./smk-p4/ui-preview/run_preview.ps1` (original Blueprint contracts,700 mounts,20 captures)|
|PASS|`./smk-p4/tests/run_host.ps1`; `./smk-p4/tests/run_usb.ps1`:boot/display geometry/PCM/driver/Python tests and mock USB descriptors/lifecycle/in-flight reconnect/failure drain|
|PASS|`./smk-p4/tests/run_amy.ps1 -Voices 4`, `-Voices 6`, `-Voices 8`:legacy real-AMY/ownership/polyphony/retrigger/backend/UI callback regressions|
|PASS|`./smk-p4/tests/compare_harmony_off.ps1 -Baseline ../smk-p4-harmony-assist-09`:seven old patch IDs produce byte-identical PCM to0.9 with Harmony OFF|
|PASS|`./smk-p4/tests/compare_factory_heap.ps1`:same host requested AMY heap214922B,163 live allocations, Runtime5540B for0.9 and0.10|
|PASS|`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-factory-8 -j 8` and `-e guition-factory-8-bench -j 8`; final sequential rebuilds after product changes|
|PASS|`python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --environment guition-factory-8` and bench environment with `--musical`:actual target, gates, pins, pool and compile definitions|
|PASS|Read-only COM13/USB serial80:F1:B2:D3:CC:86 preflight; P4 ES1.3/16MiB; two matching complete Flash reads; current app matches preserved0.9 SHA; both candidate partition tables match installed; old firmware resumes|
|PASS|`git diff --check`; shared `src/`, `smk-s3/`, `shared/` diff from0.9 empty|
|FAIL, corrected harness|First exploratory raw sweep compared an unwarmed48-osc Juno allocation against the later64-osc warmed pool. Final full sweep and separate warm stress pass; early logs remain. An initial observer run was stopped to correct its hard-clip threshold. Neither is reported as a successful final run|
|NOT RUN|Full upstream Python `make test`, S3 firmware rebuild, remote CI execution and new-image hardware upload/readback; no shared-core changes|
|MANUAL / NOT RUN|New-image physical smoke, per-patch P4 timing/memory/largest-block/stack sampling, analog clipping/continuity/latency, measured DMA underruns and prolonged qualification|

Logs, actual PCM equivalence files, observer/raw metrics, screenshots and verified backups are under `build/factory010/` (completed runner logs in `logs/`). Intentional injected OOM messages are expected negative tests followed by recovery assertions. A host matrix row with Load/voice PASS does not waive its independent clipping/release failure.

## Audio, performance and memory

No admitted patch has observed hard clipping/saturated PCM in the executed musical matrix. Soft-knee observations on55 (20 samples) and78 (32) are disclosed. Patch32 remains excluded: eight-note raw peak32767/34 saturated samples; separate pre-conversion peak64881/532 hard-clip samples, before the application master. No gain workaround or AMY algorithm change. Release-review IDs69/112/254 remain blocked. Intermediate oscillator/filter arithmetic is not exhaustively instrumented. [Audio analysis](factory_library_010_audio_analysis.md) states exact scope and limitations.

Original host maximum31273us includes scheduling outliers during builds. Separate timing recheck maximum2314us. The largest timing window used to admit a patch is5327us. These are desktop wall-clock values, never ESP32-P4 CPU/load or latency equivalence. P4 cost class is UNMEASURED. Eight-note measured host averages and explicit host LOW/MEDIUM/HIGH criteria are recorded separately.

Host requested AMY heap is214922B before/after expansion, eight instruments/64 oscillators and Runtime5540B unchanged. Final687-swap admitted runtime stress and768-load complete raw stress show no cumulative requested-byte growth; teardown is balanced. Target free/largest/fragmentation/stack under changes is pending. See [memory](factory_library_010_memory.md).

## Concrete review images

|Profile|Application bytes|SHA256|Difference from0.9|Static data+BSS total|IRAM text+end|
|---|---:|---|---:|---:|---:|
|guition-factory-8|1148224|`c50675df6a6c5dfe641d6825cf259b593d9c4e9771d1af22b5ec61a17a318b3e`|+30512|172316|96640|
|guition-factory-8-bench|1249104|`0652377914fce06a385bab4388049fac0d427e90eac204252f08c05144f8adc7`|+30512|172740|101888|

Static data+BSS increases240B (safe) /232B (bench) relative to the preserved0.9 builds; IRAM total is unchanged. Metadata growth is principally flash/rodata. Both images preserve48kHz/stereo PCM16,256 frames, four DMA descriptors,64-slot USB storage and32 USB commands/block. Safe electrical gates OFF; audible bench uses the existing50/51/52+HS0 profile only. Neither image was written to hardware. Compiled project version may precede final documentation/test-only commits; the package checks product freshness and hashes all product sources, including generated .inc files.

## Hardware assumptions, backup and risks

The installed0.9 application remains `ebcbc081d287958e466a22fb063c22d4dd2542f59c9cc7758c4f1c17ed6f3f9c` (1218592B). Two independent16MiB reads both hash `9a7589434ba52614dfc4b19d1b6a11c76536f471fbfb6f2f976e5f219ec9c30a`; extracted application and partitions are independently hashed in the preflight JSON. These reads reset the board but did not write Flash. Logs after reads show the old firmware running. Existing earlier rollback artifacts were not removed.

Board identity is confirmed for the connected unit only. Reconfirm physical wiring, PCM straps/supply/output and VBUS before an authorized write. Current backups and image hashes are concrete; authorization must name the new SHA. No low-latency, gapless switching, clean analog audio or complete physical compatibility claim. Long release tails may be intentional; they are held for review rather than called ownership faults. Categories are name heuristics, and historical DX7 ROM banks remain unresolved.

## Acceptance and next smallest step

|Gate|Decision|
|---|---|
|A Catalog|PASS:256 ROM IDs, names/resources validated|
|B Engine|PASS host:8 reusable instruments/64 oscillators, unchanged heap|
|C Admitted patches|PASS host criteria229;27 blocked with reasons|
|D Music|PASS host:8 voices, sustain, ownership, Harmony, bend/CC1, Touch; physical pending|
|E Browser|PASS real LVGL/AMY host, all screens/selections and bounded paging|
|F Real time|PASS host allocation/safety regressions; P4 load/underruns/gaps/latency MANUAL|
|G Hardware|MANUAL / NOT RUN for0.10; prolonged qualification separately PENDING|

Next: review the exact audible image, reconfirm board/port/electrical bench, then authorize the application SHA for app-only writing if desired. Execute the [short physical smoke](factory_library_010_hardware_smoke.md) and record counters/operator audition. A thirty-minute window is not an automatic prerequisite for this experimental candidate. Firmware packaging never opens serial or writes a device.

## Admitted patches by category

|Category|Admitted|
|---|---:|
|Bass|12|
|Brass|22|
|ElectricPiano|4|
|FX|74|
|Leads|7|
|Organ|16|
|Pads|1|
|Percussion|28|
|Piano|12|
|Plucks|36|
|Strings|17|
