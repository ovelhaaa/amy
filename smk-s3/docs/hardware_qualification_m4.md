# SMK-S3 M4 — Hardware Qualification, Realtime Headroom & Transition Polish

Status: **protocol defined, host/CI verification done, hardware measurements pending**

This document is the reproducible test protocol for M4. It defines *what* to
measure, *how* to measure it, and how to separate:

```text
HOST VERIFIED      automated on the host with the real AMY engine
CI VERIFIED        128 / 256 ESP-IDF builds + host suites
HARDWARE MEASURED  collected on the ESP32-S3 with the serial console
```

No claim of "no clicks", "CPU is safe" or "headroom is ideal" is valid until the
hardware tables below contain real data.

---

## 1. Instrumentation semantics

The M4 instrumentation is deliberately small and realtime-safe.

### 1.1 Block budget

`block_budget_us` is derived from the active build, never hard-coded:

```text
budget_us = 1'000'000 * kBlockSize / kSampleRateHz

128 frames @ 48 kHz  ->  2666.7 us
256 frames @ 48 kHz  ->  5333.3 us
```

### 1.2 Render load

`render_load` is a **percentage in 0..100**, not a 0..1 fraction:

```text
avg_load = avg_render_us / block_budget_us * 100
max_load = max_render_us / block_budget_us * 100
```

The same equivalent workload must report a coherent percentage on both block
sizes. A 128-frame build is *not* assumed to be better or worse; see section 12.

`avg_render_us` / `max_render_us` measure the whole synthesis-owner service:
command application plus AMY render. They do **not** include the I2S write,
which runs on the separate audio task.

### 1.3 Headroom telemetry

Measured on the final post-master-gain 16-bit PCM block, in the same pass that
feeds the oscilloscope, so there is no second full-block scan:

```text
peak_abs_sample    max |sample| in the current window (0..32768)
near_clip_samples  cumulative |sample| >= 32106  (0.98 FS)
hard_clip_samples  cumulative |sample| >= 32767
```

`audio_status` also prints `peak_dbfs = 20*log10(peak / 32768)`.

### 1.4 What a reset clears

`audio_reset` (alias `audio reset`, or `diag_reset audio`) clears only runtime
audio metrics:

```text
audio_underruns          max_render_us        avg_render_us
frames_rendered          synth_pcm_starvations
synth_commands_dropped   synth_queue_high_water
synth_panics             synth_max_command_wait_us
peak_abs_sample          near_clip_samples    hard_clip_samples
```

It never clears USB connection state, MIDI counters or the panic total. Resets
are relaxed atomic stores, safe from any task.

---

## 2. Serial commands

```text
audio_reset                 clear the audio qualification window
patch_select <id>           load a factory or user patch
note_on <note> <vel> [ch]   inject a Note On (same event path as USB MIDI)
note_off <note> [ch]        inject a Note Off
macro_set <id 0..7> <0..127> set a macro position
panic                       all notes off / recovery
audio_status                compact qualification snapshot
memory                      internal + PSRAM free/largest block
audio status                space-alias of audio_status
```

Use `note_on` / `note_off` to build chords hands-free; the events travel the
production control path (arpeggiator/sequencer rules included).

Example `audio_status` output:

```text
=== AUDIO QUAL ===
block=128 rate=48000 budget_us=2666.7
avg_us=1240 max_us=2110 frames=48000
avg_load=46.5 max_load=79.1
voices=6
peak=32180 peak_dbfs=-0.2 near_clip=34 hard_clip=0
underrun=0 starvation=0
cmd_drop=0 queue_hwm=8 cmd_wait_max_us=240
int_free=... int_largest=... psram_free=... psram_largest=...
```

Copy the `=== AUDIO QUAL ===` block verbatim; it is designed to be pasted into
the results tables without further parsing.

---

## 3. Showcase patch set

Reuses existing factory IDs (no new patches):

```text
id    family        role
0     subtractive   brass
4     subtractive   strings
24    subtractive   bass
32    subtractive   lead
47    subtractive   pad (heavy)
128   FM (DX7)      brass
135   FM (DX7)      piano
142   FM (DX7)      bass
224   FM (DX7)      synth lead
246   FM (DX7)      pad (heavy)
248   FM (DX7)      FX
```

Minimum representative set for a full pass:

```text
0, 24, 32, 47, 128, 135, 246, 248
```

