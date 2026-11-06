"""Freeze 0.7 review artifacts with hashes; never accesses a serial port."""
import hashlib
import argparse
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--authorization", type=Path, help="Existing operator authorization JSON for this concrete image")
    args = parser.parse_args()
    assert not git("status", "--porcelain"), "Commit changes before freezing a review bundle"
    commit = git("rev-parse", "HEAD")
    target = ROOT / "build" / ("sound_library_07_review_" + commit[:8] + "_" + uuid.uuid4().hex[:8])
    target.mkdir(parents=True, exist_ok=False)
    metrics = json.loads((ROOT / "build/sound_library_07_metrics.json").read_text(encoding="utf-8"))
    resident_patches = sorted({int(r["patch"]) for r in metrics["ram_self_test"]})
    assert len(resident_patches) == 4
    assert {int(r["voices"]) for r in metrics["runtime_scenes"]} == {8}
    manifest = {"schema_version": 1, "source_commit": commit,
                "baseline_commit": git("rev-parse", "feature/smk-p4-polyphony-06"),
                "branch": git("branch", "--show-current"), "hardware_qualified": False,
                "flash_authorized": False, "voices": 8, "resident_patches": resident_patches,
                "profiles": {}, "files": {}}
    def copy(source, name):
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
        assert sha(source) == sha(dest)
    for env, musical in [("guition-sound-library-8",False), ("guition-sound-library-8-bench",True)]:
        command = [sys.executable, str(ROOT / "smk-p4/tests/check_blueprint_build.py"),
                   "--voices", "8", "--sound-library", "--environment", env]
        if musical: command.append("--musical")
        audit = subprocess.check_output(command, cwd=ROOT, text=True)
        build = ROOT / "smk-p4/.pio/build" / env
        for file in ["firmware.bin","firmware.elf","firmware.map","bootloader.bin","partitions.bin",
                     "sdkconfig","config/sdkconfig.h","compile_commands.json","flasher_args.json","project_description.json"]:
            copy(build / file, env + "/" + file)
        (target / env / "audit.log").write_text(audit, encoding="utf-8")
        commands = json.loads((build / "compile_commands.json").read_text())
        amy_command = next(e["command"] for e in commands if e["file"].replace("\\", "/").endswith("/src/amy.c"))
        match = re.search(r'"([^"\n]*riscv32-esp-elf-gcc(?:\.exe)?)"|(\S*riscv32-esp-elf-gcc(?:\.exe)?)', amy_command)
        assert match
        compiler = Path(match.group(1) or match.group(2))
        tool = compiler.with_name("riscv32-esp-elf-size" + (".exe" if compiler.suffix == ".exe" else ""))
        sizes = subprocess.check_output([str(tool), "-A", str(build / "firmware.elf")], text=True)
        (target / env / "sections.log").write_text(sizes, encoding="utf-8")
        sections = {name: int(count) for name, count, address in re.findall(r"^(\.\S+)\s+(\d+)\s+(\d+)$", sizes, re.M) if name.startswith((".dram", ".iram", ".rtc", ".tcm"))}
        dram = sum(sections.get(name, 0) for name in (".dram0.data", ".dram1.data", ".dram0.bss", ".dram1.bss"))
        manifest["profiles"][env] = {"i2s_usb_enabled": musical, "firmware_sha256": sha(build / "firmware.bin"),
                                      "bytes": (build / "firmware.bin").stat().st_size,
                                      "static_dram_data_bss_bytes": dram, "sections_bytes": sections}
    if args.authorization:
        authorization = json.loads(args.authorization.read_text(encoding="utf-8"))
        assert authorization["authorized"] is True
        assert authorization["firmware_sha256"] == manifest["profiles"]["guition-sound-library-8-bench"]["firmware_sha256"]
        assert authorization["app_offset"] == "0x10000"
        manifest["flash_authorized"] = True
        copy(args.authorization, "authorization.json")
        for file in sorted(args.authorization.parent.iterdir()):
            if file.is_file() and file.suffix in (".log", ".json"):
                copy(file,"physical/" + file.name)
        rollback=args.authorization.parent / "rollback_06_eight.bin"
        if rollback.exists(): copy(rollback,"rollback/firmware_06_eight.bin")
    for file in sorted((ROOT / "build/smk_p4_amy_8_library").glob("*.wav")):
        copy(file, "audio/" + file.name)
    copy(ROOT / "build/sound_library_07_metrics.json", "metrics.json")
    for file in sorted((ROOT / "build/library07_visual").glob("*.png")):
        copy(file, "ui/" + file.name)
    for pattern in ["*07*.log", "library*.log", "adapter07*.log"]:
        for file in sorted((ROOT / "build").glob(pattern)):
            copy(file, "logs/" + file.name)
    subprocess.check_call(["git", "archive", "--format=zip", "--output=" + str(target / "source.zip"), commit], cwd=ROOT)
    patch = subprocess.check_output(["git", "diff", manifest["baseline_commit"], commit], cwd=ROOT)
    (target / "change.patch").write_bytes(patch)
    for file in sorted(target.rglob("*")):
        if file.is_file(): manifest["files"][file.relative_to(target).as_posix()] = sha(file)
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    archive = target.with_suffix(".zip")
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as bundle:
        for file in sorted(target.rglob("*")):
            if file.is_file(): bundle.write(file, file.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as bundle: assert bundle.testzip() is None
    print(str(archive))
    print("SHA256 " + sha(archive))
    print("Physical qualification is separate; flash_authorized=" + str(manifest["flash_authorized"]).lower())

if __name__ == "__main__":
    main()
