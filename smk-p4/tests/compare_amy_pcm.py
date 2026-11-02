"""Compare the identical RAM diagnostic on host/P4; not analog audio approval."""
import argparse
from pathlib import Path
import re

PATTERN = re.compile(r"RAM_SELF_TEST patch=(\d+) peak=(\d+) rms=([0-9.]+) clipped=(\d+) release=(\d) panic=(\d)")


def metrics(path):
    result = {}
    for match in PATTERN.finditer(path.read_text(errors="replace")):
        patch, peak, rms, clipped, release, panic = match.groups()
        result[int(patch)] = (int(peak), float(rms), int(clipped), int(release), int(panic))
    if set(result) != {128, 135}:
        raise ValueError(f"Incomplete ROM diagnostic: {path}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--p4", type=Path, required=True)
    parser.add_argument("--tolerance", type=float, default=0.10)
    args = parser.parse_args()
    if not 0 <= args.tolerance <= 0.25:
        parser.error("tolerance must be in [0, 0.25]")
    host, p4 = metrics(args.host), metrics(args.p4)
    for patch in host:
        for label, values in (("host", host[patch]), ("P4", p4[patch])):
            if values[0] <= 0 or values[1] <= 0 or values[2:] != (0, 1, 1):
                raise ValueError(f"Invalid {label} PCM/release/Panic: patch {patch}: {values}")
        for index, name in ((0, "peak"), (1, "RMS")):
            difference = abs(p4[patch][index] / host[patch][index] - 1)
            if difference > args.tolerance:
                raise ValueError(f"Patch {patch} {name}: relative difference {difference:.2%}")
        print(f"PASS: ROM {patch}: host={host[patch]} P4={p4[patch]} tolerance={args.tolerance:.0%}")


if __name__ == "__main__":
    main()