Factory patch neutral macro position is the patch's own `default_val`
(typically 64). Leaving macros untouched after `patch_select` reproduces the
factory baseline.

---

## 4. Test matrix

### Families

```text
Subtractive Bass / Lead / Pad
FM (DX7)
```

### Polyphony

```text
1 voice, 4 voices, 8 voices (or the patch's safe maximum)
```

For FM count the real operator cost, not just the voice count.

### FX

```text
dry, chorus, delay, reverb, drive, all FX
```

### Performance controls

```text
neutral macros
moderate macros
extreme safe macros (0 and 127)
continuous macro sweep
```

### Recommended run sequence

```text
audio_reset
patch_select <id>
# neutral: leave macros untouched
note_on 48 100 ; note_on 52 100 ; note_on 55 100 ; note_on 60 100
# hold 30-60 s, listen, then:
audio_status
note_off 48 ; note_off 52 ; note_off 55 ; note_off 60
panic
```

Use `knob_bank fx` + the SMK25 knobs (or `macro_set`) to select dry / single FX
/ all FX. For extreme macros, set every relevant macro to 0 or 127 and repeat.
For a continuous sweep, repeatedly step the macro (e.g. `macro_set 4 0..127` in
small increments) while a chord sustains.

---

## 5. Results tables (fill on hardware)

### 5.1 Per-configuration window

| patch | family | voices | FX | block | avg_us | max_us | avg_load | max_load | underrun | starv | cmd_drop | q_hwm | peak | near | hard |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| | | | | | | | | | | | | | | | |
| | | | | | | | | | | | | | | | |
| | | | | | | | | | | | | | | | |
| | | | | | | | | | | | | | | | |
| | | | | | | | | | | | | | | | |
| | | | | | | | | | | | | | | | |

### 5.2 128 vs 256 comparison (same patch / voices / FX)

| metric | block=128 | block=256 |
|---|---|---|
| latency (ms, = block/48000) | 2.67 | 5.33 |
| avg_us | | |
| max_us | | |
| avg_load % | | |
| max_load % | | |
| PCM starvation | | |
| cmd_wait_max_us | | |
| queue_hwm | | |
| peak / near / hard | | |
| subjective stability | | |

---

## 6. Memory qualification

Record the `memory` block before/after each event:

```text
memory
```

| event | int_free | int_largest | psram_free | psram_largest |
|---|---|---|---|---|
| boot (idle) | | | | |
| first chorus activation | | | | |
| first delay activation | | | | |
| first reverb activation | | | | |
| heavy patch (8 voices, all FX) | | | | |
| after 100 patch switches | | | | |

Expected allocations (not leaks):

- chorus/delay/reverb allocate their PSRAM buffers lazily on first use and keep
  them for the lifetime of the engine.
- PCM drum block is allocated once at boot.

Investigate only *continuous growth* or a shrinking `*_largest` block across
repeated activate/deactivate cycles. One-time lazy allocation is expected.

---

## 7. Gain-staging audit

Using the peak telemetry, measure each combination (one chord, then a mono
note):

```text
dry
drive only            (drive 0.0 / 0.25 / 0.5 / 1.0)
chorus only
delay only
reverb only
all FX
all FX + 8 voices
```

Record peak / near / hard for each. Only change gain staging if the data shows:

```text
hard clipping
excessive near clipping with no musical reason
a large level jump when an effect is enabled
```

Preferred fixes are simple and local (a fixed compensation factor at the effect
insert), never a new compressor/limiter/mastering chain.

Expected drive behavior (confirm by ear **and** peak):

```text
drive 0.0    bit-neutral (no saturation path)
drive ~0.5   saturation without a large level jump
drive 1.0    clearly distorted but controlled
```

---

## 8. Parameter transition audit

Classification:

```text
A  naturally continuous / AMY already handles it
B  perceptible step but acceptable
C  real risk of zipper/click
```

| parameter | class | notes |
|---|---|---|
| filter cutoff | B | Coefficient updates are block-rate; large jumps can transient. |
| filter resonance | B | Same as cutoff; high Q emphasises steps. |
| chorus depth | B | Masked by the chorus LFO; small level step. |
| reverb size | B | Diffuse tail hides small coefficient steps. |
| reverb mix | C | Gain-like crossfade; candidate for a short ramp. |
| delay mix | C | Gain-like; candidate for a short ramp. |
| delay feedback | B | Feedback smooths the change over repeats. |
| delay time | C (worst) | Moves the read pointer directly; see section 9. |
| drive | C | Waveshaper gain; candidate for a short ramp. |
| master tone | C | Shelving EQ gain; candidate for a short ramp. |
| FM mod index / ratio | B | Timbre change is expected; ratio is a pitch step. |
| osc detune | B | Small cents step. |
| envelope A/D/S/R | B | Applied on next note; sustain jump while held. |
| waveform | B | Discrete; can click if changed while sounding. |
| preset load | A | `synth_delay_ms = 4` micro-fade + all-notes-off. |

