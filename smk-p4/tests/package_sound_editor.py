"""Freeze a reviewable0.11 bundle. Never opens serial, flashes or publishes."""
import hashlib
import json
import shutil
import subprocess
import uuid
import wave
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "dfcef41dd04eaa034fc4c20945bfd95ce65062d9"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, encoding="utf-8").strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert not git("status", "--porcelain"), "Commit review sources first"
    assert not git("diff", BASELINE, "HEAD", "--", "src", "smk-s3", "shared"), "Upstream/other targets changed"
    markers = {
        "regressions.log": ["PASS editor:", "PASS real AMY PCM", "PASS factory Runtime swaps=687"],
        "ui-run.log": ["PASS LVGL9.3:"],
        "legacy-factory-ui.log": ["PASS factory live adapter"],
        "factory-pcm-comparison.log": ["PASS byte-identical 0.11 no-overrides off_4.pcm", "off_135.pcm"],
        "audit-safe.log": ["PASS: effective Blueprint"],
        "audit-bench.log": ["PASS: effective Blueprint"],
    }
    for name, expected in markers.items():
        evidence = (ROOT / "build/editor011" / name).read_text(encoding="utf-8")
        assert all(marker in evidence for marker in expected), f"Missing passing evidence: {name}"
    commit = git("rev-parse", "HEAD")
    target = ROOT / "build" / f"editor_011_review_{commit[:8]}_{uuid.uuid4().hex[:8]}"
    target.mkdir(exist_ok=False)
    manifest = {"format": 1, "source_commit": commit, "baseline": BASELINE,
                "branch": git("branch", "--show-current"), "flash_authorized": False,
                "hardware_smoke": "MANUAL / NOT RUN", "fresh_device_read": "BLOCKED: COM13 open stalled",
                "prior_rollback_rehashed": True, "voices": 8, "oscillators": 64,
                "user_slots": 16, "profiles": {}, "files": {}}

    def copy(source, relative):
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        assert sha(source) == sha(destination)
        return destination

    previous = ROOT.parent / "smk-p4-full-factory-library-010/build/factory010"
    backup_a = previous / "preflight/installed_full_a.bin"
    backup_b = previous / "preflight/installed_full_b.bin"
    expected = "9a7589434ba52614dfc4b19d1b6a11c76536f471fbfb6f2f976e5f219ec9c30a"
    assert sha(backup_a) == sha(backup_b) == expected
    copy(backup_a, "rollback/full_09_verified.bin")
    prefix = previous / "flash/postflash_prefix.bin"
    old_app = prefix.read_bytes()[0x10000:0x10000 + 1249104]
    assert hashlib.sha256(old_app).hexdigest() == "0652377914fce06a385bab4388049fac0d427e90eac204252f08c05144f8adc7"
    (target / "rollback/application_010_verified.bin").write_bytes(old_app)
    copy(previous / "preflight/installed_partitions.bin", "rollback/installed_partitions.bin")
    copy(previous / "flash/readback.json", "rollback/prior_readback_receipt.json")

    products = [ROOT / name for name in git("ls-files", "smk-p4/main", "smk-p4/components", "smk-p4/platformio.ini", "smk-p4/CMakeLists.txt", "smk-p4/sdkconfig.defaults*", "smk-p4/partitions.csv").splitlines()]
    manifest["product_sources"] = {str(path.relative_to(ROOT)).replace("\\", "/"): sha(path) for path in products}
    for environment in ("guition-editor-8", "guition-editor-8-bench"):
        build = ROOT / "smk-p4/.pio/build" / environment
        binary = build / "firmware.bin"
        assert max(path.stat().st_mtime for path in products) <= binary.stat().st_mtime, "Stale product image"
        assert (build / "partitions.bin").read_bytes() == (previous / "preflight/installed_partitions.bin").read_bytes(), "Partition mismatch"
        config = (build / "config/sdkconfig.h").read_text()
        for flag in ("CONFIG_SMKP4_SOUND_EDITOR", "CONFIG_SMKP4_FULL_FACTORY_LIBRARY", "CONFIG_SMKP4_HARMONY", "CONFIG_SMKP4_UNIFIED_ENGINE"):
            assert f"#define {flag} 1" in config
        assert b"SMK-P4 EDITOR 0.11 EXPERIMENTAL" in binary.read_bytes()
        for name in ("firmware.bin", "firmware.elf", "firmware.map", "bootloader.bin", "partitions.bin", "sdkconfig", "config/sdkconfig.h", "compile_commands.json"):
            copy(build / name, f"{environment}/{name}")
        manifest["profiles"][environment] = {"bytes": binary.stat().st_size, "sha256": sha(binary)}
    subprocess.run(["git", "archive", "--format=zip", "--output", str(target / "sources.zip"), "HEAD"], cwd=ROOT, check=True)
    artifacts = ROOT / "build/editor011"
    for path in artifacts.iterdir():
        if path.suffix in (".log", ".png"):
            copy(path, f"evidence/{path.name}")
        if path.name.startswith("audio_") and path.suffix == ".pcm":
            with wave.open(str(target / "evidence" / (path.stem + ".wav")), "wb") as output:
                output.setnchannels(2)
                output.setsampwidth(2)
                output.setframerate(48000)
                output.writeframes(path.read_bytes())
    for directory in ("pcm_baseline", "pcm_candidate"):
        for path in (artifacts / directory).glob("*.pcm"):
            copy(path, f"evidence/{directory}/{path.name}")
    for path in target.rglob("*"):
        if path.is_file():
            manifest["files"][str(path.relative_to(target)).replace("\\", "/")] = {"bytes": path.stat().st_size, "sha256": sha(path)}
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive = Path(shutil.make_archive(str(target), "zip", target))
    with zipfile.ZipFile(archive) as handle:
        assert handle.testzip() is None
    print(json.dumps({"bundle": str(target), "archive": str(archive), "sha256": sha(archive), "profiles": manifest["profiles"]}, indent=2))


if __name__ == "__main__":
    main()
