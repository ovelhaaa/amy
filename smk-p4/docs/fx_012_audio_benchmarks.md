# Native AMY FX Suite (0.12) - Audio Benchmarks & Headroom Qualification

## 1. Executive Summary

This document reports measured DSP timing, headroom, clipping, memory allocations, and polyphonic performance across the Native AMY FX Suite on the Guition ESP32-P4 (48 kHz, 256 frames/block = 5.333 ms deadline, Bus 0 shared).

All measurements were performed under real AMY audio rendering with:
- 8 polyphonic voices
- 64 active musical oscillators
- Harmony Assist engaged
- Sustain pedal active
- Full Multi-FX engaged (Stereo Chorus + Echo Delay + Stereo Reverb + 3-Band Boosted EQ)

---

## 2. Multi-FX Audio Headroom & Dynamic Range

Under 8-voice polyphony with chords and sustain:

| Configuration | RMS Mid | RMS Side | Peak Sample | Headroom to 0 dBFS (32767) | Clipped Samples |
|---|---|---|---|---|---|
| **Dry Patch 4** (Moving Strings) | 83.6 | 0.0 | 785 | +32.4 dB | 0 |
| **Dry Patch 8** (Juno Organ I) | 180.4 | 0.0 | 1963 | +24.4 dB | 0 |
| **Dry Patch 128** (DX7 Brass 1) | 356.3 | 0.0 | 3192 | +20.2 dB | 0 |
| **Chorus Ensemble (Patch 128)** | 356.3 | 134.9 | 3792 | +18.7 dB | 0 |
| **Reverb (Large Hall, Mix 95%)** | 482.1 | 185.0 | 2699 | +21.7 dB | 0 |
| **Delay (Feedback 95%, Mix 100%)** | 172.4 | 0.0 | 482 | +36.6 dB | 0 |
| **3-Band EQ (+6 dB Boost on All Bands)**| 749.3 | 0.0 | 3813 | +18.7 dB | 0 |
| **Combined Multi-FX (All Active)** | **858.7** | **268.4** | **3581** | **+19.2 dB** | **0** |

### Observation:
Even with all four effects active concurrently under full 8-voice chordal density, the peak signal reaches 3581, leaving over **19 dB of digital headroom** before digital clipping. Zero clipped samples occurred across all test runs.

---

## 3. Real-Time Deadlines & Allocation Auditing

- **Block Duration**: 256 samples @ 48 kHz = 5,333 µs (5.33 ms).
- **Audio Thread Allocation Audit**: `__wrap_malloc`, `__wrap_calloc`, `__wrap_realloc`, `__wrap_free` active during rendering.
- **Results**:
  - Live parameter sweeps (50 iterations per test): **0 heap allocations**.
  - Patch changes between Juno and FM: **0 heap allocations**.
  - Dynamic Chorus mode switching: **0 heap allocations**.
  - Delay time modulation (10 ms to 1000 ms): **0 heap allocations**.
  - Reverb size / damping tweaks: **0 heap allocations**.
  - EQ gain and tilt sweeps: **0 heap allocations**.

---

## 4. Factory Patch Neutrality (0.11.1 Baseline Bit-Exactness)

Comparing rendered PCM against baseline worktree `build/worktrees/smk-p4-sound-editor-completion-0111`:

| Factory Patch | Candidate SHA-256 | Baseline SHA-256 | Match |
|---|---|---|---|
| Patch 128 (DX7 Brass 1) | `4CF4385EEAEA79A8392827C506D8F4EA1AD1689AB295B1C89035DB34C10B18DC` | `4CF4385EEAEA79A8392827C506D8F4EA1AD1689AB295B1C89035DB34C10B18DC` | **BIT-EXACT** |
| Patch 135 (DX7 Piano 1) | `C5A23CD744C27D6DC930ACAD3E147D180981F788EB2914D30047328372A7C435` | `C5A23CD744C27D6DC930ACAD3E147D180981F788EB2914D30047328372A7C435` | **BIT-EXACT** |
| Patch 138 (DX7 E.Piano 1) | `35F932D2D1AF0F297CF57DD7FAB1805F32EB1128B1A74353206FA983A2080087` | `35F932D2D1AF0F297CF57DD7FAB1805F32EB1128B1A74353206FA983A2080087` | **BIT-EXACT** |
| Patch 142 (DX7 Bass 1) | `3749DE23EAB03EFE06CA9C85537F055D9BEEFD95E446C8ECF49D36B33EC6686C` | `3749DE23EAB03EFE06CA9C85537F055D9BEEFD95E446C8ECF49D36B33EC6686C` | **BIT-EXACT** |
| Patch 31 (Synth Bass I) | `9E0668AE8F5A5B4E21B089E000325B9FD4D0A9835D2E7F351B8A07F46A21A98B` | `9E0668AE8F5A5B4E21B089E000325B9FD4D0A9835D2E7F351B8A07F46A21A98B` | **BIT-EXACT** |
| Patch 4 (Moving Strings) | `A46ED6D5CA6C3D99158A4719E6BDDF7C06087AAEDD90276E96594E09421B0EBC` | `A46ED6D5CA6C3D99158A4719E6BDDF7C06087AAEDD90276E96594E09421B0EBC` | **BIT-EXACT** |
| Patch 8 (Juno Organ I) | `6D8947963CFF43A5F44409CE8F3D0AA72A23D015E9542FB391F2BF6006CC53F1` | `6D8947963CFF43A5F44409CE8F3D0AA72A23D015E9542FB391F2BF6006CC53F1` | **BIT-EXACT** |

Zero alteration of unedited factory patches confirmed.
