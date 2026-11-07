# Factory library 0.10 — authorized application write and idle observation

Operator authorization: **“autorizada”**, referring to the concrete audible candidate offered in this chat. The previous software review manifest is an immutable preflash snapshot; this follow-up records the subsequent authorization/write. No new build or application change was introduced.

## PASS — identity, exact image and rollback

COM13 USB serial/MAC `80:F1:B2:D3:CC:86` again identified ESP32-P4 ES revision1.3,16MiB Flash. The existing two identical complete Flash reads and extracted0.9 app/partitions were rehashed before writing. A fresh1314816-byte read immediately before writing matched the same prefix of that verified full backup, including bootloader, partitions, NVS and current application.

Preserved full Flash SHA256:
`9a7589434ba52614dfc4b19d1b6a11c76536f471fbfb6f2f976e5f219ec9c30a`.
Preserved0.9 application SHA256:
`ebcbc081d287958e466a22fb063c22d4dd2542f59c9cc7758c4f1c17ed6f3f9c`.
The source candidate package and all earlier recovery artifacts remain unchanged.

## PASS — app-only write and independent readback

Authorized image:1249104 bytes, SHA256:
`0652377914fce06a385bab4388049fac0d427e90eac204252f08c05144f8adc7`.
Source commit: `d91857ccf6b85e5fe5e81d62fabf2c3aa8ec588b`.

The pinned esptool5.1.0 wrote only the application at `0x10000`. Its log confirms erase interval `0x10000..0x140fff`,1249104 bytes written and data hash verified. No bootloader, partition or NVS write was requested. A separate read of `0..0x140fff` then confirmed:

- application bytes exactly equal the authorized image;
- the entire pre-application64KiB exactly matches the fresh preflash read;
- bootloader, partition table and NVS in that prefix are unchanged.

No complete post-write16MiB comparison is claimed. Flash outside the application erase interval was not requested for writing.

Executed commands (from the0.10 worktree):

```text
C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset read-flash 0 0x141000 build/factory010/flash/preflash_prefix.bin --no-progress
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset write-flash --no-progress 0x10000 build/factory_010_review_d91857cc_e9e7f2d9/guition-factory-8-bench/firmware.bin
C:/.platformio/penv/Scripts/python.exe C:/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32p4 --port COM13 --baud 921600 --before usb-reset --after hard-reset read-flash 0 0x141000 build/factory010/flash/postflash_prefix.bin --no-progress
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/factory010/flash/boot.log --seconds 30 --reset
```

## PASS — observed running boot/idle only

The capture contains a partial early image-load line and periodic running snapshots at approximately17–43 seconds uptime. It is not a complete early-boot trace or cold power-cycle qualification. The observed AMY initialization admission passed and I²S frames advanced. No crash/backtrace, OOM or failed render was observed.

|Metric|Last captured value|
|---|---:|
|AMY ready / patch / occupied gates|1 /128 /0|
|Physical frames written|1983232|
|Render average / maximum|193 /436us|
|Render / compute deadline counts|0 /0|
|I²S errors / timeouts / refill risks|0 /0 /0|
|PCM fallback blocks|1, classified BOOT|
|Patch/Panic/stale/unexpected-active/OTHER gaps|0 /0 /0 /0 /0|
|Clipped / OOM / Harmony errors|0 /0 /0|
|Internal free / largest block|163087 /90112B|
|PSRAM free / largest block|31177688 /30932992B|
|Minimum captured stack margins: synth/output/UI/USB|3888 /1364 /2772 /5520B|
|LVGL free / largest / peak used|95452 /95092 /32208B|
|Touch / flush errors|0 /0|
|USB connects / disconnects / MIDI events|0 /0 /0|

The synthesis service maximum11171us includes PCM enqueue pacing; it is not a measured11171us DSP render. DMA underruns remain explicitly UNMEASURED. The comparable old0.9 idle-after-read capture had163319B free internal RAM; this0.10 observation is232B lower, consistent with the static data/BSS difference, but this does not characterize fragmentation under playing/patch changes.

## MANUAL / NOT RUN — musical smoke

No note, patch change, USB MIDI event or Harmony group occurred in this capture. Zero idle error counters do not qualify eight-note DSP performance, analog continuity/latency, MIDI reconnect, touchscreen appearance or musical usability. The physical board/bench profile was retained (GPIO50/51/52, USB HS0); no wiring change was introduced. The229 host-admitted patches remain experimental candidates.

Next smallest step: operator runs the [short musical smoke](factory_library_010_hardware_smoke.md), checking Juno/DX7, browser/PrevNext, eight notes, Harmony, sustain, bend/CC1, Panic and USB reconnect. Report audition and counters independently. Prolonged qualification remains PENDING; no automatic30-minute requirement.

Authorization, fresh prefix, write/readback logs/bytes, boot capture and hashes are in `build/factory010/flash/`. A ZIP-tested supplemental receipt is `build/factory010/factory_010_authorized_flash_receipt.zip`; it does not replace or mutate the original software candidate/rollback bundle. No AMY core, source configuration or application binary was changed during this write task. Validation of this documentation change is `git diff --check`; no firmware rebuild is necessary.
