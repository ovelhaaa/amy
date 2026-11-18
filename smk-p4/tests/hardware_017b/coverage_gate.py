"""Measure periodic telemetry in firmware uptime, within a single capture."""
from datetime import datetime, timedelta
import re


# These families carry the automatic criteria and are emitted by the same
# diagnostics loop (kDiagnosticsIntervalMs = 5000). Event logs do not count.
PERIODIC = {
    "AUDIO": ("render_max_us", "stack_free_bytes", "write_errors", "timeouts", "render_deadlines"),
    "OUTPUT08": ("frames", "stack_free", "errors", "timeouts"),
    "ENGINE": ("oom",),
    "SWAP08": ("failures",),
    "PATCH_LOAD": ("last_us", "generation_before", "generation_after"),
    "BENCH_AUDIO": ("loads",),
    "SCENE": ("token", "generation", "phase", "effective", "applied", "error"),
    "BENCH_SCENE": ("token", "due_phase", "applied_phase", "units_per_frame"),
    "MEMORY": ("internal_min", "internal_largest", "psram_free", "psram_largest"),
    "NVS": ("used_entries",),
    "UI_BLUEPRINT": ("lvgl_free",),
    "USB": ("state", "connects", "disconnects", "errors"),
}
BINDING = ("session_id", "board_id", "profile", "firmware_sha256", "elf_sha256", "manifest_sha256")


def nonempty(value):
    return isinstance(value, str) and bool(value.strip())


def digest_string(value):
    return isinstance(value, str) and bool(re.fullmatch(r"[a-f0-9]{64}", value))


def capture_duration(meta):
    try:
        started, ended = (datetime.fromisoformat(meta[key]) for key in ("started_utc", "ended_utc"))
        if started.utcoffset() == ended.utcoffset() == timedelta(0) and ended >= started:
            return (ended - started).total_seconds()
    except (KeyError, TypeError, ValueError):
        pass
    return None


def summarize(records):
    families = {}
    for record in records:
        kind, fields = record["type"], record["fields"]
        if kind not in PERIODIC or not all(type(fields.get(key)) is int for key in PERIODIC[kind]):
            continue
        if kind == "NVS" and not nonempty(fields.get("status")):
            continue
        families.setdefault(kind, []).append({"ts_ms": record["ts_ms"], "line": record["line"]})

    audio = families.get("AUDIO", [])
    first = audio[0]["ts_ms"] if audio else None
    last = audio[-1]["ts_ms"] if audio else None
    complete = []
    coverage = {}
    for kind in PERIODIC:
        samples = families.get(kind, [])
        times = [sample["ts_ms"] for sample in samples]
        gaps = [b - a for a, b in zip(times, times[1:])]
        if times and audio:
            gaps.extend((max(0, times[0] - first), max(0, last - times[-1])))
        coverage[kind] = {"count": len(samples), "first_ts_ms": times[0] if times else None,
                          "last_ts_ms": times[-1] if times else None,
                          "max_gap_seconds": max(gaps, default=0) / 1000 if len(times) >= 2 else None,
                          "non_increasing_lines": [sample["line"] for a, b, sample in
                                                   zip(times, times[1:], samples[1:]) if b <= a]}

    # A diagnostics cycle starts at AUDIO and ends after USB. Stop tracking
    # the cycle there so unrelated output cannot fill its missing fields.
    valid_lines = {sample["line"] for samples in families.values() for sample in samples}
    cycle = {}
    for record in records:
        kind = record["type"]
        if kind == "AUDIO":
            cycle = {}
        if record["line"] in valid_lines and (kind == "AUDIO" or cycle):
            cycle[kind] = record
        if kind == "USB":
            if cycle.keys() == PERIODIC.keys():
                complete.append({"first_ts_ms": cycle["AUDIO"]["ts_ms"],
                                 "last_ts_ms": record["ts_ms"], "line": cycle["AUDIO"]["line"],
                                 "last_line": record["line"]})
            cycle = {}

    report_gaps = [(b["first_ts_ms"] - a["first_ts_ms"]) / 1000
                   for a, b in zip(complete, complete[1:])]
    return {"first_ts_ms": first, "last_ts_ms": last,
            "observed_seconds": (last - first) / 1000 if len(audio) >= 2 and last >= first else None,
            "periodic_snapshots": len(complete), "max_report_gap_seconds": max(report_gaps, default=None),
            "complete_reports": complete, "families": coverage}


def stress_markers(evidence, metrics, fixture_mode):
    markers = evidence.get("stress")
    if not isinstance(markers, dict):
        return None, "marcadores de início e fim do stress ausentes"
    if not evidence_binding(evidence, metrics, fixture_mode):
        return None, "marcadores sem vínculo com a sessão e o SHA-256 do log bruto"
    start = markers.get("start_ms")
    end = markers.get("end_ms")
    anchors = {report["first_ts_ms"]: report["line"] for report in metrics["coverage"]["complete_reports"]}
    if (type(start) is not int or type(end) is not int or start >= end
            or start not in anchors or end not in anchors):
        return None, "marcadores fora de ordem ou sem relatório periódico completo correspondente"
    return (start, end, anchors[start], anchors[end]), None


def evidence_binding(evidence, metrics, fixture_mode):
    expected = "TEST FIXTURE" if fixture_mode else "REAL HARDWARE SESSION"
    return (evidence.get("kind") == expected == metrics["provenance"]
            and evidence.get("raw_sha256") == metrics["raw_sha256"]
            and nonempty(evidence.get("reviewer"))
            and (fixture_mode or all(evidence.get(key) == metrics["metadata"].get(key) for key in BINDING)))
