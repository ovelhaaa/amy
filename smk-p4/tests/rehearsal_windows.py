"""Collect audio_status windows without reset/flash, or compare a saved raw log.

Uses only the existing pyserial dependency when collecting. Counters are never
reset. --probe explicitly enables phase timing for this collection only.
"""
import argparse
import json
from pathlib import Path
import re
import time


COUNTERS = (
    "frames", "starvation", "render_deadline_misses", "i2s_write_errors",
    "i2s_timeouts", "cmd_drop", "midi_events", "probe_blocks", "probe_service_us",
    "patch_count", "pcm_discarded", "gap_startup", "gap_barrier", "gap_empty",
)


def snapshots(raw):
    current = None
    raw = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", raw)
    for line in raw.splitlines():
        if "=== AUDIO QUAL ===" in line:
            current = {}
        elif "=== AUDIO QUAL END ===" in line:
            if current is not None:
                yield current
            current = None
        elif current is not None:
            for key, value in re.findall(r"\b([a-z][a-z0-9_]*)=([^\s]+)", line):
                current[key] = int(value) if value.isdecimal() else value


def compare(before, after):
    # Maxima/EWMA are observations, never subtract maxima and call it a window max.
    identity = ("boot_id", "elf_sha256", "block", "rate", "dma_buffers", "dma_frames", "pcm_blocks", "probe")
    for key in identity:
        if key not in before or key not in after or before[key] != after[key]:
            raise ValueError(f"cannot compare changed/missing {key}")
    elapsed = after["uptime_us"] - before["uptime_us"]
    if not 0 < elapsed <= 60_000_000:
        raise ValueError("window must be increasing and <=60 seconds; reboot/reset/gap suspected")
    delta = {}
    for key in COUNTERS:
        a, b = before[key], after[key]
        if b < a and a < 0xF0000000:
            raise ValueError(f"counter reset suspected: {key}")
        delta[key] = (b - a) & 0xFFFFFFFF
    if delta["frames"] > elapsed * after["rate"] / 1_000_000 + 4 * after["block"]:
        raise ValueError("implausible frame delta; reset or inconsistent observation")
    mean = None
    if before["probe"] and delta["probe_blocks"]:
        mean = delta["probe_service_us"] / delta["probe_blocks"]
    return {"seconds": elapsed / 1_000_000, "delta": delta,
            "probe_service_mean_us": mean, "before": before, "after": after}


def report(raw, scenario, notes):
    samples = list(snapshots(raw))
    if len(samples) < 2:
        raise ValueError("need two complete snapshots from instrumented firmware; legacy/truncated logs are not comparable")
    windows = []
    for a, b in zip(samples, samples[1:]):
        try:
            windows.append(compare(a, b))
        except (ValueError, KeyError) as error:
            windows.append({"status": "BLOCKED", "reason": str(error)})
    return {"scenario": scenario, "operator_notes": notes,
            "auditory_confirmation": "MANUAL", "snapshots": samples, "windows": windows}


def collect(args):
    import serial
    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, 115200, 0.1
    port.dtr = port.rts = False
    port.open()  # Configure inactive modem lines before opening; no reset pulse.
    with port, args.output.open("wb") as output:
        deadline, next_query = time.monotonic() + args.seconds, 0.0
        try:
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_query:
                    command = b"audio_status probe_on\n" if args.probe else b"audio_status\n"
                    port.write(command)
                    next_query = now + args.interval
                chunk = port.read(max(1, port.in_waiting))
                if chunk:
                    output.write(chunk)
                    output.flush()
        finally:
            if args.probe:
                port.write(b"audio_status probe_off\n")  # Preserve all accumulated counters.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--port")
    mode.add_argument("--input", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="raw log for collection; JSON for --input")
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--interval", type=float, default=10)
    parser.add_argument("--probe", action="store_true")
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--operator-notes", default="NOT RUN: no operator observations supplied")
    args = parser.parse_args()
    if not 3 <= args.interval <= 30 or not 2 * args.interval + 2 <= args.seconds <= 7200:
        parser.error("interval must be 3..30s and duration 2*interval+2..7200s")
    if args.input and args.probe:
        parser.error("--probe requires --port")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.input:
        raw, target = args.input.read_text(encoding="utf-8", errors="replace"), args.output
    else:
        collect(args)
        raw, target = args.output.read_text(encoding="utf-8", errors="replace"), args.output.with_suffix(".json")
    try:
        result = report(raw, args.scenario, args.operator_notes)
    except (ValueError, KeyError) as error:
        parser.exit(1, f"BLOCKED: {error}; raw capture retained\n")
    target.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Saved {len(result['snapshots'])} observations to {target}; auditory confirmation remains MANUAL")
    if any(window.get("status") == "BLOCKED" for window in result["windows"]):
        raise SystemExit("BLOCKED: incompatible windows retained separately; inspect JSON")


if __name__ == "__main__":
    main()
