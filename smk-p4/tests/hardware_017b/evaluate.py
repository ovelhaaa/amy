"""Apply versioned release criteria to a single primary boot and operator evidence."""
import argparse
import hashlib
import json
import math
import operator
from pathlib import Path

from analyze import analyze
from coverage_gate import BINDING, evidence_binding, stress_markers

COMPARE = {"eq": operator.eq, "le": operator.le, "ge": operator.ge}
BLOCK_FRAMES = 256
TEMPO_UNITS_PER_BPM_FRAME = 24 * 25


def automatic(metrics, criterion):
    kind, field = criterion.get("record"), criterion.get("field")
    items = [r for r in metrics["records"] if r["type"] == kind]
    values = [(r["line"], r["fields"].get(field)) for r in items]
    values = [(line, value) for line, value in values if type(value) is int]
    lines = [line for line, _ in values]
    method = criterion["method"]
    complete = len(values) == len(items) and len(values) >= 2
    if method == "alert":
        alerts = [a["line"] for a in metrics["alerts"] if a["type"] == field]
        return len(alerts), alerts, bool(metrics["records"])
    if method == "resets":
        if metrics["metadata"].get("role") == "persistence":
            return None, [], False
        boot_lines = [r["line"] for r in metrics["records"] if r["type"] == "BOOT"]
        return (max(len(metrics["resets"]), len(boot_lines) - 1),
                sorted(set(metrics["resets"] + boot_lines[1:])), bool(items))
    if method == "physical":
        modes = [r for r in metrics["records"] if r["type"] == "AUDIO" and r["fields"].get("mode")]
        failures = [r["line"] for r in modes if r["fields"]["mode"] in {"FAIL", "RAM_ONLY"}]
        if failures:
            return 0, failures, True
        advancing = complete and values[-1][1] > values[0][1]
        mode_evidence = len(modes) >= 2 and all(r["fields"]["mode"] == "I2S_PRODUCING" for r in modes)
        return (int(advancing) if values else None,
                sorted(lines + [r["line"] for r in modes]), complete and mode_evidence)
    if method == "nvs":
        observed = [r for r in items if "status" in r["fields"]]
        return sum(r["fields"]["status"] != "ESP_OK" for r in observed), [r["line"] for r in observed], len(observed) >= 2
    if method == "scene":
        terminal_keys = ("effective", "applied", "error", "token", "generation")
        failures = [r["line"] for r in items if r["fields"].get("phase") in (3, 5)
                    or (r["fields"].get("phase") == 2
                        and (r["fields"].get("error") != 0 or
                             r["fields"].get("effective") != r["fields"].get("applied")))]
        confirmed = [r for r in items if r["fields"].get("phase") == 2]
        valid = [r for r in confirmed if all(type(r["fields"].get(k)) is int for k in terminal_keys)
                 and r["fields"]["token"] > 0 and r["fields"]["effective"] > 0]
        return len(failures), failures or [r["line"] for r in valid], bool(valid) and len(valid) == len(confirmed)
    if method == "generation":
        pairs = [(r["line"], r["fields"]) for r in items if type(r["fields"].get("last_us")) is int and r["fields"]["last_us"] > 0]
        measured = [(line, f) for line, f in pairs if all(type(f.get(key)) is int for key in ("generation_before", "generation_after"))]
        failures = [line for line, f in measured
                    if f["generation_after"] != (f["generation_before"] + 1) % (2**32)]
        return len(failures), failures or [line for line, _ in measured], bool(measured) and len(measured) == len(pairs)
    if method == "next_bar":
        samples = []
        last_scene = None
        unit = criterion["bpm"] * TEMPO_UNITS_PER_BPM_FRAME
        for r in metrics["records"]:
            f = r["fields"]
            if r["type"] == "SCENE":
                last_scene = r
            if r["type"] != kind or last_scene is None:
                continue
            scene = last_scene["fields"]
            due, applied = f.get("due_phase"), f.get("applied_phase")
            if (0 <= r["ts_ms"] - last_scene["ts_ms"] <= 100 and scene.get("phase") == 2
                    and type(f.get("token")) is int and scene.get("token") == f["token"] and f.get("units_per_frame") == unit
                    and type(due) is int and due > 0 and type(applied) is int):
                # Starting before the boundary is always invalid.
                samples.append((r["line"], (applied - due) / unit if applied >= due else BLOCK_FRAMES + 1))
        return max((value for _, value in samples), default=None), [line for line, _ in samples], bool(samples)
    if not values:
        return None, [], False
    if method == "usb_delta":
        states = [r["fields"].get("state") for r in items]
        series = [value for _, value in values]
        valid = complete and states[0] == states[-1] == 2 and series == sorted(series)
        return series[-1] - series[0], lines, valid
    if method == "min":
        return min(v for _, v in values), lines, complete
    if method == "max":
        series = [v for _, v in values]
        monotonic = not criterion.get("cumulative") or series == sorted(series)
        return max(series), lines, complete and monotonic
    raise ValueError(f"unknown method {method}")


