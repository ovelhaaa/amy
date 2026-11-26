# SMK-P4 0.19-A — Harmonic Pads

## Implementation and provenance

Branch: `feature/smk-p4-harmonic-pads-019a`. Remote reference verified at `ff2f098d0d6417273198e4faed782fe6ad6a4b32`; no later reference commits existed. Work is isolated in a separate worktree, preserving `codex/p4my-boot-splash`. Tested implementation SHA: `b7a6f91a40085102036bec9a705b963752285210`. Documentation/capture commits follow this code SHA; the delivered branch HEAD is reported in the delivery message.

The explicit P4/800x480 request supersedes the legacy S3 specification for this application. No AMY DSP, GPIO, driver, NVS format, Scene format, controller mapping, clock, allocator capacity or task priority changes were made.

Commits separate navigation safety, musical contracts, owner protocol and functional UI. The UI commit includes HOLD only after the owner lease/identity tests passed.

## Musical behavior and ownership

`TOCAR > PADS` replaces the primary ENCADEAR placeholder. Its planned status remains visible as secondary text. Eight 190x80 pads form a 4x2 grid. Local ROOT, SCALE and OCT controls do not write global Harmony; the persistent header continues to display global Harmony context. Local selections become frozen intent when a pad is requested, and owner confirmation determines ACTIVE. Current controls choose triads; the underlying contract also tests existing inversions/open voicing and seventh chords.

Seven ascending degrees are derived from `harmony::kScales`; pad eight is tonic one octave higher. Names follow the generated pitch-class intervals. Chromatic, pentatonic and blues scales disable degree pads and explain the seven-note requirement. Input range is checked before narrowing; complete-group folding remains Harmony's existing rule.

`HarmonicPad` groups use pad index, lifetime-monotonic token and owner generation, independently of original/generated pitch. The existing eight-entry allocator transforms the frozen configuration once and reserves all slots atomically. Pads bypass global Harmony eligibility, avoiding a second transform. Common pitches are separate physical gates. Capacity failure emits no partial chord and steals no voice. Snapshot records request/admission/rejection and active token per pad; publication uses the global owner generation even on the first ready snapshot after boot/patch, and rejects stale snapshots before copying pad state; queue acceptance alone never produces ACTIVE.

MOMENTARY issues On only on PRESSED and Off on RELEASED/PRESS_LOST/DELETE. Duplicate presses are ignored. HOLD toggles on PRESSED and survives the Touch keyboard's unrelated ten-second lease. A dedicated two-second owner safety lease is renewed only by the live pad view. Loss of UI service expires groups; stale queued intents and stale release tokens cannot resurrect or terminate a newer group. Mode changes and RELEASE ALL PADS release only pad tokens. Widget destruction also releases them before callback state is freed.

Leaving PADS, including HOME, releases only pads. USB, keyboard, Arp and Sequencer retain their own origins. USB disconnect preserves pads; global Panic and patch barriers clear all groups. Configuration changes affect new admissions, leaving admitted pitches frozen. No heap operations, locks, logs, storage or LVGL calls were added to musical owner processing. Bounded release latches remain independent of normal queue saturation.

## Navigation safety

The independent `35168af5` commit guards direct navigation, all footer areas, Back, HOME and same-screen rebuilds. Requests are checked both before scheduling and at deferred execution. Blocked requests preserve history and sub-tabs. The externally mounted User Presets panel also blocks navigation. Panic cancels its modal without invoking a destructive callback. Scene/Pattern/User Preset scenarios and injected MIDI HOME/NextPage/Learn are covered. See [navigation evidence](navigation_safety_019a0.md).

## Validation

**CONDITIONAL GO for host software / compiled P4 candidates.** All pad acceptance tests and relevant editor regressions pass. Physical qualification is deferred. The inherited SoundLibrary heap-budget failure remains open and is separate from the full-editor candidate.

