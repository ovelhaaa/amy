# SMK-P4 0.15.2 — Selective Patch Loading & Performance Preservation

## 1. Overview

In SMK-P4 0.15.2, patch selection introduces **Performance Lock**: a selective preservation mechanism that allows musicians to switch synth timbres without interrupting their performance context.

```text
                    SELECT PATCH (ROM 000..255)
                                │
                  ┌─────────────┴─────────────┐
                  ▼                           ▼
          CLEAN LOAD (0x00)          LOCKED LOAD (mask)
          ─────────────────          ─────────────────
          • Default FX               • Keep FX (Chorus, Delay, Reverb, EQ)
          • Default Harmony          • Keep Harmony / Chord Assist
          • Reset Arpeggiator        • Keep Arpeggiator (Rate, Mode, Oct, etc.)
          • Default Factory Routes   • Keep Macros (if routes valid for family)
```

---

## 2. Lock Group Specifications

### 2.1 Keep FX (`perf_lock::Fx` = 0x01)
- **Preserved:**
  - `delay_mode` (Free / Sync)
  - `delay_division` (1/4 .. 1/16T)
  - `delay_time_ms` (10 .. 1000 ms)
  - `delay_feedback` (0 .. 0.95)
  - `delay_mix` (0 .. 1.0)
  - `chorus_mode`, `chorus_depth`
  - `reverb_size`, `reverb_mix`
  - `eq_high_db`, `eq_mid_db`, `eq_low_db`
- **Dirty flag:** Marked dirty (`state_.fx.dirty = true`) so the user can save the resulting combination into a User Preset slot.
- **Audio safety:** During patch transition, AMY drops and rebuilds synthesis voices while bus FX config is preserved without reallocation.

### 2.2 Keep Harmony (`perf_lock::Harmony` = 0x02)
- **Preserved:**
  - `scale_on`, `scale`
  - `chord_on`, `chord`
  - `root`, `quant`, `inversion`, `open`, `spread`
- **Audio safety:** Touch/MIDI input queues are flushed through the patch change barrier; no stuck chord notes or stale voice leases survive.

### 2.3 Keep Arpeggiator (`perf_lock::Arp` = 0x04)
- **Preserved:**
  - `enabled`, `rate`, `mode`, `octaves`, `gate`, `swing`, `latch`, `restart`
- **Discarded / Reset:**
  - `sounding = false`, `held_count = 0`, `length = 0`, `step = 255`
  - Held and latched notes are released before the patch swap.
  - No stuck arpeggiator notes across the timbre boundary.

### 2.4 Keep Macros (`perf_lock::Macros` = 0x08)
- **Validation:**
  - When switching patches within the same family (e.g. DX7 to DX7), custom routes and knob positions M1..M8 are preserved.
  - When switching across families (e.g. DX7 FM to Juno Subtractive), destination parameters are checked against `destinationFamilies(dest) & family` and `available & edit::bit(dest)`.
  - If any custom route targets an unavailable parameter (e.g. operator attack on a subtractive synth), the custom routes are safely discarded and replaced with factory default routes for the target patch.

---

## 3. Real-Time Ownership & Concurrency

- **Command Delivery:** `amy::Command` carries `uint8_t preserve_mask`.
- **Generation Barrier:** Patch loading advances `commands_.panic(OutputReason::Patch)`. All in-flight traffic is drained.
- **Quiescent Reconstruction:** Voices are reconstructed on the audio worker thread via `loadReusable()` with zero heap allocations in render.
- **Session Defaults:** `kDefaultLockMask = Fx | Harmony | Arp` (0x07).
