import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)

print("=== Scanning Juno patches 0..127 for noise and anomalies ===")
juno_issues = []
for pid_str, name, wire in patches:
    pid = int(pid_str)
    if pid < 128:
        # Check noise level osc: in Juno, v5 is typically noise (w5)
        # Check if v5 has non-zero amplitude
        v5_match = re.search(r'Zv5a([\d\.]+)', wire)
        if v5_match:
            v5_amp = float(v5_match.group(1))
            if v5_amp > 0.05:
                # Check if patch is meant to have noise (e.g. snare, hihat, wind, surf, sweep)
                is_noise_patch = any(w in name.upper() for w in ['NOISE', 'SNARE', 'SURF', 'WIND', 'OCEAN', 'SEASHORE', 'EXPLOSION', 'GUN', 'HELICOPTER', 'STORM', 'THUNDER', 'STEAM'])
                if not is_noise_patch:
                    juno_issues.append((pid, name.strip(), f"v5 (NOISE) amp = {v5_amp:.3f}"))

print(f"Juno patches with significant noise generator (v5): {len(juno_issues)}")
for pid, name, msg in juno_issues:
    print(f"  #{pid:3d} [{name:24s}]: {msg}")
