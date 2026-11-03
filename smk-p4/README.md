# SMK-P4 — standalone AMY 0.2 e controlador híbrido

Standalone 0.2: `guition-standalone` now builds native AMY from the shared
`src/`, a local Performance backend and the existing PCM5102A driver. Four
voices; ROM 128 (DX7 BRASS 1) and 135 (DX7 PIANO 1); mono touch keyboard,
release/Panic, ROM browser and smoothed master. Unsupported macros stay disabled.
I²S remains **disabled by default** until wiring/supply/straps are confirmed.
Without I²S the engine renders to RAM and the screen says `AUDIO RAM`.
See [0.2 results and acceptance](docs/standalone_amy_performance_02_results.md)
and [wiring](docs/pcm5102_wiring.md). No UART/S3/HELLO in standalone;
native USB Host is opt-in behind its independent electrical qualification gate.

Standalone 0.3A adds deterministic owner-mailbox validation before USB startup.
The qualified SMK25 profile routes virtual cable 0 and counts ignored auxiliary
cables, handles event timestamp skew, and retains the USB handle briefly when a
transfer error precedes device removal. Notes are canceled immediately; an error
without confirmed removal still enters bounded recovery. Four total voices are
shared by USB and Touch, including sustained notes.
See [hardware validation, bench gates and recovery](docs/standalone_hardware_validation_03a_results.md).
PCM and USB remain disabled by default. The report distinguishes the safe build
from operator-qualified bench images and records the actual flashed profile;
software tests alone do not certify physical audio, MIDI or rehearsal readiness.

Firmware 0.3B prepares the existing musical baseline for a later UI integration.
It freezes the qualified configuration/hash reference and adds backend contract
regressions without changing the musical runtime or integrating UI Blueprint.
See the [backend contract](docs/performance_backend_contract.md),
[integration readiness and rollback](docs/firmware_integration_readiness_03b.md),
and [regression matrix](docs/ui_integration_regression_matrix.md).
Binary references are local artifacts; the verifier requires an explicit bundle
path and checks generated C configuration separately from the requested sdkconfig.

The preserved hybrid architecture is P4 touchscreen -> UART -> SMK-S3 Single
engine -> PCM5102A (`guition-link`); `guition-display` remains the diagnostic.
Neither uses the standalone runtime. No Layer B or new sequencer in this change.

The project includes an independent Gate 0/1 display test and a Performance
controller with the UART link enabled. See [results](docs/rehearsal_mvp_results.md).
Build and upload passed on the referenced COM13 unit on 2026-11-01. Runtime
refreshes and stable sampled heap were captured; physical picture/touch
confirmation was subsequently provided by the operator. The UART software
integration and Performance screen are implemented; physical UART and musical
acceptance remain pending. This is not yet a qualified rehearsal instrument.

## Hardware evidence

The user identified `C:/progs/P4SDM` as using the same physical board. That
project identifies JC4880P443C_I_W / JC-ESP32P4-M3, with previously checked
ST7701S + GT911 landscape rendering. A new ROM query on COM13 confirmed P4 ES
revision 1.3 and 16 MB Flash. JC4880P433 is not treated as a general alias.
This profile is for the referenced unit; do not use it for arbitrary P4 boards.
The operator confirmed correct image/touch/button/slider behavior on this unit.

## Build

Use PlatformIO Core 6.1.18, Python 3.10–3.13. The project pins pioarduino
55.03.36-1 / ESP-IDF 5.5.2 (same SDK as S3 and P4SDM) and LVGL 9.3.0 (MIT).
No global SDK upgrade is needed. Commands from the repository root:

```powershell
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display
```

The generic PlatformIO ES board supplies the chip target; the Guition display
profile is isolated in `components/board/`. This uses ESP-IDF, not Arduino.
Only the ES <3.0 image is built. PSRAM initialization is enabled; GPIOs/LDOs
come from the user's tested reference, not from a guessed JC4880P433 pinout.

## Identification without flashing

```powershell
C:/.platformio/penv/Scripts/python.exe -m serial.tools.list_ports -v
# Choose the P4's actual port. This query resets it; it does not write Flash.
C:/.platformio/penv/Scripts/python.exe -m esptool --chip esp32p4 --port COM13 --baud 115200 --no-stub --after hard-reset flash-id
```

