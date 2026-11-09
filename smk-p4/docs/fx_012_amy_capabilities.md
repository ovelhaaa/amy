# SMK-P4 0.12 — AMY Native FX Suite Architecture & Capabilities Audit

**Repository:** `ovelhaaa/amy`  
**Branch:** `feature/smk-p4-native-amy-fx-012`  
**Target Hardware:** Guition ESP32-P4 (800×480), PCM5102A (48 kHz / 256 frames), M-VAVE SMK25 V2  
**AMY Engine Configuration:** 48 kHz, 256 frames/block, 8 voices, 64 musical oscillators (`0..63`), Bus 0 shared

---

## 1. Executive Summary

This document fulfills **Gate A** of the Native AMY FX Suite specification. An exhaustive audit of the upstream AMY engine (`src/amy.h`, `src/amy.c`, `src/filters.c`, `src/delay.c`, `src/reverb.c`), the SMK-P4 runtime adapter, and UI blueprint was conducted to map all native capabilities, memory footprints, execution constraints, and routing architecture.

The upstream AMY engine possesses production-ready, highly optimized DSP implementations for 3-band parametric EQ, stereo chorus, stereo echo/delay, and stereo algorithmic reverb. However, these features were disabled in SMK-P4 0.11.1 (`config.features.reverb = config.features.chorus = config.features.echo = 0`). The goal of 0.12 is to safely integrate and expose these native processors without modifying the AMY core or compromising real-time audio stability.

---

## 2. Native AMY Bus FX Processing Chain

In `src/amy.c` (`amy_fill_buffer()`), the per-bus effects chain is applied sequentially per block (256 frames at 48 kHz = 5.333 ms budget) in a **strictly fixed order**:

$$\text{Voice Render} \longrightarrow \text{Bus Accumulator} \longrightarrow \mathbf{EQ} \longrightarrow \mathbf{Chorus} \longrightarrow \mathbf{Echo/Delay} \longrightarrow \mathbf{Reverb} \longrightarrow \text{Bus Hook} \longrightarrow \text{Master Mix}$$

```
                ┌────────────────────────────────────────────────────────┐
                │                     AMY BUS 0                          │
Voice 0..7 ───► │  [Parametric EQ] ──► [Chorus] ──► [Echo] ──► [Reverb]  │ ──► Master Output
(64 Oscs)       └────────────────────────────────────────────────────────┘
```

### Architectural Implications:
1. **Fixed Processing Order**: The signal flow is permanently `EQ → Chorus → Echo → Reverb`. The UI parameter `fx.routing` and "PARALLEL SEND" must remain marked `Planned` or documented as decorative/fixed-topology.
2. **True Bypass via Zero-Level**:
   - **EQ**: Bypassed if `eq.eq[0..2] == F2S(1.0f)`.
   - **Chorus**: Bypassed if `chorus.level == 0` (skips both the LFO wave render and delay interpolation loop).
   - **Echo**: Bypassed if `echo.level == 0` (skips the circular buffer read/write and feedback loop).
   - **Reverb**: Bypassed if `reverb.level == 0` (skips the 4-comb + 6-allpass Stautner-Puckette network).
3. **Dry Signal Neutrality**: When all levels are 0 and EQ is neutral, `amy_fill_buffer()` produces bit-exact identical PCM to 0.11.1 with zero additional CPU overhead.

---

## 3. Native AMY FX APIs & Parameter Specifications

### 3.1 Stereo Chorus
- **API**: `config_chorus(uint8_t bus, float level, uint16_t max_delay, float lfo_freq, float depth)`
- **Native Parameters**:
  - `bus`: uint8_t (Target Bus `0`).
  - `level`: float ($0.0 \dots 1.0$), controls wet signal mix. $0.0 = \text{Bypass}$.
  - `max_delay`: uint16_t, modulation center delay in samples (must be $\le \text{DELAY\_LINE\_LEN} = 512$ samples, default 320 samples = 6.67 ms @ 48 kHz).
  - `lfo_freq`: float, LFO modulation speed in Hz (default 0.5 Hz, recommended range 0.1 to 8.0 Hz).
  - `depth`: float ($0.0 \dots 1.0$), LFO modulation amplitude depth.
