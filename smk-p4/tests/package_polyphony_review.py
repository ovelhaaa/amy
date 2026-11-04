"""Freeze safe, disabled 0.6 build artifacts for review; performs no device I/O."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid
import zipfile

repo = Path(__file__).resolve().parents[2]

def git(*args):
    return subprocess.check_output(["git", *args], cwd=repo, text=True, encoding="utf-8").strip()

def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024*1024), b""):
            digest.update(block)
    return digest.hexdigest()

commit = git("rev-parse", "HEAD")
production = ["src", "smk-p4/main", "smk-p4/components", "smk-p4/platformio.ini", "smk-p4/sdkconfig.defaults.voices6", "smk-p4/sdkconfig.defaults.voices8"]
assert not git("diff", "HEAD", "--", *production), "Commit production changes before freezing candidates"
root = repo / "build" / f"polyphony_06_review_{commit[:8]}_{uuid.uuid4().hex[:8]}"
root.mkdir(parents=True, exist_ok=False)
manifest = {"schema_version": 1, "source_commit": commit, "branch": git("branch", "--show-current"), "working_tree_status": git("status", "--short"), "image_kind": "COMPILE VALIDATION; I2S AND USB DISABLED", "hardware_qualified": False, "flash_authorized": False, "profiles": {}, "files": {}}

def copy(source, destination):
    target = root / destination
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    assert sha(source) == sha(target)

for voices in (4, 6, 8):
    env = "guition-standalone-blueprint" + (f"-{voices}" if voices != 4 else "")
    build = repo / "smk-p4/.pio/build" / env
    audit = subprocess.check_output([sys.executable, str(repo / "smk-p4/tests/check_blueprint_build.py"), "--voices", str(voices)], cwd=repo, text=True)
    (root / env).mkdir()
    (root / env / "audit.log").write_text(audit, encoding="utf-8")
    header = (build / "config/sdkconfig.h").read_text()
    assert re.search(rf"^#define CONFIG_SMKP4_AMY_VOICES {voices}$", header, re.M)
    assert "#define CONFIG_SMKP4_PCM_WIRING_CONFIRMED 1" not in header
    assert "#define CONFIG_SMKP4_USB_WIRING_CONFIRMED 1" not in header
    for name in ("firmware.bin", "firmware.elf", "firmware.map", "bootloader.bin", "partitions.bin", "sdkconfig", "config/sdkconfig.h", "compile_commands.json", "flasher_args.json"):
        copy(build / name, f"{env}/{name}")
    commands = json.loads((build / "compile_commands.json").read_text())
    command = next(e["command"] for e in commands if e["file"].replace("\\", "/").endswith("/src/amy.c"))
    match = re.search(r'"([^"\n]*riscv32-esp-elf-gcc(?:\.exe)?)"|(\S*riscv32-esp-elf-gcc(?:\.exe)?)', command)
    assert match
    compiler = Path(match.group(1) or match.group(2))
    tool = compiler.with_name("riscv32-esp-elf-size" + (".exe" if compiler.suffix == ".exe" else ""))
    sizes = subprocess.check_output([str(tool), "-A", str(build / "firmware.elf")], text=True)
    sections = {name: int(count) for name, count, address in re.findall(r"^(\.\S+)\s+(\d+)\s+(\d+)$", sizes, re.M) if name.startswith((".dram", ".iram", ".tcm", ".rtc"))}
    dram = sum(sections.get(name, 0) for name in (".dram0.data", ".dram1.data", ".dram0.bss", ".dram1.bss"))
    native = json.loads((build / "flasher_args.json").read_text())
    manifest["profiles"][voices] = {"environment": env, "firmware_sha256": sha(build / "firmware.bin"), "bytes": (build / "firmware.bin").stat().st_size, "static_dram_data_bss_bytes": dram, "static_sections_bytes": sections, "offsets_from_native_build": {key: native[key]["offset"] for key in ("bootloader", "app", "partition-table")}}
    audio = repo / "build" / ("smk_p4_amy" if voices == 4 else f"smk_p4_amy_{voices}")
    for prefix in ("profile", "full"):
        for patch in (128, 135):
            name = f"{prefix}{voices}_patch_{patch}.wav"
            copy(audio / name, f"audio/{name}")
    for suffix in ("tests_final", "capacity_quiet"):
        name = f"profile{voices}_{suffix}.log"
        copy(repo / "build" / name, f"evidence/{name}")
for name in ("polyphony_06_results.md", "polyphony_06_memory_budget.md", "polyphony_06_performance_matrix.md", "audio_quality_05a_verification.md"):
    copy(repo / "smk-p4/docs" / name, f"docs/{name}")
for name in ("polyphony_06_summary.json", "gate0_final_provenance.log"):
    copy(repo / "build" / name, f"evidence/{name}")
log_data = (repo / "build/gate0_final_provenance.log").read_bytes()
log_text = log_data.decode("utf-16" if log_data.startswith(b"\xff\xfe") else "utf-8-sig")
evidence_match = re.search(r"^Evidence:\s+(.+)$", log_text, re.M)
assert evidence_match, "Missing independent A/B evidence path"
evidence = Path(evidence_match.group(1).strip()).resolve()
assert evidence.is_relative_to((repo / "build/retrigger_ab").resolve())
copy(evidence / "manifest.json", "ab/manifest.json")
ab = json.loads((evidence / "manifest.json").read_text(encoding="utf-8-sig"))
for test, expected in ab["harness_sha256"].items():
    source = repo / "smk-p4/tests" / f"{test}.cpp"
    assert sha(source).upper() == expected.upper()
    copy(source, f"source/harness/{test}.cpp")
for variant in ("baseline", "integration"):
    for patch in (128, 135):
        name = f"{variant}_patch_{patch}.wav"
        copy(evidence / name, f"ab/{name}")
    for test in ("render_retrigger_probe", "test_retrigger_scenarios"):
        name = f"{variant}_{test}.log"
        copy(evidence / name, f"ab/{name}")
for name in ("polyphony_final_uptodate.log", "standalone_legacy_firmware.log", "s3_link_firmware_final.log", "s3_amy_checked_pwsh.log", "s3_amy_legacy_pwsh.log", "s3_amy_128.log", "polyphony_host.log", "polyphony_usb.log", "polyphony_safety.log", "smk_ui.log", "smk_link.log", "blueprint_preview_final.log", "profile8_capacity_final.log", "profile6_firmware.log", "profile8_firmware.log"):
    copy(repo / "build" / name, f"evidence/{name}")
for voices in (4, 6, 8):
    name = f"blueprint_adapter{voices}_final_verified.log"
    copy(repo / "build" / name, f"evidence/{name}")
copy(repo / "smk-p4/platformio.ini", "source/platformio.ini")
copy(repo / "smk-p4/dependencies.lock", "source/dependencies.lock")
(root / "source/changes.patch").write_text(git("diff", "dd4753af", commit, "--", "smk-p4", "src", ".github/workflows/smk-p4-build.yml")+"\n", encoding="utf-8")
for path in sorted(root.rglob("*")):
    if path.is_file():
        manifest["files"][path.relative_to(root).as_posix()] = sha(path)
(root / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
for name, digest in manifest["files"].items():
    assert sha(root / name) == digest
archive = root.with_suffix(".zip")
with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as zipped:
    for path in sorted(root.rglob("*")):
        if path.is_file():
            zipped.write(path, path.relative_to(root).as_posix())
with zipfile.ZipFile(archive) as zipped:
    assert zipped.testzip() is None
print(json.dumps({"bundle": str(root), "zip": str(archive), "zip_sha256": sha(archive)}, indent=2))