The ROM query cannot identify PCB revision, LCD/touch controller or UART wiring.
Confirm those from the board and matching schematic. USB/JTAG is 303A:1001 on
this unit. Do not substitute the S3's USB-UART port or the P4 USB-OTG port.

## Gate 1 physical check

After preserving any firmware/data needed from the P4, flash only this P4:

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display -t upload --upload-port COM13
C:/.platformio/penv/Scripts/pio.exe device monitor --port COM13 --baud 115200
```

Expect a dark 800×480 screen, corner labels, a button, a slider explicitly
labelled display test, and a pointer following touch. Check corners and center,
drag slowly across the slider and off the button, then release. The button
counter must increment once per click. Confirm no duplicate rotation, inverted
axes, ghost touches or blank screen. Logs must show GT911 480×800, no touch or
flush errors, advancing refreshes, stable heap after initial warmup over 30 min.
Record cold boot separately from successful build. A display failure stops this
test; the independent S3 is unaffected. The diagnostic screen sends no musical commands.

LVGL uses a 51,200-byte partial RGB565 draw buffer in PSRAM, three native
768,000-byte driver framebuffers and bounded dirty-tile repair. It redraws
invalidated regions; there are no animations. Only the UI owner uses LVGL,
display and touch. Framebuffer reuse waits for two refreshes in the UI task.
The diagnostic environment initializes no P4 codec, amplifier, C6, SD, MIDI Host
or inter-board UART.

## UART bridge increment

`guition-display` retains the display diagnostic with the link disabled.
`guition-link` enables the UART client and Performance controller;
`smk-s3/` provides the `esp32-s3-link` backend. Both original build environments
remain available. Use the [bench wiring guide](docs/uart_wiring.md) for the
operator-confirmed profile and the [protocol spec](../shared/smk_protocol/README.md)
for messages, ACK limits, retries, leases and commands. Compile with:

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link
./tests/run_smk_link.ps1
```

The diagnostic slider is not a macro control. Performance uses only actual
backend macro names, support flags and values, with eight sliders, master,
previous/next patch, an eight-row paged factory browser and global Panic.
Unsupported macros and offline controls are disabled. MIDI activity comes
from USB MIDI events on S3; AUDIO RUN means frames are advancing, not measured
audio quality. ACKs explicitly show RECEIVED; snapshots represent control state
and do not certify that AMY applied a patch.

The single-finger keyboard spans C4–C5 initially, supports octave changes,
fixed velocity 100 and a defensive 10-second maximum hold. Release, leaving
the key, browser/patch/octave changes and link loss release its Touch source.
Sustain from the physical keyboard does not latch Touch notes. Reconnection
loads S3 state without replaying offline musical actions. Performance has been flashed to the P4;
the S3 backend has also been flashed to COM8. HELLO and advancing real state
snapshots are confirmed on both devices. On 2026-11-02 the operator confirmed
LINK ON/actual patch, sound and release from P4 touch keys. The operator later
clarified that only P4 was tested; physical SMK25 USB input remains untested.
The operator also confirmed enabled macros changing sound, master volume,
patch name/timbre changes and Panic stopping notes with normal playing afterward.
The physical SMK25 subsequently passed sound/release after rebooting S3 with
the keyboard attached; logs confirmed 869 USB events. The earlier hot-plug
failed with endpoint ESP_ERR_NO_MEM. After the headless update, three USB
disconnect/reconnect cycles passed with operator confirmation and continuous
logs: no S3 reboot, no ESP_ERR_NO_MEM, stable heap and normal playing afterward.
Sustain, P4 UART link-loss isolation/reconnection and soak still need acceptance.
Audio starvation/deadline counters increased between readings;
the triggering interaction has not been isolated. This is not full rehearsal qualification.

The S3 bridge profile now selects `CONFIG_SMKS3_P4_HEADLESS=y`: it keeps the
existing UI state/controller mappings, but uses a PSRAM DummyDisplay surface
without initializing the local LCD/SPI/DMA or starting its 8192-byte render
task. This reserves internal RAM for USB hot-plug. The original S3 environment
retains its local display. This update has been flashed: a runtime reading with
USB enumerated showed 41.5 KB internal free and a 28.7 KB largest block.
The hot-plug result applies to this measured headless profile; the original
local-display profile has not received that memory reserve. See the results report.
Existing generated sdkconfigs may need the
new option applied explicitly; verify `CONFIG_SMKS3_P4_HEADLESS` in the generated
header before uploading, rather than assuming a successful build selected it.