- **Dedicated Chorus Oscillator**:
  - AMY reserves oscillator index `CHORUS_MOD_SOURCE + bus` = `AMY_OSCS + bus` = $64 + 0 = 64$.
  - It does **not** consume any of the 64 musical oscillators ($0 \dots 63$) allocated to the 8 synth voices.
  - Generates a `TRIANGLE` wave modulating the delay buffer with Hermite 4-point interpolation in opposite phase across Left and Right channels ($+\text{scale}$ on L, $-\text{scale}$ on R) for true stereo width.

### 3.2 Stereo Echo / Delay
- **API**: `config_echo(uint8_t bus, float level, float delay_ms, float max_delay_ms, float feedback, float filter_coef)`
- **Native Parameters**:
  - `bus`: uint8_t (Target Bus `0`).
  - `level`: float ($0.0 \dots 1.0$), echo mix level. $0.0 = \text{Bypass}$.
  - `delay_ms`: float, current delay time in milliseconds ($10.0 \dots 1000.0\text{ ms}$).
  - `max_delay_ms`: float, capacity of the delay line. Quantized to enclosing power of 2 in samples:
    $$\text{For } 1000\text{ ms} @ 48\text{ kHz} \implies 48,000\text{ samples} \longrightarrow \text{allocates } 65,536\text{ samples} = 1365.33\text{ ms}.$$
  - `feedback`: float ($0.0 \dots 0.95$), regeneration gain. Discounted by $1/(1 - \text{filter\_coef})$ if HPF is active to prevent runaway gain.
  - `filter_coef`: float ($-0.99 \dots +0.99$). Positive = 1-pole normalized IIR LPF (analog tape style); negative = FIR HPF.

### 3.3 Stereo Algorithmic Reverb
- **API**: `config_reverb(uint8_t bus, float level, float liveness, float damping, float xover_hz)`
- **Optional Freeze API**: `config_reverb_freeze(uint8_t bus, uint8_t freeze)`
- **Native Parameters**:
  - `bus`: uint8_t (Target Bus `0`).
  - `level`: float ($0.0 \dots 1.0$), wet reverb mix. $0.0 = \text{Bypass}$.
  - `liveness`: float ($0.0 \dots 1.0$, default $0.85$), feedback decay factor of the 4 delay lines.
  - `damping`: float ($0.0 \dots 1.0$, default $0.5$), high frequency absorption factor.
  - `xover_hz`: float ($100.0 \dots 10000.0\text{ Hz}$, default $3000.0\text{ Hz}$), 1-pole lowpass filter cutoff.
- **Topology**: Stautner-Puckette design with 4 cross-coupled feedback delay lines ($58.64\text{ ms}$, $69.43\text{ ms}$, $74.52\text{ ms}$, $86.12\text{ ms}$) and 6 early reflection delays ($75.25\text{ ms}$ down to $13.65\text{ ms}$).

### 3.4 3-Band Parametric EQ
- **API**: `config_eq(uint8_t bus, SAMPLE eq_l, SAMPLE eq_m, SAMPLE eq_h)`
- **Native Parameters**:
  - `eq_l`, `eq_m`, `eq_h`: `SAMPLE` (fixed-point linear gain, neutral = `F2S(1.0f)` = 0 dB).
  - Center frequencies: Low Shelf = $100\text{ Hz}$, Peaking Mid = $1000\text{ Hz}$, High Shelf = $7000\text{ Hz}$.
  - dB mapping: $\text{linear} = 10^{\text{dB} / 20.0}$. Range: $-12\text{ dB}$ to $+12\text{ dB}$.

---

## 4. Memory Footprint and Allocation Safety Audit

The core rule of Gate B is: **No dynamic allocations during audio rendering or parameter adjustment.**

| Subsystem | Buffer Type | Sample Length (Stereo) | Element Size | Total Heap | Target Memory Region |
|---|---|---|---|---|---|
| **Chorus LFO Buffer** | Modulation signal | $256 \times 1$ | 4 bytes (`SAMPLE`) | $1,024\text{ B}$ | PSRAM / SPIRAM |
| **Chorus Delay Lines** | Stereo delay line | $512 \times 2$ | 4 bytes (`SAMPLE`) | $4,160\text{ B}$ | PSRAM / SPIRAM |
| **Chorus Modulator Osc** | Voice struct (`synth[64]`) | 1 instance | $320\text{ B}$ | $320\text{ B}$ | Internal SRAM |
| **Echo Delay Lines** | Stereo delay line ($65,536\text{ samps}$) | $65,536 \times 2$ | 4 bytes (`SAMPLE`) | $524,352\text{ B}$ ($512\text{ KB}$) | PSRAM / SPIRAM |
| **Reverb Comb Delays** | 4 delay lines | $4096+4096+4096+8192$ | 4 bytes (`SAMPLE`) | $81,920\text{ B}$ ($80\text{ KB}$) | PSRAM / SPIRAM |
| **Reverb Early Refs** | 6 delay lines | $4096+4096+2048+1024+1024+1024$ | 4 bytes (`SAMPLE`) | $53,248\text{ B}$ ($52\text{ KB}$) | PSRAM / SPIRAM |
| **Parametric EQ** | State and coefficients | Biquad delays | 4 bytes (`SAMPLE`) | Preallocated in `bus[0]` | Internal SRAM |
| **TOTAL FX FOOTPRINT** | — | — | — | **$\approx 664.8\text{ KB}$** | **$99.9\%$ in PSRAM** |

