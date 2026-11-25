# P4my boot identity

## Implemented and architecture

Independent `P4mySplash` in `components/ui_blueprint/include/p4my_splash.h`
and `src/p4my_splash.cpp`. The operator reference now drives the composition:
italic curved wordmark, 4 with amber halo, full-width sine/triangle waves,
centered upper/lower radial tick arcs, four corner tick clusters, two amber
rails, tracked subtitle at y=282 and italic AMY credit at y=320.
The reference is archived as `operator_reference.png`; it is not rendered by
firmware. Existing AMY attribution and licenses are retained.

Scope follows the explicit P4 request (800 x 480), superseding only the older
S3/284 x 76 scope in root specs.md/AGENTS.md for this task. S3, AMY core, DSP,
MIDI, audio drivers and the Performance layout are unchanged. Existing local
UI redesign changes are outside this commit. The reference was supplied after
the first iteration and the final LVGL capture was visually compared with it.
This is a close reconstruction, not a pixel-identical copy (RGB565 output,
LVGL subtitle font and sampled wave lines differ from the reference).

The existing standalone control/LVGL owner mounts the overlay above the built
Performance tree. It continues servicing the backend and display. No timer,
worker, delay, audio callback or cross-task LVGL access is introduced. A real
engine-ready snapshot (or a reported engine error) requests a bounded 180 ms
fade. No fake boot status is displayed. The musical initialization does not
wait for the timeline. On already-ready boots most of the nominal animation
will intentionally be skipped. Diagnostic and legacy non-Blueprint UI remain
unchanged. Nominal completion holds the final composition until readiness.

The built-in LVGL allocator receives a startup preflight (20 KB free, 2 KB
largest block); a depleted pool skips the optional overlay and reports failure.
This headroom is a guard, not a physical memory qualification.

The grid and 56 technical tick/rail segments use one LVGL draw callback with
bounded loops and flash-resident coordinates. Drawing stays on the UI owner;
there are no explicit frame buffer allocations in this callback.
All waveform points live in three fixed 161-point arrays shared by five mutable LVGL
lines. LVGL objects/styles allocate at begin, not explicitly each frame. The
phase slows exponentially. The firmware instance is static: its fixed point
storage remains reserved after close; all LVGL objects/styles are released. `close()`, repeated begin, destruction and external
parent deletion are supported; no timers can remain orphaned. The splash owner
must outlive its LVGL objects and be destroyed before LVGL shutdown.

## Configuration and preview

- `CONFIG_SMKP4_BOOT_SPLASH=n`: disables the boot overlay.
- `CONFIG_SMKP4_SPLASH_FPS=20`: reduced update rate; 21-30 select 30 FPS.
- `P4mySplash::Config`: duration (default 2700 ms), exit duration (0-1000 ms),
  frame rate (20 or 30). Invalid config fails before object creation.
- `seek(elapsed_ms)`: deterministic manual playback, with the nominal timeline
  scaled by configured duration. It does not advance the owner clock.

Nominal reveal: grid 0-400, secondary waves 200-900, amber 400-1200,
logo 900-1800, subtitle 1800-2300, AMY 2300-2700 ms.

From the repository root, using the existing GCC/MinGW and LVGL 9.3 checkout:

```powershell
powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_splash.ps1
powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_splash.ps1 -Manual
powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1
# Capture/integration path independently of the existing regression suite:
powershell -ExecutionPolicy Bypass -File smk-p4/ui-preview/run_preview.ps1 -PreviewOnly
$env:PYTHONIOENCODING='utf-8'
$env:PYTHONUTF8='1'
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-editor-8 -j 8
```

