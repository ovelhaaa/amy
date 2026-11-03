"""Audit actual Blueprint target/header/translation units, without touching hardware."""
import argparse
import json
import re
from pathlib import Path


def require(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--musical", action="store_true")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    build = project / ".pio/build/guition-standalone-blueprint"
    defines = dict(re.findall(r"^#define (CONFIG_\w+) ([^\r\n]+)", (build / "config/sdkconfig.h").read_text(), re.M))
    for key in ("CONFIG_IDF_TARGET_ESP32P4", "CONFIG_SMKP4_STANDALONE", "CONFIG_SMKP4_UI_BLUEPRINT", "CONFIG_LV_FONT_MONTSERRAT_14"):
        require(defines.get(key) == "1", key)
    for key in ("CONFIG_SMK_LINK_ENABLED", "CONFIG_SMKP4_PCM_DIAGNOSTIC"):
        require(key not in defines, key + " must be disabled")
    for key in ("CONFIG_SMKP4_PCM_WIRING_CONFIRMED", "CONFIG_SMKP4_USB_WIRING_CONFIRMED"):
        require((defines.get(key) == "1") == args.musical, key + " gate")
    require(defines.get("CONFIG_LV_MEM_SIZE_KILOBYTES") == "128", "LVGL pool changed")
    entries = json.loads((build / "compile_commands.json").read_text())
    files = [str(item["file"]).replace("\\", "/") for item in entries]
    require(any(path.endswith("ui_blueprint_performance_adapter.cpp") for path in files), "missing live adapter")
    require(any(path.endswith("standalone_main.cpp") for path in files), "missing standalone boot")
    require(not any(path.endswith(("p4_link.cpp", "esp_preview_main.cpp")) for path in files), "wrong app sources")
    lvgl_roots = {path.split("/src/")[0] for path in files if "/lvgl__lvgl/src/" in path}
    require(len(lvgl_roots) == 1, "missing or duplicated LVGL source tree")
    amy = next(item for item in entries if str(item["file"]).replace("\\", "/").endswith("/src/amy.c"))
    for flag in ("AMY_SAMPLE_RATE=48000", "AMY_BLOCK_SIZE=256", "AMY_PATCH_CHECKED_RESOURCE_ADMISSION=1"):
        require(flag in amy["command"], flag)
    require((build / "firmware.bin").stat().st_size > 0, "missing firmware")
    print("PASS: effective Blueprint target, electrical gates, live adapter, single LVGL, pool128KB, AMY48000/256")
    print("MANUAL: compilation does not qualify physical audio, USB, touch, memory or deadlines")


if __name__ == "__main__":
    main()
