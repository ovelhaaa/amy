import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)

print("=== Scanning Juno patches for unstable filter cutoffs (F > 20000 or R > 10) ===")
for pid_str, name, wire in patches:
    pid = int(pid_str)
    if pid < 128:
        # Match F<freq>,<res>
        f_match = re.search(r'Zv0F([\d\.]+),([\d\.]+)', wire)
        if f_match:
            freq = float(f_match.group(1))
            res = float(f_match.group(2))
            if freq > 20000.0 or res > 10.0:
                print(f"Patch #{pid:3d} [{name.strip():24s}]: Filter Cutoff = {freq:.1f} Hz, Res = {res:.2f}")