The splash runner reuses the existing LVGL host archive preparation and compiles
with `-Wall -Wextra -Werror`. Python/Pillow exports PNG/GIF from actual RGB565
LVGL captures; it is a host-only dependency already available on this machine,
with no firmware memory/Flash/real-time impact. Tested export dependency:
Pillow 12.3.0, MIT-CMU (installed package metadata), source
[python-pillow/Pillow](https://github.com/python-pillow/Pillow). Maintenance
stays with upstream; no library code is vendored or patched. The host pin is
`ui-preview/requirements-splash-preview.txt`; install it only if needed.
Pillow is required by this export helper, not by the module or firmware.
Existing preview already provides PNG export; Pillow supplies GIF packing here.
The GIF helper holds 91 RGB frames (~100 MiB pixel storage, estimate, not RSS).
Firmware memory/Flash dependency impact is zero; no audio-time code is added. `build/p4my_splash/replay.html`
provides Play/Pause and a time slider over 91 captured frames (no simulated UI).
The four committed 800 x 480 milestones are 0, 700, 1400 and 2700 ms. GIF is
review-only; firmware renders primitives. GIF delay granularity may round the
33 ms frame duration; the HTML replay uses 33 ms nominal scheduling.

## Validation

See validation.txt for measured results and actual command status.
The standalone splash harness checks early finish/idempotence, repeated
lifecycle/heap balance, tick wrap, parent deletion, invalid configuration and
preservation of the underlying object. The normal preview additionally tests
exit over the genuine UiBlueprint Performance tree before its existing full
navigation/capture regression. No firmware was uploaded.

## Finalization & Hardening (baseline `a8fd098`)

- **Lifecycle State Machine:** `Inactive` → `Animating` (2700 ms) → `FinalHold` (250 ms) → `WaitingForEngine` (bounded `wait_timeout_ms=8000`) → `Exiting` (180 ms fade) → `Closed`.
- **Musical Readiness & Safe Dismiss:** `setEngineReady()` is decoupled from animation progress. Early dismiss is supported via intentional touch (`LV_EVENT_PRESSED` with bubbling stopped), USB MIDI Note On (`vel > 0` via lock-free atomic counter in `UsbInput`, passing the note through to AMY untouched), explicit `finish()`, or `setEngineError()`. MIDI Clock (`0xF8`), Active Sensing (`0xFE`), and Note On with `vel == 0` do not dismiss.
- **Render Optimization:** Timeline clamps at `duration_ms`; final frame renders once and freezes waveforms/styles during `FinalHold`, `WaitingForEngine`, and `Exiting`.
- **P4my Identity:** Header badge (`P4my` / `P4my SIM`) and `MAIS > DIAGNOSTICOS > SISTEMA` About view (`P4my - Digital Performance Synth | Powered by AMY`, FW version, ESP32-P4 target, AMY credits, build info).
- **Physical Qualification Procedure (Guition ESP32-P4):**
  1. Flash `guition-editor-8` (or `-bench`) only after explicit operator authorization.
  2. Cold boot: verify full 2700 ms P4my reveal + 250 ms hold + 180 ms fade to Performance; check serial `SPLASH_DONE` log (`dur_ms`, `ready_ms`, `reason`, `frames`, `waves`, `render_avg_us`, `render_max_us`) and zero `AUDIO` underruns/deadlines.
  3. Repeat cold boot and tap touchscreen mid-animation: verify immediate fade without triggering underlying Performance controls.
  4. Repeat cold boot and play a note on SMK25 V2: verify immediate fade AND audible note with zero dropped/duplicated Note On.

## Hardware assumptions, risks and next smallest step

No new wiring/controller assumptions. Uses the existing P4 800 x 480 port.
Additional PSRAM/style/software drawing load requires board measurement;
host timings do not prove physical render deadlines or audio stability.
Kconfig changes may require refreshing an existing PlatformIO generated
configuration; verify BOOT_SPLASH and SPLASH_FPS in config/sdkconfig.h.
The animation starts after the existing display and UI setup: it cannot cover
work before display initialization, and does not move that initialization.

MANUAL (not executed; no upload authorized): after an independently authorized
firmware deployment, cold boot with the current qualified wiring. Verify no
stalled Performance entry, correct landscape/colors, responsive touch after
fade, normal MIDI notes/release/Panic, and no regression in same-boot audio
counters. Measure internal/PSRAM before/during/after, UI frame duration and
underruns over repeated boots, at 30 and 20 FPS. Repeat with BOOT_SPLASH=n.
Expect no splash in diagnostic/legacy profiles and no changes to stored patches.
Next smallest step: operator review of the updated captures, then qualification
of render cost on the actual board without changing audio.


## Wordmark artwork provenance and regeneration

`src/p4my_wordmark.inc` stores five small native LVGL A8 masks (P, 4, my,
4 halo, and the AMY credit), 36,140 bytes of immutable pixel data. There is no
full-screen raster animation, font engine or GIF dependency in firmware.
A8 images blend at native dimensions; their opacity animates with the existing
timeline. Font binaries and font tables are not embedded or distributed.

The fixed text artwork was rendered locally with the installed Windows Arial
Italic (104 px for the wordmark, 17 px for credit), supersampled 4x. Its installed
name-table permission covers creating/displaying content; the font software
itself remains on the operator machine. No font binary was copied into the repo.
The source generator accepts a separately installed font through `--font`:

```powershell
python smk-p4/ui-preview/generate_wordmark.py --font C:/Windows/Fonts/ariali.ttf
```

Pillow 12.3.0 is the same pinned host dependency already used for capture export.
The generated include is committed, so normal firmware and host builds need no
font or regeneration. Different input fonts change the wordmark; this is artwork
regeneration, not a runtime fallback. LVGL keeps the subtitle as ordinary text.
