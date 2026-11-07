# Sound editor0.11 — experimental software candidate

Branch feature/smk-p4-live-sound-editor-011 in independent worktree
build/worktrees/smk-p4-live-sound-editor-011. Baseline exact commit
 dfcef41dd04eaa034fc4c20945bfd95ce65062d9 (0.10). Initial checkout was clean.
No destructive reset, automatic merge, force-push, dependency update or firmware
write. Explicit P4/800x480 request takes precedence over the older S3 specs.

## Implemented / architecture impact

Real Juno cutoff/Q/Attack/Release; six mapped FM operator levels and valid ratios;
modulator-only FM brightness. Eight reusable instruments/eight voices/64 oscs and
independent PCM output retained. Native sparse events on the sole AMY owner;
exact original logarithmic restore uses existing native delta API. ROM, AMY core,
S3, USB/I2S/display drivers and shared protocol have no changes.

Non-destructive factory/manual/macro state; Compare retains edits, Revert clears
them. Brightness/Attack/Release macro contributions do not overwrite manual data.
Latest-state coalescing on a separate one-slot mailbox at50Hz; confirmed snapshots,
stale/error rejection and timeout/Panic/retry. No musical queue-size increase.

Existing Blueprint editor tabs/widgets and browser gain Compare/Revert/User,
name/slot selection, Save As/rename/copy/overwrite confirmation and Recall.
Dirty switching is rejected until explicit Save/Revert.16 persistent slots,
128-byte versioned LE/CRC records, ROM compatibility discriminator, dual-key
recovery. UI/control-only NVS with owner-ACKed maintenance silence; no Flash write
from synthesis/render, slider or macro. Original FX remain off/factory EQ retained.

Operator levels are percentages of captured original gain; no promise of a full
DX7 editor. Mix/PWM/filter-env/keytrack/Decay/Sustain/Character/LFO/routing and FX
remain PLANNED. No SD, sampler, layers or limiter. Those extensions require their
own native mappings/tests rather than false AVAILABLE labels.

## Validation

|Status|Executed command / scope|
|---|---|
|PASS|./smk-p4/tests/run_harmony.ps1: bootstrap native C engine and prior musical regressions|
|PASS|./smk-p4/tests/run_sound_editor.ps1: owner/native maps,1000 edits without service heap, valid/range/NaN/stale rejection, Compare/Revert, Harmony edit/group release, coalescing, timeout/retry, codec16 slots, CRC/future-format/blocked source/torn writes/recovery/recall|
|PASS|Same runner: deterministic real PCM changes on1/8 voices for Juno4 and FM135;22 renders, no clipped samples; Compare/Revert fresh onset equals original PCM|
|PASS|Same runner: inherited Harmony/USB/factory runtime with editor enabled;687 swaps, all229 admitted IDs, eight voices, sustain, bend/CC1, disconnect/reconnect, OOM/cancel recovery, bounded memory and balanced teardown|
|PASS|./smk-p4/tests/compare_editor_factory.ps1 -Baseline ../smk-p4-full-factory-library-010: seven byte-identical no-override PCM files|
|PASS|./smk-p4/ui-preview/run_preview.ps1: original Blueprint20 frames/700 mounts/contracts; real LVGL9.3|
|PASS|./smk-p4/tests/run_sound_editor_ui.ps1: requested/applied/rejected values, macros neutral, DIRTY guard, Save/Recall, failed save rejection, simulated reboot, seven editor tabs/User panel and LVGL heap integrity|
|PASS|./smk-p4/tests/run_factory_ui.ps1: original profile/all14 screens/229 factory selections/44-page browser/negative recovery; two editor screenshots visually reviewed, preset keyboard corrected to fit dialog|
|PASS|python smk-p4/tests/generate_factory_library.py --check; ROM/catalog unchanged|
|PASS|git diff --check; no src/smk-s3/shared diff relative0.10|
|PASS|C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j8 and -e guition-editor-8-bench -j8; sequential final builds|
|PASS|python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8; bench adds --musical and its environment;48k/256/stereo, pins/gates, LVGL128KiB, identity and owner verified|
|BLOCKED|Fresh read-flash on COM13 stalled opening serial; stopped with no bytes/new backup or firmware write. Serial inventory still matches80:F1:B2:D3:CC:86|
|PASS|Prior complete0.9 Flash reads rehashed identically; prior0.10 independent app readback rehashed; preserved as rollback evidence|
|MANUAL / NOT RUN|0.11 physical audio/touch/USB smoke, NVS reboot/power-cut/GC/Flash-cache behavior, P4 timings/RAM/stacks and prolonged qualification|
|NOT RUN|Full upstream Python suite, S3 firmware rebuild, remote CI, complete override combinations/extrema or full FM editor qualification|