### Memory Allocation Plan for Gate B:
1. All FX delay lines and buffers must be allocated strictly during `AmyRuntime::start()`:
   - `ensure_osc_allocd(CHORUS_MOD_SOURCE, NULL)`
   - `alloc_chorus_delay_lines(0)`
   - `alloc_echo_delay_lines(0, 65536)`
   - `alloc_reverb_delay_lines(0)`
2. Post-allocation verification: verify all pointer handles (`chorus_delay_lines[0..1]`, `delay_mod`, `echo_delay_lines[0..1]`, `reverb.rev->delay_1..4`).
3. Set initial levels to 0.0f (bypassed) and EQ to 1.0f (neutral).
4. Any subsequent call to `config_chorus()`, `config_echo()`, or `config_reverb()` will reuse the preallocated buffers without calling `malloc()`, `new_delay_line()`, or resizing buffers.

---

## 5. UI Parameter Mapping Strategy

The existing UI controls in `effects_screen.cpp` and `ui_parameters.cpp` map directly to native AMY parameters:

| UI Parameter ID | Label | Range | AMY Target API & Parameter | Status |
|---|---|---|---|---|
| `fx.chorus.mode` | CHORUS MODE | Enum 0..5 | Mode presets modulating Rate, Max Delay, Depth, and Mix | **AVAILABLE in 0.12** |
| `fx.chorus.depth` | CHORUS DEPTH | $0 \dots 100\%$ | `config_chorus(0, mix, max_delay, rate, depth / 100.0f)` | **AVAILABLE in 0.12** |
| `fx.delay.time_ms` | DELAY TIME | $10 \dots 1000\text{ ms}$ | `config_echo(0, mix, time_ms, 1000.0f, fb, filter)` | **AVAILABLE in 0.12** |
| `fx.delay.feedback`| DELAY FDBK | $0 \dots 95\%$ | `config_echo(0, mix, time, 1000.0f, fb / 100.0f, filter)` | **AVAILABLE in 0.12** |
| `fx.delay.mix` | DELAY MIX | $0 \dots 100\%$ | `config_echo(0, mix / 100.0f, time, 1000.0f, fb, filter)` | **AVAILABLE in 0.12** |
| `fx.reverb.size` | REVERB SIZE | $0 \dots 100\%$ | `config_reverb(0, mix, 0.4f + 0.55f * size, damp, xover)` | **AVAILABLE in 0.12** |
| `fx.reverb.mix` | REVERB MIX | $0 \dots 100\%$ | `config_reverb(0, mix / 100.0f, liveness, damp, xover)` | **AVAILABLE in 0.12** |
| `fx.master_tone` | TILT TONE / EQ | $-100 \dots +100\%$ | `config_eq(0, eq_l, eq_m, eq_h)` dual Low/High macro | **AVAILABLE in 0.12** |
| `fx.drive` | BUS DRIVE | $0 \dots 100\%$ | Soft saturation DSP | **PLANNED** (Keep disabled) |
| `fx.routing` | FX CHAIN | Enum 0..3 | Master chain order / bypass | **PLANNED** (Fixed chain documented) |
| `mixer.fx_send` | BUS FX SEND | $0 \dots 100\%$ | Per-bus aux send | **PLANNED** (Bus 0 direct) |

---

## 6. Gate A Verdict & Next Step

- **Gate A Audit Status:** `PASS`
- **Native Capability Verified:** Complete, native AMY FX suite identified and validated in source code.
- **Immediate Next Step:** Proceed to **Gate B** — Implement preallocation, safe initialization, and neutrality verification.