def manual(criterion, evidence, root, metrics, fixture_mode):
    item = evidence.get("observations", {}).get(criterion["id"], {})
    value = item.get("value")
    if type(value) not in (float, int) or not math.isfinite(value):
        return None, [], False
    file = item.get("file")
    if not isinstance(file, str) or not str(item.get("note", "")).strip():
        return value, [], False
    instrument = item.get("instrument", "")
    if criterion.get("instrument_required") and (not isinstance(instrument, str) or not instrument.strip()):
        return value, [], False
    data = (root / file).resolve()
    valid = (evidence_binding(evidence, metrics, fixture_mode)
             and data.is_relative_to(root.resolve())
             and data.is_file()
             and data.stat().st_size > 0)
    if not valid:
        return value, [], False
    digest = hashlib.sha256()
    with data.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    checksum = digest.hexdigest()
    if not fixture_mode and item.get("sha256") != checksum:
        return value, [], False
    if not fixture_mode and criterion.get("instrument_required"):
        allowed = ({".csv", ".vcd", ".sr", ".sal"} if criterion["id"] == "physical_clock_interruptions"
                   else {".wav", ".flac"})
        if data.suffix.lower() not in allowed:
            return value, [], False
    return value, [{"file": file, "sha256": checksum,
                    "note": item["note"], "reviewer": evidence["reviewer"],
                    "instrument": instrument}], True


def temporal(metrics, gap_limit, evidence, fixture_mode):
    coverage = metrics.get("coverage", {})
    reports = coverage.get("complete_reports", [])
    families = coverage.get("families", {})
    lines = [r["line"] for r in reports]
    duration = coverage.get("observed_seconds")
    elapsed = coverage.get("capture_seconds")
    missing = [kind for kind, row in families.items() if row["max_gap_seconds"] is None]
    gaps = {kind: row["max_gap_seconds"] for kind, row in families.items()
            if row["max_gap_seconds"] is not None}
    if coverage.get("max_report_gap_seconds") is not None:
        gaps["relatórios completos"] = coverage["max_report_gap_seconds"]
    gap = max(gaps.values(), default=None)
    gap_reason = "; ".join(filter(None, [
        "famílias com menos de duas amostras: " + ", ".join(missing) if missing else None,
        f"maior intervalo em {', '.join(k for k, v in gaps.items() if v == gap)}" if gaps else None,
    ]))
    invalid_times = sorted(set(coverage.get("untimed_lines", []) + metrics["resets"]
                               + [line for row in families.values() for line in row["non_increasing_lines"]]))
    meta = metrics["metadata"]
    interruptions = []
    if meta.get("capture_exit") not in (None, 0):
        interruptions.append(f"capturador retornou {meta['capture_exit']}")
    if elapsed is not None and meta.get("role") == "primary" and duration is not None:
        if (type(meta.get("requested_seconds")) is int
                and elapsed + gap_limit < meta["requested_seconds"]):
            interruptions.append("capturador terminou antes da janela solicitada")
        if elapsed - duration > 2 * gap_limit or duration - elapsed > gap_limit:
            interruptions.append("janela observada diverge do tempo de captura no host")
    if invalid_times:
        interruptions.append("timestamps ausentes, repetidos ou regressivos")
    if meta.get("role") == "persistence":
        interruptions.append("sessão complementar de persistência")
    window_valid = duration is not None and not invalid_times and meta.get("role") != "persistence"
    markers, marker_reason = stress_markers(evidence, metrics, fixture_mode)
    before, after = [], []
    if markers:
        start, end, start_line, end_line = markers
        before = [r for r in reports if r["first_ts_ms"] <= start and r["line"] <= start_line]
        after = [r for r in reports if r["first_ts_ms"] > end and r["line"] > end_line]
    return {
        "MINIMUM_SESSION_DURATION": (duration, lines, window_valid, "janela AUDIO válida insuficiente"),
        "MINIMUM_PERIODIC_SNAPSHOTS": (len(reports), lines, window_valid, "relatórios completos insuficientes"),
        "MAXIMUM_TELEMETRY_GAP": (gap, lines, window_valid and not missing and len(reports) >= 2, gap_reason),
        "PRE_STRESS_BASELINE": (len(before) if markers else None,
                                [r["line"] for r in before], window_valid and bool(markers),
                                marker_reason or "referência anterior ao stress insuficiente"),
        "POST_STRESS_RECOVERY": (len(after) if markers else None,
                                 [r["line"] for r in after], window_valid and bool(markers),
                                 marker_reason or "recuperação posterior ao stress insuficiente"),
        "capture_continuity": (int(not interruptions) if duration is not None else None,
                               invalid_times, window_valid and not interruptions and
                               (elapsed is not None or metrics["provenance"] == "TEST FIXTURE"),
                               "; ".join(interruptions) or "janela de captura ausente ou insuficiente"),
    }


