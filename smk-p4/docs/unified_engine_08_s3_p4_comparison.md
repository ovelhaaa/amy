# Unified engine 0.8: source audit

Source: `bd564872` (0.7). Explicit operator request for P4/800x480 takes
precedence over the older S3/284x76 `specs.md`. No S3 feature or hardware
configuration is migrated implicitly. No firmware upload is authorized.

|Question|S3|P4 0.7 / implication for 0.8|
|---|---|---|
|Main instrument|`beginEngine`: synth 1, eight voices, bus 0, 4 ms stealing delay; drums synth 10 allocated first|Eight single-voice instruments per patch, delay zero; application owns admission|
|Selection|`loadPreset` submits a copied Preset command; `executePreset` clears notes, sets patch/voice count, executes deltas, captures FM baseline|Selection silences and changes bank index; no preset load in service|
|Materialization|`amy_add_event` assigns instrument/resources and parses patch into deltas; oscillator reset/configuration happens in `amy_execute_deltas`|Identity getters alone cannot prove DSP initialization succeeded|
|Replacement heap|Instrument object and two voice FIFOs; patch parsing consumes delta pool; reset/configuration can allocate/free oscillator/filter DSP storage|Even FM/FM replacement allocates; checked admission is NOT transactional materialization|
|AMY owner|Synthesis worker (`serviceBlock`), separate from AudioTask|`runAudio` owns commands, AMY rendering AND blocking I2S writes|
|I2S relationship|Two-block bounded PCM ring; AudioTask copies or produces zeros without calling AMY|Loading on the existing P4 owner would delay DMA refill even outside `render()`|
|Blocking protection|Output can substitute zeros while worker loads; ring alone does not prove no hardware underruns or inaudibility|Need independent output, bounded handoff and explicit silent-transition telemetry before firmware integration|
|Notes/sustain/Panic|NoteOwnership plus pitch reference counts; final matching release reaches engine; Panic clears deltas, ownership, pedal, stack and gates|P4 keeps independent Touch tokens and USB channel/pitch slots, channel sustain/bend/CC1; retain these semantics|
|Repeated pitch|References aggregate overlapping routed outputs; mono stack otherwise|P4 single-voice slot plus phase restart fix preserves retrigger; one shared instrument cannot directly replace source-specific tuning|
|Six/eight oscs|Preset replacement releases old voice osc ranges and allocates new ranges|Eight independent replacements can fragment a tight pool; test FM/Juno in both directions and delete/quiesce all slots if required|
|FM reset|Execute pending reset/config deltas before capture; invalidate cached operator baseline and rebuild per synth|P4 has no FM editor cache; verify oscillator maps, wave/algorithm links after every load|
|Retained resources|Engine global buffers, pools, output ring, drums; main instrument/FIFOs are replaced|Catalog metadata may remain ROM-only while resident voice/osc limits stay constant|
|Pool mutation|Instrument preparation/commit, release_voice_oscs, oscillator reset, patch parsing, lazy DSP, delta pool growth|Must isolate from DMA refill; no full rollback guarantee in current API|
|Internal memory|Hot synth/events/block/mix internal; delay/sample/sysex in PSRAM; 120 oscs; drum-first ordering|P4 keeps same capability split; reducing duplicated residency is distinct from moving hot state or LVGL pools|

Inspected sources: S3 `amy_adapter.cpp`, `amy_commands.cpp`, `synth_config.h`,
`patch_manager.cpp`, `factory_patches.cpp`, `audio_task.cpp` and adapter header;
P4 runtime/config/commands/backend/Blueprint adapter; shared `patches.c` and
`instrument.c`. Factory S3 descriptors can override mono, filter, envelope and
effects: a dry ROM patch on P4 is not guaranteed sonically equivalent to S3.

Provisional preference is eight reusable single-voice instruments: preserve
per-source/per-channel controls and existing voice ledger. One eight-voice
instrument remains a required host comparison, not an assumed substitute.
Choose only after measured replacement and failure tests. Keep upstream core,
USB, I2S, pinout, LVGL allocator and legacy 4/6/8 profiles unchanged during
the isolated host investigation.

Hardware baseline is historical evidence, not reproduced 0.8 measurements:
0.7 USB64 smoke passed; the replacement prolonged qualification was NOT RUN.
See `sound_library_07_hardware_qualification.md` for the preserved 25-drop FAIL.
