import re

with open('src/patches.h', 'r', encoding='utf-8') as f:
    content = f.read()

target_ids = [42, 62, 74, 78, 84, 86, 114, 116, 124, 158]

for tid in target_ids:
    s_tid = str(tid)
    for match in re.finditer(rf'/\*\s*{s_tid}:\s*([^/]+?)\s*\*/\s*"([^"]+)"', content):
        print(f"=== Patch {tid}: {match.group(1).strip()} ===")
        print(f"Wire: {match.group(2)}")
