"""Package firmware images from sequential, successful builds of a clean commit."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[2]
PROFILES = ("guition-editor-8", "guition-editor-8-bench")
FLAGS = ("PCM_WIRING_CONFIRMED", "USB_WIRING_CONFIRMED", "USB_DESCRIPTOR_LOG",
         "USB_MIDI_TRACE", "HARDWARE_017B_TELEMETRY", "PCM_DIAGNOSTIC", "AMY_RAM_SELF_TEST")
PARTITION = struct.Struct("<HBBII16sI")
APP_DESC = 32  # ESP image header (24 bytes) + first segment header (8 bytes).


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_descriptor(image):
    if len(image) < APP_DESC + 256 or image[0] != 0xE9 or image[32:36] != bytes.fromhex("3254cdab"):
        raise ValueError("ESP application descriptor missing")
    app = image[APP_DESC:]
    return {"idf_app_version": app[16:48].split(b"\0")[0].decode("ascii"),
            "idf_version": app[112:144].split(b"\0")[0].decode("ascii"),
            "elf_sha256": app[144:176].hex()}


def partitions(path):
    rows = []
    raw = path.read_bytes()
    for magic, kind, subtype, offset, size, name, flags in PARTITION.iter_unpack(raw):
        if magic in (0xEBEB, 0xFFFF):
            break
        if magic != 0x50AA or flags:
            raise ValueError("unexpected binary partition entry")
        rows.append({"name": name.split(b"\0")[0].decode(), "type": kind, "subtype": subtype,
                     "offset": offset, "size": size})
    return rows


def source_partitions(path):
    kinds = {"data": 1, "app": 0}
    subtypes = {"nvs": 2, "phy": 1, "factory": 0}
    with path.open() as source:
        return [{"name": row[0].strip(), "type": kinds[row[1].strip()],
                 "subtype": subtypes[row[2].strip()], "offset": int(row[3], 0), "size": int(row[4], 0)}
                for row in csv.reader(line for line in source if line.strip() and not line.lstrip().startswith("#"))]


def config_flags(path):
    lines = set(path.read_text(encoding="utf-8").splitlines())
    return {key: f"CONFIG_SMKP4_{key}=y" in lines for key in FLAGS}


def validate(bundle, profile):
    manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
    firmware = bundle / "firmware.bin"
    table = bundle / "partition-table.bin"
    if manifest["schema"] != 1 or manifest["profile"] != profile or profile not in PROFILES:
        raise ValueError("profile mismatch")
    if manifest["firmware_version"] != "0.17B" or not re.fullmatch(r"[a-f0-9]{40}", manifest["commit"]):
        raise ValueError("invalid firmware version or source commit")
    if sha(firmware) != manifest["firmware_sha256"] or firmware.stat().st_size != manifest["size_bytes"]:
        raise ValueError("firmware size or SHA-256 mismatch")
    descriptor = image_descriptor(firmware.read_bytes())
    if any(manifest.get(key) != value for key, value in descriptor.items()):
        raise ValueError("application descriptor mismatch")
    if sha(table) != manifest["partition_sha256"] or partitions(table) != manifest["partitions"]:
        raise ValueError("partition table mismatch")
    if manifest["partitions"] != source_partitions(PROJECT / "partitions.csv"):
        raise ValueError("bundled partition table differs from repository")
    factory = next(row for row in manifest["partitions"] if row["name"] == "factory")
    if manifest["app_offset"] != factory["offset"] or manifest["size_bytes"] > factory["size"]:
        raise ValueError("application offset or capacity mismatch")
    bench = profile.endswith("-bench")
    expected = {key: bench for key in FLAGS[:-2]} | {key: False for key in FLAGS[-2:]}
    config = bundle / "sdkconfig"
    if (sha(config) != manifest["sdkconfig_sha256"] or manifest["flags"] != expected
            or config_flags(config) != expected):
        raise ValueError("unexpected effective hardware or bench flags")
    return manifest


def package(profile, destination, log, commit):
    build = PROJECT / ".pio" / "build" / profile
    app = build / "firmware.bin"
    table = build / "partitions.bin"
    config = build / "sdkconfig"
    flash = json.loads((build / "flasher_args.json").read_text(encoding="utf-8"))
    text = log.read_text(encoding="utf-8-sig", errors="replace")
    if f"Processing {profile} (" not in text or not re.search(rf"^{profile}\s+SUCCESS\b", text, re.M):
        raise ValueError("successful build summary for this profile required")
    sizes = dict(re.findall(r"^(RAM|Flash):[^\r\n]*\(used (\d+) bytes", text, re.M))
    manifest = {"schema": 1, "firmware_version": "0.17B", "commit": commit, "profile": profile,
                "firmware_sha256": sha(app), "size_bytes": app.stat().st_size,
                "partition_sha256": sha(table), "partitions": partitions(table),
                "app_offset": int(flash["app"]["offset"], 0),
                "pio_ram_bytes": int(sizes["RAM"]), "pio_flash_bytes": int(sizes["Flash"]),
                "flags": config_flags(config), "sdkconfig_sha256": sha(config),
                **image_descriptor(app.read_bytes())}
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise FileExistsError(destination)
    with tempfile.TemporaryDirectory(dir=destination.parent) as temp:
        stage = Path(temp) / "bundle"
        stage.mkdir()
        for path in (app, config, log):
            shutil.copy2(path, stage)
        shutil.copy2(table, stage / "partition-table.bin")
        (stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        validate(stage, profile)
        stage.rename(destination)



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("package", "check"))
    parser.add_argument("--profile", choices=PROFILES, required=True)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    if args.action == "package":
        if not args.log:
            parser.error("--log required to package")
        git = ["git", "-C", str(PROJECT)]
        if subprocess.check_output([*git, "status", "--porcelain"], text=True).strip():
            parser.error("package from a clean commit")
        commit = subprocess.check_output([*git, "rev-parse", "HEAD"], text=True).strip()
        package(args.profile, args.bundle, args.log, commit)
    else:
        print(json.dumps(validate(args.bundle, args.profile)))


if __name__ == "__main__":
    main()