def evaluate(metrics, config, evidence=None, root=Path("."), fixture_mode=False):
    if metrics["schema"] != 1 or config["schema"] != 1:
        raise ValueError("unsupported schema")
    evidence = evidence or {}
    temporal_rules = {r["id"]: r for r in config["criteria"] if r["method"] == "temporal" and r["required"]}
    if temporal_rules.keys() != {"MINIMUM_SESSION_DURATION", "MINIMUM_PERIODIC_SNAPSHOTS",
                                 "MAXIMUM_TELEMETRY_GAP", "PRE_STRESS_BASELINE",
                                 "POST_STRESS_RECOVERY", "capture_continuity"}:
        raise ValueError("required session coverage criteria are missing or unsupported")
    timing = temporal(metrics, temporal_rules["MAXIMUM_TELEMETRY_GAP"]["limit"], evidence, fixture_mode)
    observed = all(sufficient and COMPARE[temporal_rules[key]["op"]](value, temporal_rules[key]["limit"])
                   for key, (value, _, sufficient, _) in timing.items())
    results = []
    for rule in config["criteria"]:
        reason = None
        if rule["method"] == "temporal":
            value, refs, sufficient, reason = timing[rule["id"]]
        elif rule["method"] == "manual":
            value, refs, sufficient = manual(rule, evidence, root, metrics, fixture_mode)
        else:
            value, refs, sufficient = automatic(metrics, rule)
        meets = value is not None and COMPARE[rule["op"]](value, rule["limit"])
        # Minima sampled as lower bounds (e.g. number of USB cycles) require
        # complete coverage before an insufficient count can be called a failure.
        decisive = sufficient or (
            rule["method"] in {"alert", "resets", "scene", "generation", "nvs"}
            or (rule["method"], rule["op"]) in {("max", "le"), ("max", "eq"), ("min", "ge")}
        )
        status = ("NOT MEASURED" if value is None else
                  "FAIL" if decisive and not meets else
                  "PASS" if sufficient and meets else "INCONCLUSIVE")
        if status == "PASS" and rule["method"] not in {"temporal", "manual"} and not observed:
            status, reason = "INCONCLUSIVE", "cobertura temporal insuficiente para qualificar a sessão"
        results.append({"id": rule["id"], "status": status, "required": rule["required"],
                        "group": rule["group"], "value": value, "unit": rule["unit"],
                        "operator": rule["op"], "limit": rule["limit"], "evidence": refs,
                        "rationale": rule["rationale"],
                        "reason": reason if reason and status != "PASS" else {"NOT MEASURED": "sem observação",
                                   "INCONCLUSIVE": "cobertura ou vínculo da evidência insuficiente",
                                   "FAIL": "limite violado", "PASS": "limite atendido"}[status]})
    provenance = metrics["provenance"]
    eligible = ((provenance == "REAL HARDWARE SESSION" and not fixture_mode
                 and metrics["metadata"].get("role") == "primary")
                or (fixture_mode and provenance == "TEST FIXTURE"))
    required = [r for r in results if r["required"]]
    status = ("FAIL" if any(r["status"] == "FAIL" for r in required) else
              "PASS" if eligible and required and all(r["status"] == "PASS" for r in required) else "INCONCLUSIVE")
    return {"schema": 1, "criteria_version": config["version"], "status": status,
            "raw_sha256": metrics["raw_sha256"], "raw_file": metrics["raw_file"],
            "identity": {key: metrics["metadata"].get(key) for key in (*BINDING, "commit", "role")},
            "identity_issues": metrics["identity_issues"], "coverage": metrics["coverage"],
            "provenance": provenance, "release": "GO" if status == "PASS" and provenance == "REAL HARDWARE SESSION"
            else "NO-GO", "results": results}