Initial development failures: Release's missing bp_is_set caught by real PCM;
lossy Hz/log original restore caught by byte comparison; both corrected. An early
inherited test warning was promoted to error by the new runner; the existing
compact test's warning is now explicitly nonfatal, without modifying the old test.
A UI run overlapped a header ABI edit and was discarded; final UI runs rebuild
all widget/backend translation units. Do not count these early attempts as PASS.

## Memory / timing / audio evidence

Host AMY requested heap remains214922B in warmed factory regressions. Host Runtime
is6284B versus0.10's5540B (+744B), predominantly fixed edit snapshots/maps/mailbox.
Backend/save/preset UI state adds further fixed/control allocations; this is not
an overall target RAM measurement. No render/apply allocation in guarded tests.
Actual P4 free/largest/PSRAM/stack before/after remain MANUAL. Existing command,
render and gap counters plus new EDIT011 apply_max_us/coalesced diagnostics are
available; no target render-time or latency improvement is claimed.

No clipped samples in the22 specific edited PCM fixtures. This does not qualify
arbitrary edits/polyphonic ratios/Q combinations. Original27 blocked ROM IDs
remain blocked, with no gain/FX workaround. Factory qualification does not extend
automatically to User Presets. Native smoothing is retained;50Hz limits updates.
Attack can alter an in-progress attack; Release affects subsequent/ongoing release
slope. Compare need not be perfectly continuous. Save/Recall silence is deliberate.
DMA underruns and analog continuity/latency are unmeasured.

## Hardware / rollback / remaining gates

Last verified installed image in0.10 records:1249104 bytes SHA256
0652377914fce06a385bab4388049fac0d427e90eac204252f08c05144f8adc7.
Its operator smoke was approved. Current serial inventory confirms the same
COM13 USB serial only; it does not prove current Flash contents. Fresh0.11
preflight is BLOCKED until COM13 communication is restored.

Two existing complete0.9 backups both rehash to
9a7589434ba52614dfc4b19d1b6a11c76536f471fbfb6f2f976e5f219ec9c30a.
The previous independent0.10 postflash prefix rehashes to
33a5a005d1f664f89221a9128546ad8046e45f0e432612f453c387699f4e6992;
its extracted0.10 application matches the installed-image hash above. Bundle
includes exact0.10 app plus a complete verified0.9 rollback and partition evidence.
These preserved reads are not relabeled fresh backups.

Bench GPIO50/51/52, USB HS0, PCM straps/power/output and VBUS are inherited,
not a new hardware design. Safe profile electrical gates remain OFF. NVS stays
24KiB, partition table unchanged.16 slots admitted conservatively at boot;32 slots
and actual NVS capacity/fragmentation were not qualified on hardware.

Acceptance A-G have representative software evidence with stated scope; Gate H
and physical persistence approval are pending. The0.11 request is not fully
hardware-qualified. Next smallest step: restore COM13, confirm/read current Flash,
review exact candidate SHA, obtain explicit authorization, then perform the short
operator smoke and actual power-cycle recall. Do not inherit0.10 flash authority.

See sound_editor_011_architecture.md, sound_editor_011_amy_capabilities.md,
sound_editor_011_parameter_matrix.md, sound_editor_011_user_preset_format.md,
sound_editor_011_nvs_safety.md, sound_editor_011_audio_tests.md and
sound_editor_011_hardware_smoke.md. Build/package manifest stores actual commit,
source hashes, image hashes, evidence and rollback; it never uploads firmware.

## Frozen candidate image measurements

|Profile|Application bytes|SHA256|Change from0.10|
|---|---:|---|---:|
|guition-editor-8|1186864|`9220a290e79c63068f3c0f13bfba3db7e107d5af5426e5c515de2be057f0626a`|+38640B|
|guition-editor-8-bench|1287776|`0bd1fd6ce2f6c72679c17cecf9e96ca6e61cefc8993349463e70701f5c3f2911`|+38672B|

Exact SDK configuration, ELF/map/sections, source archive, logs,22 WAV fixtures, PCM equivalence bytes and rollback are packaged by package_sound_editor.py. Image hashes here identify the reviewed binaries; Flash authorization remains false.

Static ELF data+BSS (excluding unchanged60B TCM): safe174480B vs172316B
(+2164B); bench174896B vs172740B (+2156B). IRAM text+end: safe98816B vs96640B
(+2176B); bench103936B vs101888B (+2048B). These measured section increases
are not runtime free/largest-block or stack measurements. Section logs are in the bundle.
