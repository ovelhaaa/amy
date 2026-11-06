"""Freeze review artifacts only. No serial, reset, upload or approval action."""
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


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert not git("status", "--porcelain"), "Commit the review before freezing artifacts"
    assert not git("diff", "bd564872", "HEAD", "--", "src"), "Shared AMY unexpectedly modified"
    commit = git("rev-parse", "HEAD")
    target = ROOT / "build" / f"unified_engine_08_review_{commit[:8]}_{uuid.uuid4().hex[:8]}"
    target.mkdir(parents=True, exist_ok=False)
    metrics = json.loads((ROOT / "smk-p4/docs/unified_engine_08_host_metrics.json").read_text())
    preflight = json.loads((ROOT / "build/unified08/preflight.json").read_text())
    assert preflight.get("independent_reads_match") is True
    assert preflight.get("installed_matches_07_usb64") is True
    assert preflight.get("candidate_partition_matches_installed") is True
    assert sha(ROOT / "build/unified08/preflash_full_16m.bin") == preflight["backup_sha256"]
    assert sha(ROOT / "build/unified08/preflash_verify_16m.bin") == preflight["backup_sha256"]
    rollback = ROOT / "build/unified08/rollback_07_usb64_full.zip"
    assert sha(rollback) == preflight["rollback_bundle_sha256"]
    assert len(metrics["patches"]) == 16 and sum(p["software_candidate"] for p in metrics["patches"]) == 15
    for path, expected in metrics["evidence_sha256"].items():
        assert sha(ROOT / path) == expected, "Evidence changed: regenerate matrix"
    manifest = {"schema_version": 1, "source_commit": commit, "branch": git("branch", "--show-current"),
                "baseline_commit": git("rev-parse", "bd564872"), "hardware_qualified": False,
                "flash_authorized": False, "device_backup_verified_in_this_task": True,
                "voices": 8, "resident_instruments": 8, "oscillator_capacity": 64,
                "software_patch_ids": [p["id"] for p in metrics["patches"] if p["software_candidate"]],
                "profiles": {}, "files": {}}
    manifest["product_sources_sha256_lf"] = {
        name: hashlib.sha256((ROOT / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest()
        for name in git("ls-files", "src", "smk-p4/main", "smk-p4/components").splitlines()
        if Path(name).suffix in (".c", ".cpp", ".h", ".txt", ".yml") or Path(name).name == "Kconfig"}

    def copy(source, name):
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
        assert sha(source) == sha(dest)

    for environment, audible in (("guition-unified-engine-8", False), ("guition-unified-engine-8-bench", True)):
        audit_args = [sys.executable, str(ROOT / "smk-p4/tests/check_blueprint_build.py"), "--voices", "8",
                      "--sound-library", "--unified-engine", "--environment", environment]
        if audible:
            audit_args.append("--musical")
        audit = subprocess.check_output(audit_args, cwd=ROOT, text=True)
        build = ROOT / "smk-p4/.pio/build" / environment
        # Review pins exact current product files, independently of git version
        # strings generated before later documentation-only commits.
        for pattern in ("smk-p4/components/amy_runtime/*.*", "smk-p4/main/*.cpp", "smk-p4/main/*.h",
                        "smk-p4/components/ui_blueprint/**/*.cpp", "smk-p4/components/ui_blueprint/**/*.h"):
            for source in ROOT.glob(pattern):
                assert source.stat().st_mtime <= (build / "firmware.bin").stat().st_mtime, f"Stale image: {source}"
        for name in ("firmware.bin", "firmware.elf", "firmware.map", "bootloader.bin", "partitions.bin", "sdkconfig",
                     "config/sdkconfig.h", "compile_commands.json", "flasher_args.json", "project_description.json"):
            copy(build / name, f"{environment}/{name}")
        (target / environment / "audit.log").write_text(audit, encoding="utf-8")
        entries = json.loads((build / "compile_commands.json").read_text())
        command = next(e["command"] for e in entries if e["file"].replace("\\", "/").endswith("/src/amy.c"))
        match = re.search(r'"([^"\n]*riscv32-esp-elf-gcc(?:\.exe)?)"|(\S*riscv32-esp-elf-gcc(?:\.exe)?)', command)
        assert match
        compiler = Path(match.group(1) or match.group(2))
        tool = compiler.with_name("riscv32-esp-elf-size" + compiler.suffix)
        sizes = subprocess.check_output([str(tool), "-A", str(build / "firmware.elf")], text=True)
        (target / environment / "sections.log").write_text(sizes, encoding="utf-8")
        sections = {name: int(size) for name, size in re.findall(r"^(\.\S+)\s+(\d+)\s+\d+$", sizes, re.M)}
        manifest["profiles"][environment] = {"audible_candidate": audible, "flash_authorized": False,
            "compiled_project_version": json.loads((build / "project_description.json").read_text()).get("project_version"),
            "firmware_sha256": sha(build / "firmware.bin"), "firmware_bytes": (build / "firmware.bin").stat().st_size,
            "static_dram_bytes": sum(sections.get(n, 0) for n in (".dram0.data", ".dram1.data", ".dram0.bss", ".dram1.bss")),
            "iram_bytes": sum(size for name, size in sections.items() if name.startswith(".iram")),
            "sections_bytes": {name: size for name, size in sections.items() if name.startswith((".dram", ".iram", ".rtc", ".tcm"))}}
        if audible:
            assert manifest["profiles"][environment]["firmware_sha256"] == preflight["candidate_app_sha256"]
    for file in (ROOT / "build/unified08").glob("*.wav"):
        copy(file, "audio/" + file.name)
    for file in (ROOT / "build/unified08").glob("*.log"):
        copy(file, "logs/" + file.name)
    for file in (ROOT / "build/unified08").glob("*.json"):
        copy(file, "metrics/" + file.name)
    for name in ("browser.ppm", "browser.png"):
        if (ROOT / "build/unified08" / name).is_file():
            copy(ROOT / "build/unified08" / name, "ui/" + name)
    for pattern in ("regression08*.log", "unified08*.log", "ui08*.log"):
        for file in (ROOT / "build").glob(pattern):
            copy(file, "logs/" + file.name)
    retrigger_root = ROOT / "build/retrigger_ab"
    if retrigger_root.exists():
        for run in retrigger_root.iterdir():
            if not run.is_dir():
                continue
            for pattern in ("manifest.json", "*.wav", "*test_retrigger_scenarios.log"):
                for file in run.glob(pattern):
                    copy(file, f"retrigger_ab/{run.name}/{file.name}")
    copy(ROOT / "smk-p4/docs/unified_engine_08_host_metrics.json", "host_metrics.json")
    copy(rollback, "rollback/rollback_07_usb64_full.zip")
    subprocess.check_call(["git", "archive", "--format=zip", "--output=" + str(target / "source.zip"), commit], cwd=ROOT)
    (target / "change.patch").write_bytes(subprocess.check_output(["git", "diff", "bd564872", commit], cwd=ROOT))
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
    print(json.dumps({"bundle": str(archive), "sha256": sha(archive), "profiles": manifest["profiles"],
                      "flash_authorized": False}, indent=2))


if __name__ == "__main__":
    main()
