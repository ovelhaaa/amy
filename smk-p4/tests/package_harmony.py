"""Freeze a software review bundle. Never opens serial or flashes a device."""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import uuid
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "f40f34b4"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert not git("status", "--porcelain"), "Commit review sources first"
    assert not git("diff", BASELINE, "HEAD", "--", "src", "smk-s3", "shared")
    commit = git("rev-parse", "HEAD")
    target = ROOT / "build" / f"harmony_09_review_{commit[:8]}_{uuid.uuid4().hex[:8]}"
    target.mkdir(exist_ok=False)
    manifest = {"schema_version": 1, "source_commit": commit, "baseline_commit": git("rev-parse", BASELINE),
                "branch": git("branch", "--show-current"), "flash_authorized": False,
                "device_backup_verified_in_this_task": False, "hardware_qualified": False,
                "hardware_smoke": "NOT RUN", "gate0_physical": "MANUAL / NOT RUN",
                "prolonged_qualification": "PENDING", "voices": 8, "profiles": {}, "files": {}}

    def copy(source, name):
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
        assert sha(source) == sha(dest)

    rollback = ROOT.parent / "smk-p4-unified-patch-engine-08/smk-p4/.pio/build/guition-unified-engine-8-bench/firmware.bin"
    if rollback.is_file():
        expected = "a5b410c29b677fcdc894ee78b1f2a22c64d98594143f460edd7800a4965e8617"
        assert sha(rollback) == expected, "Preserved0.8 image differs"
        copy(rollback, "rollback/preserved_08_application.bin")
        manifest["preserved_08_application_sha256"] = expected
        manifest["rollback_requires_current_device_backup_and_authorization"] = True
    tracked = git("ls-files", "src", "smk-p4", "shared").splitlines()
    manifest["product_sources_sha256_lf"] = {
        name: hashlib.sha256((ROOT / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest()
        for name in tracked if Path(name).suffix in (".c", ".cpp", ".h", ".txt", ".yml", ".ini")
        or Path(name).name in ("Kconfig", "dependencies.lock") or "sdkconfig.defaults" in name}
    for env, audible in (("guition-harmony-8", False), ("guition-harmony-8-bench", True)):
        args = [sys.executable, "smk-p4/tests/check_blueprint_build.py", "--voices", "8", "--sound-library",
                "--unified-engine", "--harmony", "--environment", env]
        if audible:
            args.append("--musical")
        audit = subprocess.check_output(args, cwd=ROOT, text=True)
        build = ROOT / "smk-p4/.pio/build" / env
        for name in tracked:
            source = ROOT / name
            if name.startswith(("smk-p4/main/", "smk-p4/components/")) and source.suffix in (".cpp", ".c", ".h"):
                assert source.stat().st_mtime <= (build / "firmware.bin").stat().st_mtime, f"Stale image: {name}"
        for name in ("firmware.bin", "firmware.elf", "firmware.map", "bootloader.bin", "partitions.bin", "sdkconfig",
                     "config/sdkconfig.h", "compile_commands.json", "flasher_args.json", "project_description.json"):
            copy(build / name, f"{env}/{name}")
        (target / env / "audit.log").write_text(audit, encoding="utf-8")
        entries = json.loads((build / "compile_commands.json").read_text())
        command = next(e["command"] for e in entries if e["file"].replace("\\", "/").endswith("/src/amy.c"))
        match = re.search(r'"([^"\n]*riscv32-esp-elf-gcc(?:\.exe)?)"|(\S*riscv32-esp-elf-gcc(?:\.exe)?)', command)
        assert match
        compiler = Path(match.group(1) or match.group(2))
        tool = compiler.with_name("riscv32-esp-elf-size" + compiler.suffix)
        sizes = subprocess.check_output([str(tool), "-A", str(build / "firmware.elf")], text=True)
        (target / env / "sections.log").write_text(sizes, encoding="utf-8")
        sections = {name: int(size) for name, size in re.findall(r"^(\.\S+)\s+(\d+)\s+\d+$", sizes, re.M)}
        manifest["profiles"][env] = {"audible_candidate": audible, "firmware_sha256": sha(build / "firmware.bin"),
            "firmware_bytes": (build / "firmware.bin").stat().st_size,
            "compiled_project_version": json.loads((build / "project_description.json").read_text()).get("project_version"),
            "sections_bytes": {name: size for name, size in sections.items() if name.startswith((".dram", ".iram", ".rtc", ".tcm"))}}
    for file in (ROOT / "build/harmony09").rglob("*"):
        if file.is_file() and file.suffix in (".log", ".png", ".pcm", ".json"):
            copy(file, "evidence/" + file.relative_to(ROOT / "build/harmony09").as_posix())
    for file in (ROOT / "smk-p4/docs").glob("harmony_09*.md"):
        copy(file, "docs/" + file.name)
    copy(ROOT / "smk-p4/docs/output_08_gap_classification.md", "docs/output_08_gap_classification.md")
    subprocess.check_call(["git", "archive", "--format=zip", "--output=" + str(target / "source.zip"), commit], cwd=ROOT)
    (target / "change.patch").write_bytes(subprocess.check_output(["git", "diff", BASELINE, commit], cwd=ROOT))
    for file in target.rglob("*"):
        if file.is_file():
            manifest["files"][file.relative_to(target).as_posix()] = sha(file)
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    archive = target.with_suffix(".zip")
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as bundle:
        for file in target.rglob("*"):
            if file.is_file():
                bundle.write(file, file.relative_to(target).as_posix())
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.testzip() is None
    print(json.dumps({"bundle": str(archive), "manifest": str(target / "manifest.json"),
                      "sha256": sha(archive), "profiles": manifest["profiles"], "flash_authorized": False}, indent=2))


if __name__ == "__main__":
    main()
