import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)

print("=== Scanning all 256 patches for LFO and Modulator anomalies ===")
for pid, name, wire in patches:
    pnum = int(pid)
    # Check LFO wave type
    lfo_match = re.search(r'Zv1w(\d+)', wire)
    if lfo_match:
        lfo_wave = int(lfo_match.group(1))
        # wave 5 = NOISE, wave 1 = PULSE/SQUARE, wave 2/3 = SAW
        if lfo_wave == 5: # NOISE
            print(f"Patch #{pnum:3d} [{name.strip()}]: LFO is NOISE (w5) -> WIRE: {wire[:100]}...")
        elif lfo_wave != 0 and lfo_wave != 4: # Not SINE (0) or TRIANGLE (4)
            print(f"Patch #{pnum:3d} [{name.strip()}]: LFO is wave {lfo_wave} -> WIRE: {wire[:80]}...")
