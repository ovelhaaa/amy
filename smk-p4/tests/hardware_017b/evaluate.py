"""Apply versioned release criteria to a single primary boot and operator evidence."""
import argparse
import hashlib
import json
import math
import operator
from pathlib import Path

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
        return max(len(metrics["resets"]), metrics["boots"] - 1), metrics["resets"], bool(items)
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
    kind = "TEST FIXTURE" if fixture_mode else "REAL HARDWARE SESSION"
    valid = (evidence.get("kind") == kind and evidence.get("raw_sha256") == metrics["raw_sha256"]
             and str(evidence.get("reviewer", "")).strip() and data.is_relative_to(root.resolve())
             and data.is_file()
             and data.stat().st_size > 0)
    if not valid:
        return value, [], False
    digest = hashlib.sha256()
    with data.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return value, [{"file": file, "sha256": digest.hexdigest(),
                    "note": item["note"], "reviewer": evidence["reviewer"],
                    "instrument": instrument}], True


def evaluate(metrics, config, evidence=None, root=Path("."), fixture_mode=False):
    if metrics["schema"] != 1 or config["schema"] != 1:
        raise ValueError("unsupported schema")
    evidence = evidence or {}
    results = []
    for rule in config["criteria"]:
        is_manual = rule["method"] == "manual"
        value, refs, sufficient = (manual(rule, evidence, root, metrics, fixture_mode) if is_manual
                                   else automatic(metrics, rule))
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
        results.append({"id": rule["id"], "status": status, "required": rule["required"],
                        "group": rule["group"], "value": value, "unit": rule["unit"],
                        "operator": rule["op"], "limit": rule["limit"], "evidence": refs,
                        "rationale": rule["rationale"],
                        "reason": {"NOT MEASURED": "sem observação",
                                   "INCONCLUSIVE": "cobertura ou vínculo da evidência insuficiente",
                                   "FAIL": "limite violado", "PASS": "limite atendido"}[status]})
    provenance = metrics["provenance"]
    eligible = provenance == "REAL HARDWARE SESSION" or (fixture_mode and provenance == "TEST FIXTURE")
    required = [r for r in results if r["required"]]
    status = ("FAIL" if any(r["status"] == "FAIL" for r in required) else
              "PASS" if eligible and required and all(r["status"] == "PASS" for r in required) else "INCONCLUSIVE")
    return {"schema": 1, "criteria_version": config["version"], "status": status,
            "provenance": provenance, "release": "GO" if status == "PASS" and provenance == "REAL HARDWARE SESSION"
            else "NO-GO", "results": results}


def write_result(result, out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "gate.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [f"# Release gate — {result['provenance']}",
             f"\n**{result['status']} / {result['release']}**\n",
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
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--test-fixture", action="store_true")
    args = parser.parse_args()
    docs = [json.loads(path.read_text(encoding="utf-8-sig"))
            for path in (args.metrics, args.criteria)]
    evidence = json.loads(args.evidence.read_text(encoding="utf-8-sig")) if args.evidence else {}
    result = evaluate(*docs, evidence, args.evidence.parent if args.evidence else args.out, args.test_fixture)
    write_result(result, args.out)
    print(f"{result['provenance']}: {result['status']} / {result['release']}")
