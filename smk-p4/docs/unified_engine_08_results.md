# SMK-P4 unified patch engine0.8 — software candidate

Branch `feature/smk-p4-unified-patch-engine-08`, independent worktree
`build/worktrees/smk-p4-unified-patch-engine-08`, from0.7 `bd564872`.
Original worktree,0.7 branch, installed firmware and rollback artifacts untouched.
No push, flash, serial reset or dependency upgrade. The explicit P4/800x480
request supersedes the older S3 specification for this isolated application.

## Implemented and architectural impact

- Catalog and resident resources are independent:32 instruments/256 configured
  oscillators become8 reusable instruments/64 capacity, preserving8 musical
  gates. One eight-voice instrument was also stress-tested; its2576-byte host
  advantage did not justify changing P4 channel/source control semantics.
- Explicit worker-only load and verification, command/PCM generation barriers,
  rejection of notes in partial state, failure status and explicit retry. No
  transactional rollback claim. Severe partial-map corruption requires full
  engine restart; the host tests exercise `end()`/`begin()` recovery.
- Independent higher-priority PCM output task, fixed three-block SPSC handoff,
  initialized silence on empty/stale data, bounded ramp to silence and existing
  master fade-in. No crossfade or gapless-transition claim.
- Touch/USB pitch ownership, per-channel bend/CC1/sustain, note-off priority,
  source disconnect isolation, global Panic and0.5 FM retrigger fix retained.
-16 ROM candidates evaluated;15 admitted as software candidates. Juno32 remains
  unavailable after raw PCM clipping, not hidden by lowering the final master.
  ROM metadata/names are shared with UI; no bank per catalog entry. No256-patch
  support claim. Dry Juno uses ROM EQ, while FX remain disabled; no S3 sonic
  equivalence claim.
- Existing14 Blueprint screens and demo/planned controls retained. Backend
  distinguishes immediate admission/busy from applied confirmation and reports
  load failure. Families, favorites and available Prev/Next use existing UI.
- OUTPUT08/PATCH08/SWAP08 diagnostics distinguish physical writes, worker pacing,
  load, generations and capability heap readings. No shared AMY core changes,
  no USB/I2S/display driver changes, no hardware pin changes.

Full Runtime host requested heap:286490 ->165770 bytes (-120720,42.1%). Same
warmed live bytes before/after stress; teardown returns to baseline. This
excludes output task/stack, driver, static DRAM and LVGL pools. Internal/PSRAM
free/largest, fragmentation and P4 timing during changes have NOT been measured.
See [memory analysis](unified_engine_08_memory_analysis.md) and
[source comparison](unified_engine_08_s3_p4_comparison.md).

## Executed validation

|Status|Command / scope|
|---|---|
|PASS|`./smk-p4/tests/run_amy.ps1 -Voices 8 -SoundLibrary` before changes:0.7 baseline|
|PASS|`./smk-p4/tests/run_unified_probe.ps1`: both real-AMY strategies2005 selections each;16 dry candidates; actual Runtime1000 FM/FM +1000 FM/Juno selections with8 gates; ownership, releases, bend/CC1 isolation, sustain, NoteOn0, load failures, invalid IDs, Panic, partial object, disconnect/reconnect, explicit recovery, service OOM; allocator guards;10000 concurrent PCM handoff blocks; existing USB two-producer and retrigger suites|
|PASS|`./smk-p4/tests/run_unified_ui.ps1`: real LVGL9.3 + AMY,14 screens,15 software patches, deferred confirmation including same-ID selection, busy rejection, load error/retry, Panic, favorites and unsupported IDs|
|PASS|`python smk-p4/tests/summarize_unified_engine.py`: evidence-hashed16-patch matrix,15 distinct48kHz stereo PCM16 audition WAVs, each65536 frames|
|PASS|`./smk-p4/tests/run_host.ps1`: boot, display, PCM/driver and29 Python tests|
|PASS|`./smk-p4/tests/run_usb.ps1`: mock USB descriptors/lifecycle/reconnect/failure cleanup|
|PASS|`./smk-p4/tests/run_amy.ps1 -Voices 4`, `-Voices 6`, `-Voices 8`, and `-Voices 8 -SoundLibrary`: legacy real-AMY, ownership, polyphony, retrigger, backend and mock-widget regressions|
|PASS|`./smk-p4/ui-preview/run_preview.ps1`: original Blueprint contract,14 screens/subtabs,700 mounts without leak;20 preview frames|
|FAIL|`./smk-p4/ui-preview/run_preview.ps1 -PoolKB 96`: isolated rejected optimization, style-array OOM; production remains128KiB|
|PASS|`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-unified-engine-8 -j 8`: final sequential software-safe target|
|PASS|`C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-unified-engine-8-bench -j 8`: final sequential audible review image only, not flashed|
|PASS|`python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --environment guition-unified-engine-8` and corresponding `guition-unified-engine-8-bench --musical`: actual target/config/source/pins/gates checks|
|PASS|`git diff --check`; shared `src/` diff empty|
|NOT RUN|Full upstream Python `make test`, S3 firmware/tests and legacy4/6 target rebuilds; shared core unchanged, legacy host profiles executed|
|MANUAL / NOT RUN|Physical0.8 audio, USB/Touch, I2S continuity/underruns, latency, clicks, memory fragmentation, stack and>=30-minute qualification|

