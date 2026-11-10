# SMK-P4 0.13: FM Audio Benchmarks and DSP Performance Report

**Target:** Guition ESP32-P4 Dual Core RISC-V @ 400 MHz  
**Sample Rate:** 48,000 Hz  
**Block Size:** 256 frames (5.333 ms per block)  
**Polyphony:** 8 musical voices, 6 FM operators per voice (48 oscillators)  
**Test Harness:** `smk-p4/tests/fm_qualification_bench.cpp`

---

## 1. Real-Time Execution Deadlines

In the SMK-P4 audio engine, audio is rendered in discrete blocks of 256 frames:

$$\text{Block Period} = \frac{256\text{ frames}}{48,000\text{ frames/s}} = 5.333\text{ ms} = 5,333\text{ µs}$$

To guarantee zero audio underruns or buffer starvation, the total audio task execution (including AMY rendering, voice management, chorus, delay, reverb, and EQ) must remain strictly below 5,333 µs per block.

---

## 2. Benchmark Summary Across Topologies

Measurements were taken during sustained 8-voice polyphonic playback (48 active oscillators) across all 32 FM topologies:

| Metric | Measured Value | Real-Time Limit | Headroom Margin |
|:-------|:--------------:|:---------------:|:---------------:|
| **Minimum Average Render Time** | **121.0 µs** (Algo 31) | 5,333 µs | **97.7% Headroom** |
| **Maximum Average Render Time** | **196.8 µs** (Algo 1) | 5,333 µs | **96.3% Headroom** |
| **Worst-Case Peak Render Time** | **400.1 µs** (Algo 3) | 5,333 µs | **92.5% Headroom** |
| **Average CPU Load (8 Voices FM)** | **~2.8% – 3.7%** | 100.0% | **> 96% Available CPU** |
| **Audio Underruns** | **0** | 0 | Deterministic |
| **Real-time Allocations (malloc/free)** | **0** | 0 | Strict Zero Policy |

---

## 3. Structural Comparison: Additive vs. Serial Cascades

### 3.1 Long Serial Cascades (Algorithms 16, 17, 18)
- **Topology:** Single carrier with 5-operator linear modulation chain:
  `OP6 -> OP5 -> OP4 -> OP3 -> OP2 -> OP1 -> OUT`
- **Render Profile:**
  - Average block render time: **~184.4 µs**
  - Peak amplitude: **1,274 q15**
  - RMS amplitude: **329 q15**
- **Characteristics:**
  - Requires repeated read-modify-write cycles to internal phase buses (`BUS_ONE`, `BUS_TWO`).
  - Rich harmonic development even at low overall amplitude due to high modulation index.

### 3.2 Pure Additive (Algorithm 32)
- **Topology:** 6 independent parallel sine carriers:
  `OP1 + OP2 + OP3 + OP4 + OP5 + OP6 -> OUT`
- **Render Profile:**
  - Average block render time: **~121.8 µs**
  - Peak amplitude: **6,097 q15**
  - RMS amplitude: **1,011 q15**
- **Characteristics:**
  - Bypasses intermediate phase modulation buses; all 6 operators sum directly into the audio buffer.
  - Faster rendering (~34% lower DSP overhead than deep cascade).
  - Higher peak amplitude due to coherent in-phase acoustic summation, perfectly within 16-bit signed range (clipping threshold = 32,767).

### 3.3 Multi-Carrier Hybrid Branches (Algorithms 5, 6, 22, 23)
- **Topology:** 3 or 4 carriers with independent 2-op modulation pairs.
- **Render Profile:**
  - Average block render time: **~140 – 165 µs**
  - Peak amplitude: **3,378 – 4,524 q15**
  - RMS amplitude: **774 – 1,131 q15**
- **Characteristics:**
  - Excellent balance of rich timbre and clear polyphonic definition.

---

## 4. Hard Real-Time Memory Verification

To ensure audio stability on embedded hardware without garbage collection or heap fragmentation:
- The `probe_realtime(true)` harness wrapped all `r.render()` calls.
- GNU linker memory wraps (`-Wl,--wrap=malloc`, `calloc`, `realloc`, `free`) intercepted all heap operations.
- **Result:** Exactly zero heap allocations or frees were performed across all 64 blocks of all 32 algorithms.

---

## 5. Conclusion

The native AMY FM algorithm rendering on the SMK-P4 architecture exhibits:
1. Negligible CPU utilization (< 4% of real-time block budget).
2. Huge timing margin (> 92% worst-case headroom).
3. Zero clipping or dynamic range overflow.
4. Total immunity to memory allocation latency.
