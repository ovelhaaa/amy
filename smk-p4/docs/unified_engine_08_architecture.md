# Unified engine 0.8 architecture

Opt-in `guition-unified-engine-8` uses the existing IDF5.5.2 / LVGL9.3 pins,
48kHz,256 stereo PCM16 frames, four DMA descriptors, USB64 storage and32
USB commands/block. Electrical gates remain OFF. Legacy4/6/8 and0.7
environments remain selectable. No AMY core or driver changes, no new dependency.

Eight single-voice instruments share64 oscillator capacity. The ROM catalog
and software selection table do not multiply AMY voices. Eight gates total,
no stealing; Touch token and USB channel/pitch/sustain ledger remain unchanged.
AMY's global bend is still avoided in favor of per-slot fractional note updates;
CC1 remains per-USB-channel software vibrato. Touch and USB same pitch are
separate slots. Global Panic cancels both generations; ordinary USB disconnect
cancels USB ownership and preserves Touch, matching the tested P4 policy.

## Ownership and transition

Lower-priority synthesis worker owns all AMY state. `apply()` validates a
selection, quiesces old notes, invalidates both command generations and marks
Requested. `loadPendingPatch()` is an explicit allocating worker operation,
never a call from the output task. It deletes the entire old instrument set,
executes resets, neutralizes bus EQ, loads every slot, executes patch deltas,
warms DSP, and verifies patch IDs, voice counts, nonoverlapping osc maps and FM
operator links. A second generation barrier discards traffic arriving during
load. Confirmation updates the applied patch ID only on complete success.

State sequence: Ready -> Requested -> Loading -> Verifying -> Ready or Failed.
The single-element copied UI snapshot may coalesce fast intermediate states;
the backend marks busy immediately on admission and only confirms an applied
snapshot after the load counter advances. Failed does not accept notes.

Checked resource admission is NOT transactional materialization. Failure leaves
the last confirmed ID for diagnostics, not a claim that the old instrument is
playable. A valid explicit retry reconstructs all eight slots; severe map
corruption may require complete engine restart/reboot. Injected partial-object
corruption is tested with `end()`/`begin()` recovery, not fictional rollback.
Panic/disconnect during allocations abort confirmation; Panic remains an atomic
generation latch and can be observed between slot loads and after materialization.
Any new service OOM also fails closed to zero PCM without allocating recovery
events. No automatic rollback or engine reboot is hidden from the operator.

## PCM ownership

The higher-priority `pcm_output08` task exclusively begins, starts, writes and
stops the existing PCM5102 driver. It never accesses AMY. A fixed three-block
SPSC handoff copies512 samples/block with release/acquire publication. Full
handoff paces the synthesis worker; it never makes the output consumer wait.
Generation mismatch discards old PCM. An empty queue or generation barrier
ramps the last output sample to zero within one block, then continues initialized
silence. Reconstructed AMY output uses the existing controlled master fade-in.
There is no crossfade and no promise of gapless musical transitions.

The worker can block while loading without stopping the independent DMA writer.
This is a scheduling design, not proof of P4 continuity: output gaps, task stack,
write errors/timeouts, refill risks and frames have their own mailbox/OUTPUT08
diagnostics. DMA hardware underruns remain explicitly UNMEASURED. Worker legacy
`dma_*` timing labels measure enqueue pacing in0.8; OUTPUT08 is the physical
I2S timing source. Do not compare these different intervals as the same metric.

SWAP08 records latest before/after free and largest internal/PSRAM blocks and
selection/failure serials. Fast changes can coalesce before rate-limited logs;
physical memory testing must select one patch per captured diagnostic interval.
Render, total commands/load, enqueue pacing and physical write times are distinct.
No producer-side blocking or allocation is introduced in the output consumer.

## Qualification boundary

Host stress validates memory accounting, maps and behavior; SPSC stress validates
publication. It does not exercise FreeRTOS scheduling, analog clicks or I2S DMA.
The build is a software review candidate with physical gates disabled. Only
after specific-image authorization and target tests can Gate F/G be promoted.