Intentional injected OOM/partial-instrument errors appear in host logs. Initial
harness failures assumed immediate bank selection and bank-specific instrument
IDs; the tests now explicitly drive the worker load and reusable slot identity.
A host UI compile temporarily lost a managed LVGL header while IDF dependency
configuration ran; rebuild host UI after firmware configuration has completed.
Do not run dependency-mutating PIO configuration and host UI compilation against
the same managed-components tree concurrently.

Final build repetition also hit `No space left on device` on C:. Failed build
and dependency-configuration logs were preserved with `.diskfull.log` and
`.dependency-failure.log` suffixes. Only this new worktree's regenerable build
and dependency artifacts were NTFS-compressed; newly generated legacy4/6/8
host WAV/executable intermediates were removed (their commands/logs and final0.8 WAVs
remain). Existing branches, backups and rollback bundles were not cleaned.
Final review requires the sequential successful rebuild/audit recorded in the
bundle; the earlier failed attempts are not counted as successful validation.

Final safe image:1110656 bytes, SHA256
`d6cc4e441bc40c2cba1f9607bab23b845cf9076788c372c46988439198abc9a9`.
Final audible review image:1211344 bytes, SHA256
`52efd1ea6a5bf40a98013c873c08e99de42f9536c347864b394f896613e66e45`.
The audible image adds6640 flash bytes and1168 static DRAM bytes relative to
the preserved USB64 baseline; IRAM text is unchanged. No exact-image flash
authorization has been given. Full sections/configuration are in the manifest.

Juno14 eight-note release has one output LSB after80 simulated seconds; the
release threshold is two LSBs, not exact mathematical zero. All tested raw
clipping counters for the15 admitted candidates remain zero in1/4/8-note loads.
Normal logs and host results do not establish internal fixed-point saturation
behavior for all possible performances. [Patch matrix](unified_engine_08_patch_matrix.md)
lists the precise tested subset and limitations.

## Gate decision

|Gate|Decision|
|---|---|
|A Architecture|PASS: catalog no longer multiplies residency|
|B Resources|PASS host; MANUAL target fragmentation/capability heap|
|C Music|PASS host; MANUAL physical performance and audition|
|D Timbres|PASS software subset15/16; MANUAL physical qualification|
|E Interface|PASS host LVGL14 screens and load confirmation; MANUAL target|
|F Real time|NOT RUN on P4; independent output design is not timing evidence|
|G Hardware|NOT RUN; no image was uploaded|

This is a reviewable software candidate, not completed physical qualification.
The manifest generated by `python smk-p4/tests/package_unified_engine.py` freezes
source commit/patch, both firmware images with SHA/config/ELF/map/partitions,
section sizes, raw logs, host metrics and WAVs. It always records
`flash_authorized=false` and `hardware_qualified=false`. Source history preserves
separate audit, prototype, runtime, UI, tests/catalog and LVGL diagnosis commits;
the bundle's source commit and `git log bd564872..HEAD` identify their final IDs.

Next smallest step: operator review of that concrete audible image, verification
of unit/current-image/full backup/rollback and exact-image authorization before
the [hardware procedure](unified_engine_08_hardware_test.md). Do not inherit0.7
smoke as0.8 acceptance. Keep128KiB LVGL; the96KiB experiment is rejected and
PSRAM allocator relocation remains NOT RUN. No unresolved board detail was
turned into a new pin assignment.