### 8.1 M4 smoothing decision

M4 does **not** add smoothing. Rationale:

- the audit is a hypothesis until hardware listening confirms zipper/click;
- a generic "universal smoother" is explicitly forbidden (it can delay
  controls, break soft takeover, break neutral macros and add command traffic);
- any added ramp must be justified by measured evidence.

If hardware confirms risk on the class-C gain-like parameters, M5 should apply a
**targeted, bounded, deterministic** block-rate one-pole ramp in the synthesis
owner only, fed by the final effective target:

```text
manual + macros = effective target
        |
        v
optional per-parameter ramp (synthesis owner, per block, no heap)
        |
        v
DSP
```

Never smooth manual and macro contributions separately. A ramp must converge to
the exact target (snap when within epsilon) so save/reload and neutral macro
reproduction still hold bit-exactly.

---

## 9. Delay-time transition decision

AMY uses a fixed delay line; changing `echo_delay_ms` moves the read pointer
directly, so a large change can produce a discontinuity. Changing between
`1/16, 1/8T, 1/8, 1/8D, 1/4, 1/4D, 1/2` can therefore click.

M4 decision: **do not redesign the delay**. Options A (small rate-limited
delay-time steps) and B (brief wet fade -> change -> restore) are both more
complex than the milestone allows and neither is justified without hardware
evidence.

```text
Known limitation:
  large delay-time jumps can create a discontinuity.
  A later milestone may add rate-limited stepping or a wet fade.
```

---

## 10. Command queue stress

Drive the system hard and watch the existing counters:

```text
rapid macro sweeps        (macro_set 0..127 repeatedly)
rapid physical knob sweeps
pitch bend
multiple held notes
FX controls
```

Acceptance:

```text
cmd_drop == 0
queue_hwm comfortably below kSynthCommandCapacity (64)
cmd_wait_max_us bounded (no sustained growth)
```

Do not enlarge the queue to hide an excessive producer.

---

## 11. Realtime acceptance

During a nominal hardware window:

```text
audio_underruns      == 0
synth_pcm_starvations == 0
synth_commands_dropped == 0
```

If any is non-zero, investigate in this order before enlarging buffers:

```text
render CPU (avg/max load)
command traffic (queue_hwm, cmd_wait_max_us)
voice count
memory allocation
```

---

## 12. 128 vs 256 comparison

Do not assume 128 is better. Compare on the **same** patch/voices/FX:

```text
latency          (block / 48000)
CPU percentage   (avg_load, max_load)
max load
starvation
command latency  (cmd_wait_max_us)
stability        (underruns over a long window)
```

Because the budget is derived per build, `avg_load` is directly comparable
between the two. Do not change the default block size in M4 without evidence.

---

## 13. Audition checklist

Answer each by ear while a `=== AUDIO QUAL ===` window is open:

```text
Does the patch change volume a lot when chorus is enabled?
Does drive keep a reasonable sense of volume?
Does reverb glue to the dry signal or sound detached?
Does SPCE turn into a wash too early?
Does MOTN detune too much?
Does the CHAR macro stay useful across its whole range?
Do the FM macros keep the preset's identity?
Is there a click when changing delay subdivision?
Is there zipper noise when turning tone / drive / wet?
Are notes cut under fast chords?
```

Any curve that feels aggressive may be tuned by:

```text
reducing range
changing curve
changing compensation
```

Prefer parameter tuning over rewriting subsystems. The eight macros keep their
M2 names and semantics.

---

## 14. Known limitations

- Smoothing is not implemented in M4 (section 8.1).
- Large delay-time jumps can produce a discontinuity (section 9).
- Version string is a maintained project constant, not git-derived
  (`PROJECT_VER` is unset); automated versioning is out of M4 scope.
- `p99` render time is not tracked; `avg + max` are considered sufficient.
- All numeric results above are placeholders until measured on hardware.
