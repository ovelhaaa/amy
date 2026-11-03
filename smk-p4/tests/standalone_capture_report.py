"""Summarize complete P4 serial records; this never grants analog/UI approval."""
import argparse
import json
import re
from pathlib import Path

REQUIRED = {
    "AUDIO": "frames_written blocks_rendered render_avg_us render_max_us render_deadlines service_max_us refill_risks write_errors timeouts stack_free_bytes error".split(),
    "ENGINE": "ready patch notes clipped oom overflow expired latency_max_ms compute_deadlines cmd_avg/max dsp_avg/max dma_avg/max".split(),
    "USB": "state connects disconnects errors events parse_errors ignored_cable_packets dropped overflow stack_free polyphony_rejected".split(),
    "UI_BLUEPRINT": "stack_free_bytes screen_switches".split(),
}
COUNTERS = {
    "AUDIO": "frames_written blocks_rendered render_deadlines refill_risks write_errors timeouts".split(),
    "ENGINE": "clipped oom overflow expired compute_deadlines latency_max_ms".split(),
    "USB": "connects disconnects errors events parse_errors ignored_cable_packets dropped overflow polyphony_rejected".split(),
    "UI_BLUEPRINT": ["screen_switches"],
}
ZERO = {"AUDIO": "render_deadlines refill_risks write_errors timeouts".split(),
        "ENGINE": "clipped oom overflow compute_deadlines".split(),
        "USB": "errors parse_errors dropped overflow".split(),
        "DISPLAY": ["touch_errors", "flush_errors"]}


