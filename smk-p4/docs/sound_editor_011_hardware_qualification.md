# Sound editor0.11 — authorized flash and running observation

Operator authorization: **pode gravar na placa**, referring to the concrete
0.11 bench candidate prepared in this chat. Candidate source6052d5e8,1287776B:
`0bd1fd6ce2f6c72679c17cecf9e96ca6e61cefc8993349463e70701f5c3f2911`.
Original review bundle/manifest remains immutable; its preflash authorization=false
is historical. This receipt records the subsequent explicit authorization/write.
No rebuild or product source change was made for flashing.

## PASS — fresh identity and rollback

COM13 serial/MAC80:F1:B2:D3:CC:86 identified ESP32-P4 ES1.3,16MiB Flash.
The prior serial block cleared on retry. Two fresh complete16MiB reads are identical:
`51ba93260301ab3800e42f20fb6cbed3d70c15f2c58b680e4ff1eae2e54ddb20`.
The extracted current0.10 application matches1249104B SHA256
`0652377914fce06a385bab4388049fac0d427e90eac204252f08c05144f8adc7`.
Current partition table exactly equals the candidate's3072-byte table. Fresh full
backups, extracted0.10 app, earlier verified recovery files and original candidate
are retained. A preliminary read timed out at60s; historical/full successful reads
require about96-97s, so the successful bounded retries allowed180s. No failed read
is reported as a valid backup. An initial direct interpreter probe lacked a Python
module; using the pinned PlatformIO virtual environment resolved it without installs.

## PASS — application write and independent readback

Pinned esptool5.1.0 wrote only0x10000, erasing0x10000..0x14afff,1287776B.
Its data hash verification passed. The device stayed in the bootloader until an
independent read of0..0x14afff (1355776B) confirmed:

- Application bytes exactly equal the authorized image.
- Entire pre-application64KiB exactly equals the fresh backup: bootloader,
  partition table and NVS unchanged by this write, before first new-app boot.
- No full16MiB post-write equivalence is claimed; outside-app Flash was not
  requested for writing. Normal app initialization/preset saves may subsequently
  use NVS; the prefix proof refers to the write/readback interval.

Readback prefix SHA256:
`6b6702c367c0b5bd0d0a79da8de2d5f7cf5471682d4ae2dacf3becd000024815`.
The final read reset the device to execute0.11.

## PASS — observed running boot/idle, not musical qualification

A32-second serial capture has a partial early image line then periodic running
snapshots at approximately12-37s uptime. It is not a complete early-boot trace or
cold power-cycle test. AMY admitted initialization and I2S advanced, USB reported
one connected device. No MIDI events or patch edits occurred in this capture.

|Metric|Last captured / cumulative value|
|---|---:|
|Ready / factory patch / active gates|1 /128 /0|
|Frames written|1735424|
|Render average / maximum|197 /3635us|
|Render / compute deadline counts|0 /0|
|I2S errors / timeouts / refill risks|0 /0 /0|
|PCM fallback blocks|1 BOOT; no unexpected-active/OTHER gap|
|Clipped / OOM / edit/Harmony errors|0 /0 /0 /0|
|Internal free / largest|153727 /77824B|
|PSRAM free / largest|31176952 /30932992B|
|Minimum captured stack margins synth/output/UI/USB|3648 /1364 /2052 /4144B|
|LVGL free / largest / peak used|95452 /95092 /32208B|
|Touch / flush errors|0 /0|
|USB connects / MIDI events / parse/dropped/overflow|1 /0 /0 /0 /0|

The service maximum10661us includes PCM enqueue pacing; it is not DSP render
time. DMA underruns remain UNMEASURED. Earlier0.10 idle had a disconnected USB
and different task allocations; subtracting its RAM values would not isolate
editor memory cost. No analog continuity/latency, touch appearance, eight-note
physical performance, NVS initialization/capacity or Save/Recall claim follows
from this partial idle capture.

## MANUAL / NOT RUN — remaining operator smoke

Use sound_editor_011_hardware_smoke.md: Moving Strings held chord/cutoff/Q,
Attack/Release, Compare/Revert, Save MY STRINGS01; DX7 operator/brightness and
Save MY DX PIANO; actual power cycle and Recall both; Harmony/sustain/Panic/eight
voices and reconnect. Physical persistent Save/Recall, Flash-cache behavior and
capacity/GC/power-cut tests are pending. Prolonged qualification remains separate.
No automatic30-minute requirement. No claim that Gate H or physical persistence
passed. Next smallest step is operator audition and actual power-cycle recall.

## Commands and evidence

All paths below are relative to this0.11 worktree. The local helper uses the
pinned PlatformIO Python/esptool, COM13, ESP32-P4 and921600 baud. It performs no
implicit erase, partition update or rebuild. Its write uses --after no-reset;
reads use --after hard-reset. Backups/logs/scripts/JSON are build/editor011/flash.

```text
C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 115200 --before usb-reset --after hard-reset flash-id
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/run_esptool.py build/editor011/flash/read-a-retry.log read-flash 0 0x1000000 build/editor011/flash/full_a.bin --no-progress
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/run_esptool.py build/editor011/flash/read-b.log read-flash 0 0x1000000 build/editor011/flash/full_b.bin --no-progress
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/preflight.py
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/run_esptool.py build/editor011/flash/write.log write-flash --no-progress 0x10000 build/editor_011_review_6052d5e8_d04db5da/guition-editor-8-bench/firmware.bin
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/run_esptool.py build/editor011/flash/readback.log read-flash 0 0x14b000 build/editor011/flash/postflash_prefix.bin --no-progress
C:/.platformio/penv/Scripts/python.exe build/editor011/flash/verify_readback.py
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/editor011/flash/boot.log --seconds 32 --reset
```

Validation of this receipt is git diff --check plus SHA/byte assertions and ZIP integrity; no firmware
rebuild is required for a documentation-only follow-up.