def write_result(result, out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "gate.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [f"# Release gate — {result['provenance']}",
             f"\n**{result['status']} / {result['release']}**\n",
             f"- Log `{result['raw_file']}`, SHA-256 `{result['raw_sha256']}`",
             *[f"- {key} `{value}`" for key, value in result["identity"].items() if value is not None],
             *[f"- {issue}" for issue in result["identity_issues"]], "",
             "| Critério | Status | Observado | Limite | Evidência | Motivo |",
             "| --- | --- | ---: | --- | --- | --- |"]
    for r in result["results"]:
        refs = r["evidence"]
        ref = ", ".join(item["file"] if isinstance(item, dict) else f"L{item}" for item in refs[:8]) + (f" (+{len(refs)-8}, ver JSON)" if len(refs) > 8 else "")
        ref = ref.replace("|", r"\|")
        value = r["value"] if r["value"] is not None else "NA"
        lines.append(f"| {r['id']} | {r['status']} | {value} | "
                     f"{r['operator']} {r['limit']} {r['unit']} | {ref} | {r['reason']} |")
    (out / "release_gate.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("metrics", type=Path)
    parser.add_argument("--criteria", type=Path, default=Path(__file__).with_name("criteria.json"))
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--raw", type=Path)
    parser.add_argument("--metadata", type=Path)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--test-fixture", action="store_true")
    args = parser.parse_args()
    docs = [json.loads(path.read_text(encoding="utf-8-sig"))
            for path in (args.metrics, args.criteria)]
    evidence = json.loads(args.evidence.read_text(encoding="utf-8-sig")) if args.evidence else {}
    destinations = {(args.out / name).resolve() for name in ("gate.json", "release_gate.md")}
    raw_input = args.raw or args.metrics.parent / docs[0].get("raw_file", "serial.log")
    if any(path and path.resolve() in destinations for path in
           (raw_input, args.metadata, args.metrics, args.criteria, args.evidence)):
        parser.error("gate outputs must not replace inputs")
    if args.raw:
        current = analyze(args.raw, None, args.metadata, args.test_fixture, args.bundle)
        if docs[0] != current and current["provenance"] == "REAL HARDWARE SESSION":
            current["provenance"] = "UNVERIFIED"
            current["identity_issues"].append("metrics.json diverge da reanálise do log bruto")
        docs[0] = current
    elif docs[0]["provenance"] == "REAL HARDWARE SESSION":
        docs[0]["provenance"] = "UNVERIFIED"
        docs[0]["identity_issues"].append("reavaliação requer --raw, --metadata e --bundle")
    result = evaluate(*docs, evidence, args.evidence.parent if args.evidence else args.out, args.test_fixture)
    write_result(result, args.out)
    print(f"{result['provenance']}: {result['status']} / {result['release']}")
