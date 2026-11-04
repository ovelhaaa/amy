"""Summarize deterministic host evidence; never approves physical P4 timing."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import wave

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--voices", nargs="+", type=int, choices=(4, 6, 8), default=[4, 6, 8])
parser.add_argument("--log-suffix", default="tests")
parser.add_argument("--probe-suffix")
parser.add_argument("--output", type=Path, default=Path("build/polyphony_06_summary.json"))
args = parser.parse_args()
def fields(line):
    return {key: int(value) if value.isdigit() else value for key, value in re.findall(r"(\w+)=(\S+)", line)}

report = {"platform": "host only; not P4 deadlines, heap, USB/I2S or auditory qualification", "profiles": {}}
reference = {}
for voices in args.voices:
    folder = Path("build") / ("smk_p4_amy" if voices == 4 else f"smk_p4_amy_{voices}")
    log_bytes = Path(f"build/profile{voices}_{args.log_suffix}.log").read_bytes()
    log = log_bytes.decode("utf-16" if log_bytes.startswith(b"\xff\xfe") else "utf-8-sig")
    assert f"PASS capacity={voices}" in log
    if args.probe_suffix:
        data = Path(f"build/profile{voices}_{args.probe_suffix}.log").read_bytes()
        log = data.decode("utf-16" if data.startswith(b"\xff\xfe") else "utf-8-sig")
        assert f"PASS capacity={voices}" in log
    memory = re.findall(r"^MEM .*$", log, re.M)
    assert len(memory) == 1
    row = {"memory": fields(memory[0]), "host_timing": [fields(line) for line in re.findall(r"^HOST .*$", log, re.M)], "wav": {}}
    assert row["host_timing"]
    for patch in (128, 135):
        for stimulus in ("profile", "full"):
            path = folder / f"{stimulus}{voices}_patch_{patch}.wav"
            with wave.open(str(path), "rb") as w:
                assert (w.getframerate(), w.getnchannels(), w.getsampwidth()) == (48000, 2, 2)
                pcm = w.readframes(w.getnframes())
                frames = w.getnframes()
            samples = array.array("h", pcm)
            if sys.byteorder != "little":
                samples.byteswap()
            peak = max(abs(x) for x in samples)
            assert peak > 0 and all(-32768 < x < 32767 for x in samples)
            if stimulus == "profile":
                if patch in reference:
                    assert pcm == reference[patch], f"Common-stimulus PCM differs: {voices}/{patch}"
                else:
                    reference[patch] = pcm
            row["wav"][path.name] = {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "frames": frames, "peak": peak, "rms": math.sqrt(sum(x*x for x in samples)/len(samples))}
    report["profiles"][voices] = row
args.output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
print(f"PASS host evidence; common-stimulus PCM identical across {args.voices}: {args.output}")
