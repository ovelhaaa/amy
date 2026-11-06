# Physical qualification0.8 — MANUAL / NOT RUN

No serial port, reset, backup read, flash write or device state change was
performed in this task. Existing installed firmware, full backups and rollback
bundles are preserved. Historical0.7 USB64 smoke is not0.8 approval; corrected
0.7 did not undergo a replacement prolonged window.

## Candidate review before any upload

Build only (no upload target):

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-unified-engine-8
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --environment guition-unified-engine-8
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-unified-engine-8-bench
python smk-p4/tests/check_blueprint_build.py --voices 8 --sound-library --unified-engine --environment guition-unified-engine-8-bench --musical
```

The first image keeps electrical gates OFF. Only the second is an audible
candidate for the previously documented bench wiring: Guition JC-ESP32P4-M3
ES1.3, PCM5102 BCLK50/WS51/DATA52 and USB HS0. Reconfirm the actual unit/port,
module straps, power/VBUS, grounds, output stage and current wiring; no board
revision or physical suitability is newly inferred here. Preserve48kHz/PCM16
stereo/256 frames/four DMA blocks, touch800x480, eight voices and partition table.

Freeze a hashed review bundle with `python smk-p4/tests/package_unified_engine.py`.
Inspect its manifest, ELF/map, sdkconfig, partition/flasher metadata, binary SHA
and source. Identify currently installed image and verify full backup and known
rollback SHA before requesting explicit operator authorization for the exact
audible candidate SHA and write extent. A build or review bundle grants no flash
authority. Do not reuse an earlier authorization for this new architecture.

## Smoke and measurements

Capture boot plus existing rate-limited diagnostics; log the image SHA and boot
identity. Require AMY Ready and OUTPUT08 advancing frames, zero write errors,
timeouts/OOM, initialized PCM and a functioning screen/SMK25. Record internal
free/largest, PSRAM free/largest, static DRAM from map, all task stack minima,
LVGL pool peak/fragmentation, USB counters and output gaps separately.

1. Silence, one note, four notes and eight notes on128/135/138/142, then4/8/12/14/
   31/0/1/131/141/144/151. Verify actual different timbres, attacks/releases,
   stereo/channel order and rejection of a ninth gate. Juno32 must be unavailable.
2. USB C4 + Touch C4, release each first in both orders. Hold another MIDI channel
   and Touch while bending/CC1 on one channel; only its USB voice may change.
3. Repeated On/Off/On before release completion; slow full release/retrigger;
   sustain down, release notes, play new notes, sustain up. No lost attacks or
   delayed old release may kill a new voice. NoteOn velocity0 must release.
4. Select FM/FM and FM/Juno with eight held notes, sustain and queued controls.
   During Requested/Loading, new notes are rejected/discarded. Applied ID must
   change only after verification; old-generation notes must never reappear.
   Measure silence duration and clicks by recording analog output alongside
   block/generation counters. Silent transition is allowed; gapless is not claimed.
5. Panic while sounding/releasing/selecting, unplug during notes/load and reconnect.
   USB ownership clears; Touch isolation follows the preserved source policy.
   Global Panic cancels both. A failed load must show failure and silent output;
   test injected failures on host, not by inducing unstable power on hardware.
6. Navigate all14 screens, Patch Browser families, Prev/Next and favorites during
   MIDI. Demo/planned controls stay labeled and cannot send unsupported DSP edits.
7. Capture one patch selection per diagnostic interval and preserve every SWAP08
   serial, before/after free/largest blocks. Compare same-patch readings across
   repeated warmed cycles. Do not add independent pool capacities together.

Compare baseline0.7 and0.8 render average/max, command/load time, command latency,
musical interruption, worker compute deadlines, OUTPUT08 write time/errors,
refill risks, gaps/barriers, MIDI drop/overflow/parse errors, clipping, OOM and
stack. Block budget5333.33us. Worker enqueue pacing is not physical I2S write
time. DMA underruns are UNMEASURED until independently instrumented/observed;
zero refill risks alone does not certify no audible dropout.

After smoke, run at least30 minutes of representative FM/Juno/eight-voice MIDI,
Touch, sustain, bend/CC1, UI navigation and four instrumented hot-plugs. Record
operator audition separately from counters. Any growing same-patch memory loss,
OOM, stuck note, dropped critical event, write failure or audible regression
fails qualification. Freeze raw logs and same-image hashes; do not reset counters
to erase failures. Gate F/G remain NOT RUN until this evidence exists.

## Rollback

On failure, stop qualification, preserve raw logs and candidate SHA. The operator
must choose the previously backed-up known image/extent for the same unit,
verify its SHA and authorize that concrete recovery. Existing0.7 and older
branches/worktrees/bundles are untouched. No automatic upload or rollback action
is part of the scripts added here. A partial engine may require full reboot;
the implementation does not promise transactional recovery.