| Validation | Status | Evidence |
|---|---|---|
| Clean 0.19-A music, real owner, real LVGL, stress and 17 requested captures | PASS | Tested code SHA above; recorded flags and source hashes; no allocations during measured musical service |
| UI regressions C.3a, C.3, C.2, C.1 integration, C.1a hardening, Sound Editor / SMK25 / FX | PASS | Full `-Regressions` runner completed with exit 0 |
| Harmony, USB musical, retrigger and full Touch burst | PASS | Existing `run_harmony.ps1` completed with exit 0 |
| Arp engine/runtime and inactive-Arp PCM against 0.18-D.1 | PASS | Bit-identical SHA256 `11FAB98798FB56EC284F0723D115C829643ED0DC41E46FAC926C023761FA6BF5` |
| Both editor firmware profiles and build audits | PASS | Eight voices, single LVGL, pool128 KiB, 48000/256, expected electrical gates |
| Legacy SoundLibrary heap budget | FAIL (inherited) | Same `polyphony_test.cpp:82` assertion reproduced in the reference checkout and current tree; budget unchanged |
| Physical audio, USB, touch, stack high-water and deadlines | MANUAL | Deferred by user; no flash or serial monitoring |
| Other upstream/S3 platform builds | NOT RUN | No source changes outside the P4 application |

The final clean manifest is in `build/harmonic_pads_019a/test_manifest.json`; a committed [validation summary](harmonic_pads_019a_validation.json) records its digest, compiler flags, input count, firmware hashes and capture hashes. Source hashes were independently rechecked after validation. The dirty entries recorded during the run are documentation and generated evidence, not uncommitted implementation changes.

Owner cases include equal input pitches on separate pad IDs, overlapping chords, 4+4=8 voices, complete rejection beyond capacity, repeated tokens, stale Off, queue-full Off, fresh HOLD beyond ten seconds, dead-view expiry, timestamp wrap, patch/Panic barriers, frozen configuration, USB disconnect and keyboard/USB coexistence. UI tests check owner gate state after release, teardown and navigation, rather than relying on widget disappearance. An older C.1a regression expected the navigation defect; it now requires the confirmation to remain open, then explicitly cancels before navigating.

Executed commands:

```powershell
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmonic_pads_019a.ps1 -Clean -Regressions
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_harmony.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_arp_015.ps1 -Runtime -BaselineRoot build/pads_baseline
powershell -ExecutionPolicy Bypass -File smk-p4/tests/run_amy.ps1 -Voices 8 -SoundLibrary
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --environment guition-editor-8
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8-bench -j 8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --harmony --full-factory --sound-editor --musical --environment guition-editor-8-bench
```

The clean runner records compiler identity, flags, SHA, dirty state and source hashes in `build/harmonic_pads_019a/test_manifest.json`. It builds pinned LVGL 9.3, real AMY and the real P4 UI. USB messages and NVS are host fixtures. Captures are actual 800x480 LVGL framebuffer output marked HOST SIM. An early fixture incorrectly issued Panic before assertions requiring transport; it was corrected. One exploratory concurrent dependency setup briefly made LVGL headers unavailable; final dependency setup and clean validation are separate.

## Measured resources

Host ABI comparison against the reference checkout, using identical full editor flags:

| Object | 0.18-D.1 | 0.19-A | Delta |
|---|---:|---:|---:|
| Command | 24 B | 28 B | +4 B |
| UI Commands | 5280 B | 5448 B | +168 B |
| Owner Runtime | 14880 B | 15480 B | +600 B |
| Snapshot | 2584 B | 2752 B | +168 B |

The command payload accounts for +128 B in UI32 and +256 B in USB64. Eight release latches and heartbeat add bounded state. The existing group ledger is reused; no second voice allocator is allocated. Target ABI totals and static image changes must be assessed separately from these host sizes. Stack, queue capacities and 128 KiB LVGL pool are unchanged. Physical stack high-water and CPU/deadline/analog latency remain MANUAL.

Final clean pad stress: **86 -> 86 objects**, **79440 -> 79440 B free**, **79104 B largest free block**, **1% fragmentation**, **64800 B peak**, **0 occupied gates**, **0 dropped UI commands**. It exercises 1000 press/release cycles, 500 pad/macro transitions, at least 200 key/octave/mode changes, 100 saturation scenarios, 100 Panics and 100 active-note screen exits. The main navigation regression also retains 93 objects and 74192 B free, with 68736 B peak.

Host pad event cycles (including UI/owner/frame work): median **975.3 us**, P95 **1422.0 us**, max **1992.8 us**. Owner transform max **2 us** and admission max **73 us**; musical malloc/free delta **0** under the wrapped allocator. These host timings do not predict physical P4 latency or CPU load. Existing navigation cycles measured median 4071.0 us / P95 4985.9 us / max 8252.4 us; host scheduling and concurrent compilation affect such measurements.

