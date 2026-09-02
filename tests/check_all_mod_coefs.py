import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

patches = re.findall(r'/\*\s*(\d+):\s*([^/]+?)\s*\*/\s*"([^"]+)"', content)

print("=== Checking all 256 patches for COEF_MOD on FM operators ===")
for pid, name, wire in patches:
    pnum = int(pid)
    # Check if any operator v2..v7 has amp_coefs with non-zero 6th element (COEF_MOD)
    for op in range(2, 8):
        m = re.search(rf'v{op}[^Z]*a([\d\.,\-]+)', wire)
        if m:
            coefs = m.group(1).split(',')
            if len(coefs) >= 6:
                try:
                    mod_coef = float(coefs[5])
                    if mod_coef > 0:
                        print(f"Patch #{pnum:3d} [{name.strip()}]: op {op} has COEF_MOD={mod_coef} (Wire: {m.group(0)})")
                except:
                    pass
