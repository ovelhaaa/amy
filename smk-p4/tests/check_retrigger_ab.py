"""Pinned, deterministic Gate 0 acceptance; no analog quality claim."""
import pathlib
import re
import sys
import wave

root = pathlib.Path(sys.argv[1])
def read_log(name):
    data = (root / name).read_bytes()
    return data.decode("utf-16" if data.startswith(b"\xff\xfe") else "utf-8-sig")
def metrics(variant):
    text = read_log(f"{variant}_render_retrigger_probe.log")
    return {int(p): int(j) for p, j in re.findall(r"patch=(\d+).*?onset_boundary_jump=(\d+)", text)}
base, fixed = metrics("baseline"), metrics("integration")
assert set(base) == set(fixed) == {128, 135}
for patch in (128, 135):
    assert 0 < fixed[patch] < base[patch] * 0.5, (patch, base, fixed)
    def pcm(variant):
        with wave.open(str(root / f"{variant}_patch_{patch}.wav"), "rb") as w:
            assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (2, 2, 48000)
            return w.readframes(w.getnframes())
    a, b = pcm("baseline"), pcm("integration")
    assert len(a) == len(b) and a != b
    # First isolated note and release must remain bit-identical.
    assert a[:283*256*4] == b[:283*256*4], patch
    print(f"PASS patch={patch} onset={base[patch]} -> {fixed[patch]}; first attack/release identical")

def scenarios(variant):
    text = read_log(f"{variant}_test_retrigger_scenarios.log")
    rows = re.findall(r"Scenario ([A-H]):.*?OnsetJump:\s*(\d+).*?MaxDelta:\s*(\d+).*?Peak:\s*(\d+).*?RMS:\s*([\d.]+).*?Clip: (\d+)", text)
    assert len(rows) == 16, rows
    return rows
for a, b in zip(scenarios("baseline"), scenarios("integration")):
    assert a[0] == b[0] and int(b[5]) == 0 and float(b[4]) > 0
    assert int(b[2]) <= int(a[2]) * 1.2, (a, b)
    if a[0] in "ABH":
        assert a == b, (a, b)
    if a[0] in "CEF":
        assert int(b[1]) < int(a[1]), (a, b)
print("PASS A-H: sound, clipping, attack/release and retrigger regression bounds")