The UI class measures **584 B on host**, **424 B with the P4 ABI**, outside the LVGL pool; it is allocated/deleted on the UI thread only. The P4 editor global Runtime symbol is 15480 B; bench is 15520 B with its existing telemetry/profile definitions. No stack, queue capacity or pool budget was increased.

| Profile | Reference static RAM | Final static RAM | Delta | Reference Flash | Final Flash | Delta | Final image |
|---|---:|---:|---:|---:|---:|---:|---:|
| `guition-editor-8` | 70328 B | 71084 B | +756 B | 1523916 B | 1533036 B | +9120 B | 1533760 B |
| `guition-editor-8-bench` | 70876 B | 71636 B | +760 B | 1629406 B | 1638428 B | +9022 B | 1639152 B |

Reference totals are the recorded 0.18-D.1 build measurements; final totals are the executed PlatformIO size checks. Firmware SHA256 values are in the validation JSON. Physical stack high-water, RAM/PSRAM under actual DMA/USB load and CPU/deadline qualification remain unmeasured.

## Physical qualification (MANUAL, deferred)

No flash, serial monitor or physical qualification was performed. No new pinout or hardware revision was assumed. Use only the existing independently confirmed Guition bench wiring in a future physical session.

With the candidate already installed by the operator in that future session:

1. Open PADS in C major, press/release each degree and confirm the named triad and silence after release. Drag outside a pad and confirm release.
2. Enable HOLD; sustain two triads. Request a third and confirm atomic rejection, unchanged first chords and at most eight occupied gates.
3. Sustain a pad, change key/scale/octave, and confirm its pitches remain frozen. Release and retrigger to hear the new intent.
4. Hold an SMK25 note and a pad; leave PADS through HOME/FX/Timbres. Confirm only pads release.
5. Run Arp and Sequencer, open/close PADS, and confirm transport/gates remain independently owned.
6. Disconnect/reconnect USB while HOLD is active. Confirm independent pads survive disconnect and USB resumes without reboot. Panic must silence every origin.
7. Open Scene/Pattern/User Preset confirmations; attempt footer, MIDI and Back navigation, cancel, then navigate normally. Panic must never perform the pending write/delete.
8. Record actual DMA underruns, maximum render duration, stack high-water, free internal RAM/PSRAM and end-to-end latency under combined load. Host results do not establish clean analog audio or stage readiness.

## Residual limits and next stage

HOLD depends on a regularly serviced pad view; a pause of two seconds intentionally releases its groups. The touchscreen's physical multi-touch capabilities are not qualified; the owner accepts independent groups, and HOLD supports sequential activation of multiple pads. Gate capacity follows existing semantics: AMY release tails may continue after a gate becomes reusable. Controls are local session state, not persisted in Scenes/NVS. Pad velocity is fixed at 100. Chord extension controls are not exposed in this MVP.

0.19-B proposal, planning only: retain this transport-neutral pad-intent API for confirmed SMK25 profiles; define optional Arp chord routing with explicit identity/release ownership; design configurable chord banks/progression memories separately; version any future Scene persistence only after migration and safe defaults are specified. Each should have owner isolation/capacity tests before UI integration. None is implemented here.


### Proposed 0.19-B slices (not implemented)

1. Confirm physical SMK25 pad messages in the monitor and define an explicit performance-mode/profile switch. Reuse the pad-intent boundary; preserve default direct USB routing. Accept only after stale-token, reconnect and mixed-source tests pass.
2. Specify optional pad-to-Arp routing as a separate ownership contract: captured chord identity, single Harmony transformation, Stop/Panic/release behavior, and atomic eight-voice admission. Test it without UI first.
3. Define configurable chord banks and progression memories as bounded pure data. Validate notes, voicings and capacity before adding editor controls; do not introduce a second harmony library or voice allocator.
4. Propose a versioned Scene/NVS extension with CRC, safe defaults, migration and explicit save confirmation. Do not reuse raw compiler layouts or persist each control movement.
5. Review those contracts and their acceptance matrix before authorizing implementation. Physical qualification remains a separate future activity under the user's deferred decision.

Some HOST SIM pad captures retain the deliberately failed Scene Recall fixture from the preceding negative regression. That banner is existing shell context; pad admission, notes and voices are independently confirmed by the real AMY owner.
