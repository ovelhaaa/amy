"""Read-only verification of the recorded 0.3A bundle/config; never flashes.

No implicit search in another worktree. Binary verification needs --image-dir;
--sdkconfig compares a generated build to the recorded musical profile.
"""
import argparse
import hashlib
import json
from pathlib import Path


def config_values(path):
    values = {}
    for line in path.read_text().splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-dir", type=Path)
    parser.add_argument("--sdkconfig", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / "docs" / "baselines"
    manifest = json.loads((root / "standalone_03a.json").read_text())
    for name, expected in manifest["tracked_reference_files"].items():
        data = (root / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit(f"FAIL: reference changed: {name}")
    frozen = config_values(root / "standalone_03a.sdkconfig")
    for key, expected in manifest["required_config"].items():
        if frozen.get(key, "n") != expected:
            raise SystemExit(f"FAIL: frozen config mismatch: {key}")
    print("PASS: versioned 0.3A configuration/lock hashes and musical profile")
    if args.image_dir:
        for name, expected in manifest["local_bundle_files"].items():
            path = args.image_dir / name
            if not path.is_file():
                raise SystemExit(f"BLOCKED: exact reference bundle file missing: {path}")
            data = path.read_bytes()  # Never substitute a newly compiled image.
            if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
                raise SystemExit(f"FAIL: size/hash mismatch: {path}")
        print("PASS: exact local 0.3A bundle, including firmware/bootloader/partitions/config")
    else:
        print("NOT RUN: exact binary verification (supply --image-dir explicitly)")
    if args.sdkconfig:
        generated = config_values(args.sdkconfig)
        for key, expected in manifest["required_config"].items():
            if generated.get(key, "n") != expected:
                raise SystemExit(f"FAIL: generated config mismatch: {key}")
        print("PASS: effective musical build configuration; not binary/audio qualification")


if __name__ == "__main__":
    main()
