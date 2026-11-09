# SMK-P4 0.12 Native AMY FX Suite — Gate F: 3-Band EQ & Tilt Tone

## 1. Executive Summary

Gate F implements and qualifies the native 3-band parametric EQ and Tilt Tone macro on AMY Bus 0 for the SMK-P4 synthesizer.

The implementation utilizes AMY's per-bus 3-stage biquad cascade (`parametric_eq_process`), which incorporates:
- **Low Stage:** Biquad Low-Pass Filter at $800.0$ Hz (`EQ_CENTER_LOW`, $Q = 0.707$).
- **Mid Stage:** Biquad Band-Pass Filter at $2500.0$ Hz (`EQ_CENTER_MED`, $Q = 1.000$).
- **High Stage:** Biquad High-Pass Filter at $7000.0$ Hz (`EQ_CENTER_HIGH`, $Q = 0.707$).

When all bands are set to $0$ dB ($1.0$ linear gain), the entire EQ routine is automatically bypassed in `src/amy.c`, ensuring bit-exact dry neutrality and zero CPU overhead.

---

## 2. Parameter Mapping & Tilt Tone Curve

### 2.1 Master Tone Macro (Tilt EQ)

`fx.master_tone` provides immediate spectral balance control (from $-1.0$ / Dark to $+1.0$ / Bright):
- $\text{Tone} > 0$ (Bright):
  - $\text{gain}_{high} = 1.0 + 0.60 \times \text{Tone}$ (up to $+4.1$ dB at $+1.0$)
  - $\text{gain}_{low} = 1.0 - 0.40 \times \text{Tone}$ (down to $-4.4$ dB at $+1.0$)
- $\text{Tone} < 0$ (Dark):
  - $\text{gain}_{low} = 1.0 - 0.60 \times \text{Tone}$ (up to $+4.1$ dB at $-1.0$)
  - $\text{gain}_{high} = 1.0 + 0.40 \times \text{Tone}$ (down to $-4.4$ dB at $-1.0$)
- $\text{Tone} = 0.0$: Both gains $= 1.00$ ($0$ dB flat).

### 2.2 Discrete 3-Band Gains

- `eq_low_db`: $-12.0 \dots +12.0$ dB $\to$ scales linear factor $10^{\text{gain} / 20}$ on Band 0 ($800$ Hz).
- `eq_mid_db`: $-12.0 \dots +12.0$ dB $\to$ scales linear factor $10^{\text{gain} / 20}$ on Band 1 ($2500$ Hz).
- `eq_high_db`: $-12.0 \dots +12.0$ dB $\to$ scales linear factor $10^{\text{gain} / 20}$ on Band 2 ($7000$ Hz).

---

## 3. Acoustic Qualification & Audio Measurements

Measurements recorded on rendered 48 kHz stereo blocks:

### 3.1 Bit-Exact Neutrality

- With Tone $= 0.0$ and Low/Mid/High $= 0.0$ dB:
- Rendered C4 note RMS = **365.62** (bit-identical to baseline without EQ).

### 3.2 Master Tone Tilt Response

- **Low Note (MIDI C2, ~65 Hz):**
  - Dark ($\text{Tone} = -1.0$): RMS = **602.86**
  - Bright ($\text{Tone} = +1.0$): RMS = **225.37**
  - Ratio: Dark produces **2.67×** more low-frequency energy than Bright (`PASS`).
- **High Note (MIDI C6, ~1046 Hz):**
  - Dark ($\text{Tone} = -1.0$): RMS = **244.93**
  - Bright ($\text{Tone} = +1.0$): RMS = **278.43**
  - Ratio: Bright produces **1.14×** more high-frequency energy than Dark (`PASS`).

### 3.3 Discrete 3-Band Gains (+6 dB Boost and -6 dB Cut)

| Band | Test Note | +6 dB RMS | -6 dB RMS | Ratio | Target Ratio | Status |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **Low Band** (800 Hz LPF) | C2 (65 Hz) | 753.31 | 188.37 | **4.00×** | $4.00×$ (+12 dB) | `PASS` |
| **Mid Band** (2500 Hz BPF) | C6 (1046 Hz) | 506.66 | 142.18 | **3.56×** | $> 2.0×$ | `PASS` |
| **High Band** (7000 Hz HPF) | C6 (1046 Hz) | 281.81 | 243.59 | **1.16×** | $> 1.12×$ | `PASS` |

---

## 4. Real-Time & Polyphony Safety

- **Zero Allocations:** Tested under 50 rapid live parameter sweeps of `master_tone`, `eq_low_db`, `eq_mid_db`, and `eq_high_db` during active audio rendering. Verified `probe_bytes() == bytes_before` and `probe_calls() == calls_before` (`g_alloc_count == 0`).
- **8-Voice Polyphony Headroom:** Tested with 8 voices playing with $+6$ dB EQ boost:
  - RMS = $749.30$
  - Peak sample = $3813$ (safe headroom $> 18$ dB below $32767$)
  - Clipped count = $0$ (`PASS`)

---

## 5. Gate F Verification Status

| Check | Target | Observed | Status |
|:---|:---:|:---:|:---:|
| Bit-Exact Neutrality | Bit-identical dry | Clean bypass confirmed | `PASS` |
| Tilt Tone Macro | Dark $>$ Bright on low; Bright $>$ Dark on high | Confirmed on C2 and C6 | `PASS` |
| Discrete Boost & Cut | $+6$ dB $> -6$ dB across all 3 bands | Confirmed across Low, Mid, High | `PASS` |
| Live Hard Real-Time | 0 allocs | 0 bytes, 0 calls | `PASS` |
| 8-Voice Boost Headroom | 0 clips, Peak $< 32767$ | 0 clips, Peak $= 3813$ | `PASS` |
| Factory Neutrality | 7/7 patches byte-identical | 7/7 SHA-256 match 0.11.1 | `PASS` |
