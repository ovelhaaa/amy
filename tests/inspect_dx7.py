import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)
print(f"Total patches parsed: {len(patches)}")

suspicious = []
for pid, name, wire in patches:
    pnum = int(pid)
    if 128 <= pnum <= 255:
        # Check carrier / master / LFO
        # In DX7 wirecode:
        # v0: Master ALGO osc (w8)
        # v1: LFO osc
        # v2..v7: Operators 1..6 (v2=op1, v3=op2, v4=op3, v5=op4, v6=op5, v7=op6)
        
        # Check if v0 has constant amp without gate
        # Check if any operator has envelope that does not decay
        # Check feedback
        fb_match = re.search(r'b([\d\.]+)', wire)
        fb = float(fb_match.group(1)) if fb_match else 0.0
        
        # Check LFO
        lfo_match = re.search(r'Zv1w(\d+)[^Z]*f([\d\.]+)', wire)
        lfo_freq = float(lfo_match.group(2)) if lfo_match else 0.0
        
        # Check operators release times
        op_releases = []
        for op in range(2, 8):
            op_match = re.search(rf'v{op}[^Z]*A([^Z]+)', wire)
            if op_match:
                env_str = op_match.group(1)
                parts = env_str.split(',')
                if len(parts) >= 2:
                    try:
                        rel_time = float(parts[-2])
                        rel_val = float(parts[-1].replace('L1', '').replace('L0', ''))
                        op_releases.append((op, rel_time, rel_val))
                    except:
                        pass
        
        # Look for extreme release times (> 10000ms) or high feedback (> 0.15)
        extreme_rels = [r for r in op_releases if r[1] > 15000]
        if extreme_rels or fb >= 0.15:
            suspicious.append((pnum, name.strip(), fb, lfo_freq, extreme_rels))

print(f"\nDX7 Patches with potential background noise / continuous resonance ({len(suspicious)}):")
for pnum, name, fb, lfo, ex_rels in suspicious:
    print(f"  #{pnum:3d} [{name:16s}] FB={fb:.2f}, LFO={lfo:5.1f}Hz, LongReleases={ex_rels}")
