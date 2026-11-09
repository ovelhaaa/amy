# SMK-P4 0.12 Native AMY FX Suite — Gate C: Stereo Chorus

## 1. Executive Summary

Gate C implements and qualifies the native stereo chorus processor in AMY Bus 0 for the SMK-P4 synthesizer.

The implementation utilizes AMY's dedicated chorus delay lines and special modulator oscillator (`synth[64]`), preallocated at boot outside the real-time audio thread, providing lush stereo widening with zero heap allocation during live performance.

---

## 2. Chorus Architecture & Parameter Mappings

AMY native chorus uses an anti-phase modulated delay line topology:
- **Left Channel:** Modulated by $+delay\_mod$ (scale $= +1.0$)
- **Right Channel:** Modulated by $-delay\_mod$ (scale $= -1.0$)
- **Modulator:** Dedicated triangle LFO at `CHORUS_MOD_SOURCE + bus = 64`
- **Delay Buffer:** Preallocated stereo delay line in PSRAM

### Mode Preset Profiles

| Mode Index | Mode Name | Level / Mix | LFO Rate (Hz) | Max Delay (samples) | Depth Multiplier | Musical Character |
|:---|:---|:---:|:---:|:---:|:---:|:---|
| `0` | **OFF** | 0.00 | 0.00 | 320 | 0.00 | Clean bypass; bit-identical dry mono |
| `1` | **CLASSIC** | 0.50 | 0.50 | 320 | 1.00 | Standard gentle chorus |
| `2` | **JUNO I** | 0.50 | 0.40 | 320 | 0.80 | Slow, subtle Roland Juno Chorus I emulation |
| `3` | **JUNO II** | 0.50 | 0.85 | 320 | 1.30 | Faster, deeper Roland Juno Chorus II emulation |
| `4` | **ENSEMBLE** | 0.65 | 1.20 | 400 | 1.50 | Rich multi-voice ensemble emulation |
| `5` | **VIBRATO** | 0.60 | 2.50 | 200 | 0.70 | Fast rotary / pitch shimmer |

---

## 3. Stereo Qualification & Audio Measurements

Measurements recorded on rendered 48 kHz stereo blocks with mono source (MIDI C4, velocity 100):

### Mode Analysis Table

| Mode | RMS Mid | RMS Side | Side / Mid | Stereo Correlation ($r$) | Peak L / R | Observation |
|:---|:---:|:---:|:---:|:---:|:---:|:---|
| **OFF** | 365.6 | 0.0 | 0.0% | 1.0000 | 785 / 785 | Pure mono center; zero side energy |
| **CLASSIC** | 315.3 | 95.2 | 30.2% | 0.8449 | 770 / 792 | Clean widening, balanced mid |
| **JUNO I** | 157.0 | 42.4 | 27.0% | 0.8663 | 496 / 498 | Gentle slow movement |
| **JUNO II** | 356.9 | 83.4 | 23.4% | 0.8967 | 775 / 774 | Deep lush stereo spread |
| **ENSEMBLE** | 347.2 | 94.7 | 27.3% | 0.8631 | 742 / 815 | Wide stereophonic presence |
| **VIBRATO** | 217.8 | 127.9 | 58.7% | 0.5115 | 818 | 542 | Deepest phase decorrelation |

### Depth & Rate Response

- **Depth Response:**
  - Depth = 0.15: RMS Side = 91.2, Correlation = 0.8534
  - Depth = 0.95: RMS Side = 99.4, Correlation = 0.8931
  - Sample-by-sample difference between Min (0.15) and Max (0.95): **RMS Diff = 483.6** (`PASS`)
- **Rate Response:**
  - Rate Slow (Juno I, 0.40 Hz) vs Rate Fast (Vibrato, 2.50 Hz): **RMS Diff = 331.5** (`PASS`)
- **Mono Compatibility:**
  - Mid energy is maintained throughout all modes ($Mid > Side$), ensuring no catastrophic phase cancellation occurs when mono summed.
- **Clean Bypass:**
  - Switching back to Mode OFF returns immediately to $RMS_{Side} = 0.0$ and $r = 1.0000$.

---

## 4. Real-Time & Polyphony Safety

- **Zero Allocations:** Tested under 50 rapid live mode and depth adjustments while 8 voices were actively rendering. Verified `probe_bytes() == bytes_before` and `probe_calls() == calls_before` with `realtime = true` (`g_alloc_count == 0`).
- **8-Voice Polyphony + Sustain:** Tested with 8 simultaneous polyphonic voices, sustain pedal (`CC64`), and Chorus Juno II:
  - RMS Mid = 849.1, RMS Side = 244.0, Correlation = 0.8477.
  - Zero clipped samples (`clipped == 0`).
- **Factory Patch Switching:** Tested with live patch changes across Moving Strings (`Patch 4`), Brass Juno (`Patch 8`), and DX7 Piano (`Patch 128`):
  - Patch 4 (Moving Strings): RMS Mid = 83.6, RMS Side = 26.5, Correlation = 0.8217.
  - Patch 8 (Brass Juno): RMS Mid = 180.4, RMS Side = 74.8, Correlation = 0.7077.
  - Patch 128 (DX7 Piano): RMS Mid = 356.3, RMS Side = 134.9, Correlation = 0.7496.

---

## 5. Gate C Verification Status

| Check | Target | Observed | Status |
|:---|:---:|:---:|:---:|
| 5 Active Chorus Modes | $r < 0.95$, $S > 0$ | $r = 0.51 \dots 0.89$, $S = 42.4 \dots 127.9$ | `PASS` |
| Mode OFF Clean Bypass | $S = 0.0, r = 1.0000$ | $S = 0.0, r = 1.0000$ | `PASS` |
| Depth Min vs Max Delta | $RMS > 50$ | $RMS = 483.6$ | `PASS` |
| Rate Slow vs Fast Delta | $RMS > 50$ | $RMS = 331.5$ | `PASS` |
| Live Hard Real-Time | 0 allocs | 0 bytes, 0 calls | `PASS` |
| 8-Voice + Sustain Polyphony | 0 clips, $r < 0.90$ | 0 clips, $r = 0.8477$ | `PASS` |
| Factory Neutrality | 7/7 patches byte-identical | 7/7 SHA-256 match 0.11.1 | `PASS` |
