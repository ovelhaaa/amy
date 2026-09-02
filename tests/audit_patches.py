import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)
print(f"Total patches loaded: {len(patches)}")

report = []

for pid_str, name, wire in patches:
    pid = int(pid_str)
    name = name.strip()
    issues = []

    # 1. Check LFO wave type
    lfo_match = re.search(r'Zv1w(\d+)[^Z]*', wire)
    lfo_wave = int(lfo_match.group(1)) if lfo_match else None
    lfo_freq_match = re.search(r'Zv1[^Z]*f([\d\.]+)', wire)
    lfo_freq = float(lfo_freq_match.group(1)) if lfo_freq_match else 0.0

    # 2. Check if LFO is NOISE (w5) on musical patches
    if lfo_wave == 5:
        # Check if patch is SFX
        is_sfx = any(s in name.upper() for s in ['TRAIN', 'WHISL', 'TAKE OFF', 'LASER', 'GUN', 'EXPLOSION', 'ST.HELENS', 'DESCENT', 'WASP'])
        if not is_sfx:
            issues.append(f"CRITICAL: LFO is NOISE (w5) on musical patch!")
        else:
            issues.append(f"INFO: LFO is NOISE (w5) (SFX patch)")

    # 3. Check extreme LFO frequencies (> 25 Hz)
    if lfo_freq > 25.0:
        is_sfx = any(s in name.upper() for s in ['TRAIN', 'WHISL', 'TAKE OFF', 'LASER', 'GUN', 'EXPLOSION', 'ST.HELENS', 'DESCENT', 'GRAND PRIX'])
        if not is_sfx:
            issues.append(f"WARNING: LFO freq is {lfo_freq:.1f} Hz (audio-rate ring mod on musical patch)")

    # 4. Check FM operators COEF_MOD routing
    for op in range(2, 8):
        m = re.search(rf'v{op}[^Z]*a([\d\.,\-]+)', wire)
        if m:
            coefs = m.group(1).split(',')
            if len(coefs) >= 6:
                try:
                    mod_coef = float(coefs[5])
                    if mod_coef >= 0.5:
                        issues.append(f"WARNING: op {op} has high COEF_MOD = {mod_coef}")
                except:
                    pass

    # 5. Check Algorithm index
    algo_match = re.search(r'o(\d+)', wire)
    if algo_match:
        algo_idx = int(algo_match.group(1))
        if algo_idx > 32:
            issues.append(f"ERROR: Invalid FM Algorithm index {algo_idx}")

    # 6. Check Feedback value
    fb_match = re.search(r'b([\d\.]+)', wire)
    if fb_match:
        fb_val = float(fb_match.group(1))
        if fb_val > 0.5:
            issues.append(f"WARNING: Extreme Feedback {fb_val}")

    if issues:
        report.append((pid, name, issues, wire))

print(f"\n=== Found {len(report)} patches with notes/warnings ===")
for pid, name, issues, wire in report:
    print(f"\nPatch #{pid:3d} [{name}]:")
    for iss in issues:
        print(f"   -> {iss}")