After confirming wiring and the actual S3 USB-UART port, upload separately:

```powershell
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link -t upload --upload-port <S3_PORT>
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link -t upload --upload-port COM13
```

The original `esp32-s3-devkitc-1` environment remains the fallback with UART
disabled. Perform the [physical acceptance procedure](docs/rehearsal_mvp_results.md)
before rehearsal, including physical MIDI, held/sustained notes, touch release,
Panic, loss/reconnect and audio diagnostics under load.

Host geometry and buffer-retirement checks (requires the same `g++` used by
the existing S3 regression runners):

```powershell
./smk-p4/tests/run_host.ps1
```

The host runner also uses Python's standard-library unittest for same-boot
measurement-window validation. Stabilization findings, firmware identities,
recovery and pending physical acceptance are in
[the 0.2 stabilization report](docs/rehearsal_stabilization_02_results.md).
The link profile opts in to `CONFIG_SMKS3_AUDIO_PHASE_PROBE_AT_BOOT` to measure
the S3 startup spike; generic/standalone defaults leave it off. It uses fixed
atomic counters, no per-block logs or allocations. On S3, `audio_status probe_on`
and `audio_status probe_off` toggle timing without resetting history.

Collect short diagnostic windows without reset or flash (close other monitors):

```powershell
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/rehearsal_windows.py --port COM8 --output build/window_A.log --scenario A-idle --seconds 32 --interval 10
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/rehearsal_windows.py --input build/window_A.log --output build/window_A_compare.json --scenario A-idle
```

`--probe` explicitly enables phase timing during collection and disables it
afterward; neither action resets counters. The JSON retains raw observations,
same-boot deltas and operator notes. It refuses changed boots/builds/configuration,
suspected counter resets, non-increasing uptime, incomplete snapshots and gaps
over60s. Legacy firmware lacks the new record delimiter and boot identity, so
its logs require the documented manual comparison. Maxima remain cumulative;
the tool never labels a difference of maxima as a window maximum. Auditory
confirmation remains MANUAL even when counter deltas are zero.

These cover all 384,000 pixel coordinate round trips, corners, clipping, dirty
repair across two/three buffers, page changes and refresh-counter wrap. They
do not certify physical display or touch. Hardware source and licensing:
[provenance](components/board/vendor/SOURCE.md).

Capture real boot/diagnostic bytes using the existing PlatformIO Python/pyserial:

```powershell
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/rehearsal_gate1_boot.log --seconds 30 --reset
```

The tool sends no commands or firmware. `--reset` explicitly restarts the P4;
omit it to observe the running firmware. Serial initialization evidence and
the operator's visual/touch observations must be recorded separately.

## Native AMY software validation

Native P4 USB-MIDI integration, independent Touch/USB queues, four-voice
ownership, sustain and opt-in electrical gates are documented in
[the standalone 0.3 report](docs/standalone_usb_audio_03_results.md).
Run `./smk-p4/tests/run_usb.ps1` for mock USB lifecycle/descriptor checks and
`./smk-p4/tests/run_amy.ps1` for real-AMY musical and concurrent-source tests.
Both physical output and USB remain disabled until their separate wiring gates
are qualified; successful tests/builds do not certify VBUS or analog audio.

```powershell
./smk-p4/tests/run_amy.ps1
```

This uses the actual shared C engine, verifies ROM/chords/release/Panic/restart,
checks heap calls during service and balanced tracked allocations on shutdown,
and tests the local backend plus real widget callbacks against mock LVGL.
It does not certify physical display/touch or analog audio. The optional
`CONFIG_SMKP4_AMY_RAM_SELF_TEST` runs the identical PCM diagnostic on P4,
requires I²S to be disabled and logs peak/RMS/clipping/release/Panic. Normal
Performance boot sends no notes. Compare captured real results using:

```powershell
python smk-p4/tests/compare_amy_pcm.py --host build/p4_amy02_runtime_final.log --p4 build/p4_amy02_ram_selftest_final.log
```

Select `CONFIG_SMKP4_PCM_DIAGNOSTIC` explicitly for the old tone/silence source;
`CONFIG_SMKP4_PCM_TEST_SEQUENCE` controls audible routing only in that mode.
Leave diagnostic/self-test off for musical use. GPIO approval and the physical
steps in the wiring guide remain required before enabling I²S or uploading an
image that drives the DAC pins. Firmware compilation is not musical acceptance.
