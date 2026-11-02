"""Check safe target defaults and compile the enabled audio path without flashing.

Uses the toolchain/flags emitted by the actual CMake build. Does not edit the
sdkconfig, replace production objects, or produce an enabled firmware image.
"""
import json
import os
from pathlib import Path
import re
import shlex
import subprocess


def main():
    build = Path(__file__).resolve().parents[1] / ".pio/build/guition-standalone"
    config = (build / "config/sdkconfig.h").read_text()
    for required in ("CONFIG_IDF_TARGET_ESP32P4", "CONFIG_SMKP4_STANDALONE",
                     "CONFIG_ESP32P4_SELECTS_REV_LESS_V3"):
        assert f"#define {required} 1" in config, required
    for disabled in ("CONFIG_SMK_LINK_ENABLED", "CONFIG_SMKP4_PCM_WIRING_CONFIRMED",
                     "CONFIG_SMKP4_PCM_TEST_SEQUENCE"):
        assert f"#define {disabled} 1" not in config, disabled
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in commands if item["file"].endswith("standalone_main.cpp"))
    assert "audio_pcm5102" in entry["command"], "Missing native IDF audio dependency"
    output = (build / "enabled-main-check.o").as_posix()
    command, count = re.subn(r'-o\s+("[^"]+"|\S+)', f'-o "{output}"', entry["command"])
    assert count == 1, "Expected exactly one compiler output"
    command += " -DCONFIG_SMKP4_PCM_WIRING_CONFIRMED=1 -DCONFIG_SMKP4_PCM_TEST_SEQUENCE=1"
    subprocess.run(command if os.name == "nt" else shlex.split(command),
                   cwd=entry["directory"], check=True)
    print("PASS: P4 ES, safe firmware defaults, native includes and enabled-path compile")


if __name__ == "__main__":
    main()
