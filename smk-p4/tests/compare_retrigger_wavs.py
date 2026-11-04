import wave
import numpy as np

def load_wav(path):
    with wave.open(path, 'rb') as w:
        frames = w.readframes(w.getnframes())
        return np.frombuffer(frames, dtype=np.int16).reshape(-1, 2)

def analyze_clicks(name, data, sr=48000):
    left = data[:, 0].astype(np.float64)
    # First derivative (sample to sample difference)
    diffs = np.abs(np.diff(left))
    # Second derivative (curvature / impulse)
    diff2 = np.abs(np.diff(diffs))
    
    # High frequency transient energy: high-pass filter diff
    # Simple Haar wavelet / 1-sample difference energy
    rms = np.sqrt(np.mean(left**2))
    max_d1 = np.max(diffs)
    max_d2 = np.max(diff2)
    # Top 0.01% highest jumps (outliers)
    p99_99 = np.percentile(diffs, 99.99)
    p99_9 = np.percentile(diffs, 99.9)

    print(f"[{name}]")
    print(f"  RMS: {rms:.1f}, Peak: {np.max(np.abs(left)):.0f}")
    print(f"  Max D1 (sample delta): {max_d1:.0f}")
    print(f"  Max D2 (impulse jerk): {max_d2:.0f}")
    print(f"  99.99th percentile delta: {p99_99:.1f}")
    print(f"  99.9th percentile delta: {p99_9:.1f}")

    # Count abrupt jumps > 300
    jumps_300 = np.sum(diffs > 300)
    jumps_500 = np.sum(diffs > 500)
    print(f"  Jumps > 300: {jumps_300}, Jumps > 500: {jumps_500}")

if __name__ == "__main__":
    import sys
    base_135 = load_wav("build/p404/retrigger_probe/baseline_patch_135.wav")
    analyze_clicks("Baseline Patch 135", base_135)
    base_128 = load_wav("build/p404/retrigger_probe/baseline_patch_128.wav")
    analyze_clicks("Baseline Patch 128", base_128)