def report(text, min_seconds=1800, hotplugs=4, blueprint=True):
    rows = {k: [] for k in (*REQUIRED, "LVGL", "HEAP", "DISPLAY")}
    malformed = boots_after_start = 0
    for line in text.splitlines(keepends=True):
        if not line.endswith("\n"):
            continue  # The capture can stop during a serial write.
        line = re.sub(r"\x1b\[[0-9;]*m", "", line)
        if rows["AUDIO"] and ("ESP-ROM:" in line or "rst:" in line):
            boots_after_start += 1
        match = re.search(r"I \((\d+)\) P4_AMY: (AUDIO|ENGINE|USB|UI_BLUEPRINT) (.*)", line)
        if match:
            t, kind, rest = int(match[1]), match[2], match[3]
            fields = dict(re.findall(r"(\w+(?:/\w+)?)=([^\s]+)", rest))
            if not all(key in fields for key in REQUIRED[kind]):
                malformed += 1
                continue
            for key, value in list(fields.items()):
                if value.isdecimal():
                    fields[key] = int(value)
            if any(not isinstance(fields[key], int) for key in REQUIRED[kind] if key not in ("error", "cmd_avg/max", "dsp_avg/max", "dma_avg/max")):
                malformed += 1
                continue
            if kind == "ENGINE" and any(not re.fullmatch(r"\d+/\d+", fields[key]) for key in ("cmd_avg/max", "dsp_avg/max", "dma_avg/max")):
                malformed += 1
                continue
            fields["t_ms"] = t
            if kind == "AUDIO":
                fields["mode"] = rest.split()[0]
            rows[kind].append(fields)
        display_record = re.search(r"I \((\d+)\) DISPLAY_TEST: (.*)", line)
        display_text = display_record[2] if display_record else ""
        display_time = int(display_record[1]) if display_record else 0
        lvgl = re.search(r"LVGL free=(\d+) largest=(\d+) peak_used=(\d+) fragmentation=(\d+)%", display_text)
        if lvgl:
            rows["LVGL"].append(dict(zip(("free", "largest", "peak_used", "fragmentation"), map(int, lvgl.groups())), t_ms=display_time))
        heap = re.search(r"I \((\d+)\) SMK_P4_GATE0: heap internal free=(\d+) min=(\d+) largest=(\d+); PSRAM total=(\d+) free=(\d+) min=(\d+) largest=(\d+)", line)
        if heap:
            rows["HEAP"].append(dict(zip(("internal_free", "internal_min", "internal_largest", "psram_total", "psram_free", "psram_min", "psram_largest"), map(int, heap.groups()[1:])), t_ms=int(heap[1])))
        display = re.search(r"diag_buttons=\d+ touch_errors=(\d+) flush_errors=(\d+) refresh=(\d+)", display_text)
        if display:
            rows["DISPLAY"].append(dict(zip(("touch_errors", "flush_errors", "refresh"), map(int, display.groups())), t_ms=display_time))

    needed = ["AUDIO", "ENGINE", "USB", "LVGL", "HEAP", "DISPLAY"] + (["UI_BLUEPRINT"] if blueprint else [])
    checks = {"complete_telemetry": all(len(rows[k]) >= 2 for k in needed),
              "no_malformed_records": malformed == 0, "no_observed_reboot": boots_after_start == 0}
    gaps, spans = {}, {}
    for kind in needed:
        records = rows[kind]
        gaps[kind] = max((b["t_ms"]-a["t_ms"] for a, b in zip(records, records[1:])), default=0)/1000
        spans[kind] = (records[-1]["t_ms"]-records[0]["t_ms"])/1000 if len(records) >= 2 else 0
        checks[kind+"_clock_monotonic"] = all(b["t_ms"] >= a["t_ms"] for a, b in zip(records, records[1:]))
        checks[kind+"_span"] = spans[kind] >= min_seconds
        checks[kind+"_gap"] = gaps[kind] <= 10
    for kind, keys in COUNTERS.items():
        if kind == "UI_BLUEPRINT" and not blueprint:
            continue
        checks[kind+"_counters_monotonic"] = all(all(b[key] >= a[key] for a, b in zip(rows[kind], rows[kind][1:])) for key in keys)
    for kind, keys in ZERO.items():
        checks[kind+"_errors_zero"] = bool(rows[kind]) and all(r[key] == 0 for r in rows[kind] for key in keys)
    delta = lambda kind, key: rows[kind][-1][key]-rows[kind][0][key] if len(rows[kind]) >= 2 else 0
    checks["physical_audio"] = bool(rows["AUDIO"]) and all(r["mode"] == "I2S_PRODUCING" and r["error"] == "ESP_OK" for r in rows["AUDIO"])
    checks["engine_ready_four_voices"] = bool(rows["ENGINE"]) and all(r["ready"] == 1 and r["notes"] <= 4 and r["patch"] in (128,135) for r in rows["ENGINE"])
    checks["new_midi_events"] = delta("USB", "events") > 0
    checks["hotplugs_in_window"] = delta("USB", "disconnects") >= hotplugs and delta("USB", "connects") >= hotplugs
    # Host::State::Connected == 2 in usb_midi_host.h; engine readiness is separate.
    checks["host_connected_at_end"] = bool(rows["USB"]) and rows["USB"][-1]["state"] == 2
    if blueprint:
        checks["ui_transitions_observed"] = delta("UI_BLUEPRINT", "screen_switches") >= 28
    frames = delta("AUDIO", "frames_written")
    rate = frames/spans["AUDIO"] if spans.get("AUDIO", 0) > 0 else None
    checks["frames_coherent_with_48k"] = rate is not None and 47520 <= rate <= 48480
    checks["render_within_budget"] = bool(rows["AUDIO"]) and all(r["render_max_us"] <= 5333 for r in rows["AUDIO"])
    def ranges(records):
        keys = {k for row in records for k,v in row.items() if isinstance(v,int) and k != "t_ms"}
        result = {}
        for key in sorted(keys):
            values = [r[key] for r in records if isinstance(r.get(key), int)]
            result[key] = {"min": min(values), "max": max(values), "last": values[-1]}
        return result
    return {"samples": {k: len(v) for k,v in rows.items()}, "span_seconds": spans, "max_gap_seconds": gaps,
            "last": {k: v[-1] if v else None for k,v in rows.items()}, "ranges": {k:ranges(v) for k,v in rows.items()},
            "midi_event_delta": delta("USB","events"), "disconnect_delta": delta("USB","disconnects"),
            "ui_transitions_delta": delta("UI_BLUEPRINT","screen_switches"), "frames_per_second_by_count":rate,
            "malformed":malformed, "checks":checks, "serial_checks_pass":all(checks.values()),
            "limits":["Serial samples do not grant Gate E or analog, touch, screen coverage or memory-stability approval.",
                      "Counters/maxima are cumulative. Compare equivalent screens and operator actions separately.",
                      "DMA underruns, CPU total and physical key-to-output latency remain unmeasured."]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log",type=Path)
    parser.add_argument("--output",required=True,type=Path)
    parser.add_argument("--min-seconds",type=float,default=1800)
    parser.add_argument("--hotplugs",type=int,default=4)
    parser.add_argument("--legacy",action="store_true")
    args=parser.parse_args()
    if args.min_seconds < 0 or args.hotplugs < 0:
        parser.error("thresholds must be nonnegative")
    result=report(args.log.read_text(encoding="utf-8",errors="replace"),args.min_seconds,args.hotplugs,not args.legacy)
    args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("PASS" if result["serial_checks_pass"] else "FAIL", "serial checks; physical/operator approval is separate")
    if not result["serial_checks_pass"]:
        print("Failed:", ", ".join(k for k,v in result["checks"].items() if not v))
        raise SystemExit(1)


if __name__ == "__main__":
    main()
