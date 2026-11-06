# Physical qualification0.8 — MANUAL / PENDING

Preflight identified COM13 (USB serial80:F1:B2:D3:CC:86), captured the running
0.7 diagnostics, and reset the device for read-only flash backup. After explicit
confirmation of the exact SHA and wiring, the0.8 application was written and
independently read back. The0.7 full backup and rollback bundles are preserved.
Historical0.7 USB64 smoke is not0.8 approval; corrected
0.7 did not undergo a replacement prolonged window.

The ROM identified ESP32-P4 revision1.3. Two independent complete16MiB reads
matched SHA256 `5f131de44b8884e20f260587a6c0ff17508d9779d253ffba6005bb1583a44912`.
The installed application at0x10000 matches the documented0.7 USB64 SHA256
`2038bb68159ecc88aa256cf286a434b32914fc5e461b320f28216691b17b26f1`.
The installed3072-byte partition table matches the candidate table. The verified
full-flash/app-only rollback bundle is `build/unified08/rollback_07_usb64_full.zip`,
SHA256 `97fa77f6d6f43388f9d9dc54ddac57f3825b6a7e961808b74ef849e1b20bce14`.
The post-backup0.7 serial capture shows AMY ready, zero notes and zero recorded
I2S errors/compute deadlines/OOM/dropped USB events over the short observation.
This is not a new 0.8 audio or stability test.

The audible candidate is1211392 bytes at0x10000, SHA256
`a5b410c29b677fcdc894ee78b1f2a22c64d98594143f460edd7800a4965e8617`.
Image inspection identifies ESP32-P4,16MB, DIO/80MHz; the previous application
has the same image-header settings. Effective sdkconfig selects eight voices,
PCM5102 GPIO50/51/52 and USB HS0. The operator subsequently confirmed the exact
SHA and this wiring before the write. See the
[physical qualification record](unified_engine_08_hardware_qualification.md).

The reviewed write scope is the application at0x10000 only, within the existing
0x300000-byte factory partition. Bootloader, partition table, NVS and the rest
of flash are outside that requested write. Use esptool's `keep` flash header
settings and verify a post-write application readback against the candidate SHA.
The generated `flasher_args.json` lists DIO/40MHz, while `image-info` reports
DIO/80MHz in both the installed and candidate application headers. Treat these
as distinct configuration artifacts, not a measurement of the live flash clock;
do not overwrite the existing bootloader to resolve this discrepancy.

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
