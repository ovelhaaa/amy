# Factory library 0.10 — short physical smoke

Musical smoke: **MANUAL / PASS**, aggregate operator confirmation on 2026-11-07: “smoke passou, pode fazer push”. Individual checklist outcomes and active-play counter deltas were not provided; the rows below remain the original procedure, not individually confirmed results. Subsequent authorized application write/readback and observed boot/idle: PASS in the [physical follow-up](factory_library_010_hardware_qualification.md). Building and host qualification do not authorize upload. No automatic thirty-minute window is required. Prolonged qualification remains PENDING separately.

Before writing, match the candidate manifest SHA256, ESP32-P4 ES1.3 JC-ESP32P4-M3 board, actual port/USB serial, currently installed app and partition table. Preserve two matching full-flash reads plus the extracted application and hashes. Obtain explicit authorization for the concrete application SHA and app-only erase interval. Keep bootloader, partitions and NVS unchanged. The existing GPIO50 BCLK /51 WS /52 DATA and USB HS0 are the previously validated bench profile, not newly assigned final wiring; reconfirm PCM module straps, supply, ground, line output and protected VBUS.

After an authorized write and independent application readback:

|Step|Action|Expected|Status|
|---|---|---|---|
|1|Cold boot|FACTORY 0.10 identity, patch128, Harmony OFF, zero occupied gates|NOT RUN|
|2|Open Browser, reach final page and return|256 unique ROM IDs, unavailable rows blocked, correct active marker|NOT RUN|
|3|Filter JUNO A/JUNO B/AMY DX7 and categories; toggle favorites|RAM favorites, correct matches; no UI freeze|NOT RUN|
|4|Audition approved Strings, Brass, Bass, Lead, Piano, E.Piano, Organ and FX|Correct selected name and timbre; no obvious saturation/noise|NOT RUN|
|5|Hold eight notes, repeat Prev/Next across families|Bounded silence during change permitted; confirmed patch retained until load; no stale notes|NOT RUN|
|6|C Major Triads: C then E, release C|Six then three occupied gates|NOT RUN|
|7|Change Juno to DX7 while group/sustain held|Old groups cleared; new triad uses new patch; eight total voices shared|NOT RUN|
|8|Sustain, bend, CC1 and Touch/USB overlap|Independent source/channel ownership; normal release|NOT RUN|
|9|Panic during playing/loading|No stuck notes; subsequent fresh notes work|NOT RUN|
|10|Disconnect/reconnect SMK25 during playing/change|USB notes canceled; reconnect without full reboot where feasible|NOT RUN|
|11|Short random selection including highest host DSP averages|No OOM/corruption; heap/largest-block settle after warmup|NOT RUN|
|12|Navigate all14 screens while playing|Responsive UI; fixed128KiB LVGL pool; no unexpected active gaps|NOT RUN|

Capture before/after deltas for render/compute deadline counts, output GAP09 causes, I2S write errors/timeouts/refill risks, frames, queue high-water/overflow, USB reconnect/parse/drop counts, clipping, Harmony errors, internal RAM/PSRAM free/largest, and task stack margins. Record operator listening separately. Zero I2S errors does not certify analog continuity, underruns or latency. Unexpected READY-active gaps, memory corruption or stuck notes require correction before calling this an experimental musical smoke PASS.

Read-only serial collection (no reset requested):

```powershell
C:/.platformio/penv/Scripts/python.exe smk-p4/tests/capture_serial.py --port COM13 --output build/factory010/smoke.log --seconds 45
```

Choose the actual confirmed port. Existing0.9 physical acceptance does not count as0.10 acceptance. No gapless switching, physical per-patch cost class, low-latency measurement or prolonged reliability claim is made.
