# Harmony0.9 - authorized application write and boot observation

Operator authorization: **"pode graar"**, referring to the concrete0.9 audible
candidate delivered in this chat. Application-only scope; existing bench wiring
GPIO50/51/52 and USB HS0 retained. No electrical configuration was changed.

## PASS - identity, fresh backup and preserved baseline

COM13 USB serial/MAC80:F1:B2:D3:CC:86 identified ESP32-P4 v1.3,16MiB Flash.
The installed application matched the preserved0.8 SHA256:
`a5b410c29b677fcdc894ee78b1f2a22c64d98594143f460edd7800a4965e8617`.
The3072-byte partition table matched the0.9 candidate.

Two independent complete16MiB reads matched SHA256:
`f8522b50f777223eeb4f3127c959020b17e8f972c03c10eeb5fbd78741e28e27`.
Fresh full-flash/app/partition rollback archive was ZIP-tested and its internal
file hashes independently checked before writing. Archive SHA256:
`b38fffc8434c796716ba7aaede0a8f1401e84c4679806e58769abda9d5e35edb`.
Original0.8 worktree, application and earlier backups remain untouched.

## PASS - exact application write and independent readback

Authorized candidate:1218592B, SHA256:
`ebcbc081d287958e466a22fb063c22d4dd2542f59c9cc7758c4f1c17ed6f3f9c`.
Application written at0x10000; esptool erased only0x10000..0x139fff and verified
the written data hash. No bootloader, partition-table or NVS write requested.
An independent read of0..0x139fff confirmed that the entire pre-application64KiB
was byte-identical to the fresh backup and the application was byte-identical
to the authorized candidate. Bootloader, partition table and pre-app NVS region
were preserved. Flash outside the application erase interval was not requested
for writing; no full post-write16MiB comparison is claimed.

Using pinned esptool5.1.0 through PlatformIO Python, commands included:

```text
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 460800 --after no-reset flash-id
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset read-flash 0 0x1000000 <backup> --no-progress
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset write-flash --no-progress 0x10000 build/harmony_09_review_75c124ca_7d3fb377/guition-harmony-8-bench/firmware.bin
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset read-flash 0 0x13A000 build/harmony09/flash/postflash_prefix_and_app.bin --no-progress
```

The first full backup used `--trace` to observe transfer progress; the second
used the same read without trace. Earlier read attempts were interrupted before
completion; they did not write Flash and were not counted as verified backups.
Raw completed-read/write/readback logs and authorization/preflight/readback JSONs
are in `build/harmony09/flash/`. The serial observation used pyserial with DTR/RTS
false,115200 baud and a bounded25-second read; the port was closed afterward.

## PASS for observed boot/idle counters only

|Metric|Last captured observation|
|---|---:|
|AMY ready / confirmed patch / occupied gates|1 /128 /0|
|USB connects / disconnects / MIDI events|1 /0 /0|
|USB parse errors / dropped / queue overflow|0 /0 /0|
|Rendered/physical frames written|998400|
|Render average / maximum|195 /310us|
|Render / compute deadlines|0 /0|
|I2S write errors / timeouts / output refill risks|0 /0 /0|
|Output fallback blocks|1, classified BOOT|
|Patch/Panic/stale/unexpected-active/OTHER gaps|0 /0 /0 /0 /0|
|Clipping / OOM / Harmony errors|0 /0 /0|
|Internal free / largest block|159627 /81920B|
|PSRAM free / largest block|31176952 /30932992B|
|Minimum captured stack margin: synth / output / UI / USB|3888 /1368 /2772 /4144B|

No crashes/backtrace were observed. Display diagnostic counters reported zero
Touch and flush errors, but screen appearance/tactile behavior was not confirmed
by the operator. Boot is safe with Harmony configuration revision0 and no admitted
groups. This capture had no MIDI notes or patch changes, so zero unexpected gaps
is only an idle observation, not sustained active-play qualification. Hardware
DMA underruns and analog continuity/latency remain UNMEASURED.

## MANUAL / NOT RUN

Operator auditory/musical smoke remains pending: normal keyboard, C/A scales,
triads/sevenths, capacity rejection, sustain, bend/CC1, Touch, patch changes,
Panic and USB reconnect. Follow [the short procedure](harmony_09_hardware_smoke.md).
Do not inherit0.8 smoke as approval of0.9. Prolonged qualification is separately
PENDING; this write/boot result is not professional-stage or gapless-audio approval.
The original review manifest remains a preflash historical snapshot; this report
and the new raw evidence record the subsequent authorized write and verification.
