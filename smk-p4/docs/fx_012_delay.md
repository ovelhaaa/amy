# SMK-P4 0.12 Native AMY FX Suite — Gate E: Echo / Delay

## 1. Executive Summary

Gate E implements and qualifies the native echo/delay processor in AMY Bus 0 for the SMK-P4 synthesizer.

The implementation utilizes AMY's fixed delay lines (`apply_fixed_delay`), preallocated at startup with a maximum power-of-two capacity of $65,536$ samples ($1365.33$ ms at 48 kHz, 512 KB stereo in PSRAM). Changing delay time between $10$ ms and $1000$ ms at runtime adjusts the read tap pointer (`fixed_delay`) within the preallocated line, incurring zero heap allocations.

---

## 2. Echo Architecture & Parameter Mappings

- **Capacity:** Fixed preallocation of $65,536$ samples per channel ($512$ KB in PSRAM).
- **Control Interface:**
  - `fx.delay.time_ms`: $10.0 \dots 1000.0$ ms.
  - `fx.delay.feedback`: $0.0 \dots 0.95$ (hard-clamped at $95\%$ to guarantee feedback loop stability).
  - `fx.delay.mix`: $0.0 \dots 1.0$ ($0\% \dots 100\%$ wet).
  - `echo_filter_coef`: Native 1-pole filter in delay loop; configured for clean feedback.

---

## 3. Acoustic Qualification & Audio Measurements

Measurements recorded on rendered 48 kHz stereo blocks:

### 3.1 Echo Repetition Timing

- Configured delay time: $106.67$ ms ($5120$ samples = $20$ blocks).
- Input: 2-block staccato pulse (C4, velocity 100).
- **Gap RMS (blocks 4..18):** $18.53$ (fading note release).
- **Echo 1 Arrival (blocks 19..23):** $172.42$ (`PASS`).
- **Ratio:** The first echo repetition produces a sharp, clear arrival **9.3×** stronger than the inter-echo gap.

### 3.2 Feedback Decay & Runaway Prevention

- Tested under extreme maximum feedback: $95\%$ ($0.95$).
- **Peak Sample:** $482$ (far below 16-bit ceiling $32767$).
- **Clipped Samples:** $0$ (`PASS`).
- **Observation:** The feedback matrix smoothly decays without numerical divergence, runaway oscillations, or buffer overflow.

### 3.3 Multi-FX Bus Chain Interaction (Chorus + Delay + Reverb)

Tested with all three spatial processors active simultaneously:
- **Chorus:** Juno I (Rate 0.40 Hz, Depth 0.50).
- **Delay:** 250 ms, Feedback 0.40, Mix 0.30.
- **Reverb:** Size 0.70, Mix 0.30.
- **Polyphony:** 8 voices playing chords with sustain pedal (`CC64`).
- **Results:**
  - RMS = $858.72$
  - Peak sample = $3581$
  - Clipped count = $0$
  - **Verdict:** The fixed chain (`EQ → Chorus → Echo → Reverb`) processes complex polyphonic material with high clarity, wide stereo spread, and zero distortion or pointer collisions.

---

## 4. Real-Time & Polyphony Safety

- **Zero Allocations:** Tested under 50 live modifications across the full delay time range ($10 \dots 1000$ ms) and feedback range ($0 \dots 95\%$) during active audio rendering. Verified `probe_bytes() == bytes_before` and `probe_calls() == calls_before` (`g_alloc_count == 0`).
- **Zero Buffer Overflows:** $1000$ ms ($48,000$ samples) is safely contained within the $65,536$-sample buffer with $17,536$ samples ($365$ ms) of headroom.

---

## 5. Gate E Verification Status

| Check | Target | Observed | Status |
|:---|:---:|:---:|:---:|
| Discrete Echo Repetition | Echo 1 $> 2\times$ Gap | Echo 1 $= 9.3\times$ Gap | `PASS` |
| Feedback 0.95 Stability | 0 clips, Peak $< 32767$ | 0 clips, Peak $= 482$ | `PASS` |
| Live Hard Real-Time | 0 allocs | 0 bytes, 0 calls | `PASS` |
| Combined Multi-FX (Chorus+Delay+Reverb) | 0 clips, 8 voices stable | 0 clips, Peak $= 3581$ | `PASS` |
| Factory Neutrality | 7/7 patches byte-identical | 7/7 SHA-256 match 0.11.1 | `PASS` |
