"""Read unmodified ESP-IDF serial output and retain line-level evidence."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re

from coverage_gate import PERIODIC, capture_duration, digest_string, nonempty, summarize
from firmware import sha as file_sha, validate

LOG = re.compile(r"^[IWEVD] \(\s*(\d+)\) ([\w.-]+): (.*)$")
UNTIMED = re.compile(r"^[IWEVD]\s+(?:\([^)]*\)\s+)?(P4_AMY|SMK_P4_GATE0):\s+(\S+)")
FIELDS = re.compile(r"([\w/]+)=([^\s;]+)")
TAGS = set(("AUDIO ENGINE ADMISSION OUTPUT08 GAP09 PATCH_LOAD SWAP08 PATCH08 "
            "SCENE BENCH_AUDIO BENCH_SCENE BENCH_ID USB NVS UI_BLUEPRINT "
            "MIDI_TRACE ARP015 SEQ016C TEMPO0151").split())
FATAL = re.compile(
    r"Guru Meditation|(?:task|interrupt) watchdog.*triggered|stack canary watchpoint|"
    r"assert failed|abort\(\) was called|CORRUPT HEAP|Brownout detector", re.I
)
COVERAGE = {
    "AUDIO": ("render_max_us", "stack_free_bytes", "write_errors", "timeouts"),
    "OUTPUT08": ("frames", "stack_free", "errors", "timeouts"),
    "BENCH_SCENE": ("token", "due_phase", "applied_phase"),
    "USB": ("connects", "disconnects", "errors"),
}
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


def parse_fields(body, kind):
    fields = {}
    for name, raw in FIELDS.findall(body):
        base = 16 if ((kind == "SCENE" and name in {"effective", "applied"})
                      or (kind == "USB_DRIVER" and name in {"VID", "PID", "IN"})) else 10
        if "/" in name and raw.count("/") == 1:
            first, _ = name.split("/")
            if first.endswith("_avg"):
                stem = first[:-4]
                keys = (stem + "_avg_us", stem + "_max_us")
            else:
                keys = (first, first + "_largest")
            if all(re.fullmatch(r"[0-9]+", value) for value in raw.split("/")):
                fields.update(zip(keys, map(int, raw.split("/"))))
        try:
            fields[name] = int(raw, base)
        except ValueError:
            fields[name] = raw
    return fields


def analyze(raw_path, out, metadata=None, fixture=False, bundle=None):
    destinations = {(out / name).resolve()
                    for name in ("metrics.json", "summary.csv", "warnings.txt", "session_report.md")} if out else set()
    if raw_path.resolve() in destinations or (metadata and metadata.resolve() in destinations):
        raise ValueError("analysis outputs must not replace the raw log or its metadata")
    raw = raw_path.read_bytes()
    records, alerts, unknown = [], [], 0
    last_ts, boots = None, 0
    resets, untimed = [], []
    for number, line in enumerate(raw.decode("utf-8", errors="replace").splitlines(), 1):
        line = ANSI.sub("", line)
        if FATAL.search(line):
            alerts.append({"line": number, "message": line, "type": "fatal"})
        match = LOG.match(line)
        if not match:
            candidate = UNTIMED.match(line)
            if candidate and candidate[2] in {*PERIODIC, "heap", "BENCH_ID"}:
                untimed.append(number)
            if line.strip():
                unknown += 1
            continue
        timestamp, source, body = match.groups()
        timestamp = int(timestamp)
        if last_ts is not None and timestamp + 1000 < last_ts:
            resets.append(number)
        last_ts = timestamp
        kind = body.split(maxsplit=1)[0] if body.strip() else ""
        if source == "SMK_P4_GATE0" and body.startswith("IDF="):
            kind = "BOOT"
            boots += 1
        elif source == "SMK_P4_GATE0" and body.startswith("heap internal "):
            kind = "MEMORY"
        elif source == "P4_AMY" and body.startswith("PCM="):
            kind = "PCM"
        elif source in {"P4_USB", "P4_USB_DESC"}:
            kind = "USB_DRIVER"
        elif kind not in TAGS or source != "P4_AMY":
            if line.startswith("E") or "i2s" in source.lower():
                alerts.append({"line": number, "message": line,
                               "type": "i2s" if line.startswith("E") and "i2s" in source.lower() else "error"})
            else:
                unknown += 1
            continue
        fields = parse_fields(body, kind)
        if kind == "AUDIO":
            fields["mode"] = body.split()[1] if len(body.split()) > 1 else None
        if kind == "USB_DRIVER":
            fields["message"] = body
        if kind == "BENCH_SCENE":
            request = fields.get("request_ms")
            start = fields.get("start_ms")
            if all(type(v) is int for v in (request, start)) and fields.get("units_per_frame"):
                fields["owner_wait_ms"] = (start - request) % (2**32)
        if kind == "MEMORY":
            fields = {f"{prefix}_{key}": value
                      for prefix, part in zip(("internal", "psram"), body.split(";"))
                      for key, value in parse_fields(part, kind).items()}
        records.append({"type": kind, "ts_ms": timestamp, "line": number, "fields": fields})
    sha = hashlib.sha256(raw).hexdigest()
    identity_issues = []
    meta = {}
    if metadata:
        try:
            meta = json.loads(metadata.read_text(encoding="utf-8-sig"))
            if not isinstance(meta, dict):
                raise ValueError("o documento precisa ser um objeto JSON")
        except (OSError, ValueError) as error:
            identity_issues.append(f"metadados de captura ilegíveis: {error}")
            meta = {}
    identities = [r["fields"] for r in records if r["type"] == "BENCH_ID"]
    identities_match = bool(identities) and all(
        f.get("elf_sha256") == meta.get("elf_sha256") and f.get("schema") == 1
        and f.get("profile") == meta.get("profile") and f.get("version") == "0.17B"
        for f in identities
    )
    if not fixture and not identities_match:
        identity_issues.append("identidade ELF, versão ou perfil no console ausente ou divergente")
    if not fixture and (meta.get("raw_sha256") != sha or type(meta.get("bytes")) is not int
                        or meta["bytes"] != len(raw)):
        identity_issues.append("SHA-256 ou tamanho do log bruto diverge dos metadados")
    elapsed = capture_duration(meta)
    if not fixture and not (
            all(nonempty(meta.get(key)) for key in ("session_id", "board_id", "port"))
            and re.fullmatch(r"COM[1-9][0-9]*", meta["port"])
            and type(meta.get("baud")) is int and 1 <= meta["baud"] <= 4000000
            and type(meta.get("requested_seconds")) is int and 1 <= meta["requested_seconds"] <= 3600
            and meta.get("role") in {"primary", "persistence"}
            and meta.get("reset_requested") is False and elapsed is not None
            and type(meta.get("capture_exit")) is int and meta["capture_exit"] == 0):
        identity_issues.append("metadados de captura ausentes, inválidos ou coleta interrompida")
    if not fixture:
        try:
            if not bundle:
                raise ValueError("bundle ausente")
            manifest = validate(bundle, "guition-editor-8-bench")
            if (not digest_string(meta.get("manifest_sha256"))
                    or meta["manifest_sha256"] != file_sha(bundle / "manifest.json")
                    or any(meta.get(key) != manifest[key] for key in
                           ("profile", "firmware_sha256", "elf_sha256", "commit"))):
                raise ValueError("identidade divergente dos metadados")
        except (OSError, ValueError, KeyError, TypeError, StopIteration) as error:
            identity_issues.append(f"bundle não verificado: {error}")
    if fixture:
        provenance = "TEST FIXTURE"
    elif meta.get("kind") == "HOST TEST":
        provenance = "HOST TEST"
    elif meta.get("kind") == "TEST FIXTURE":
        provenance = "TEST FIXTURE"
    elif meta.get("kind") == "REAL HARDWARE SESSION" and not identity_issues:
        provenance = "REAL HARDWARE SESSION"
    else:
        provenance = "UNVERIFIED"
    warnings = [f"Linha {item['line']}: {item['message']}" for item in alerts]
    present = {item["type"] for item in records}
    for kind in sorted({"BOOT", "AUDIO", "OUTPUT08", "ENGINE", "PATCH_LOAD", "MEMORY",
                        "USB", "SCENE", "BENCH_SCENE", "NVS", "UI_BLUEPRINT"} - present):
        warnings.append(f"{kind} NOT MEASURED")
    warnings.extend(identity_issues)
    warnings.extend(f"Linha {line}: telemetria sem timestamp válido" for line in untimed)
    warnings.extend(f"Linha {line}: timestamp regrediu" for line in resets)
    if any(r["type"] == "OUTPUT08" and type(r["fields"].get("refill_risks")) is int
           and r["fields"]["refill_risks"] > 0 for r in records):
        warnings.append("OUTPUT08 indica risco de refill. Verifique a captura fisica de I2S.")
    if any(r["type"] == "GAP09" and type(r["fields"].get("unexpected_active")) is int
           and r["fields"]["unexpected_active"] > 0 for r in records):
        warnings.append("GAP09 indica fallback com notas ativas. Correlacione com a captura do DAC.")
    warnings.append(f"{unknown} linhas desconhecidas preservadas no log bruto.")
    covered = all(any(r["type"] == kind and all(type(r["fields"].get(k)) is int for k in keys)
                      for r in records) for kind, keys in COVERAGE.items())
    status = ("FAIL" if any(a["type"] in {"fatal", "i2s"} for a in alerts) else
              "NOT MEASURED" if not records else
              "INCONCLUSIVE" if not covered or resets or untimed or identity_issues
              else "PASS")
    coverage = summarize(records)
    coverage.update(capture_seconds=elapsed, untimed_lines=untimed)
    absent = [kind for kind, row in coverage["families"].items() if not row["count"]]
    if absent:
        warnings.append("Famílias periódicas sem amostras válidas: " + ", ".join(absent))
    metrics = {"schema": 1, "provenance": provenance, "raw_sha256": sha, "metadata": meta,
               "raw_file": raw_path.name, "metadata_file": metadata.name if metadata else None,
               "coverage": coverage, "identity_issues": identity_issues,
               "parser_status": status, "boots": boots, "resets": resets,
               "unknown_lines": unknown, "alerts": alerts, "records": records}
    if out is None:
        return metrics
    out.mkdir(parents=True, exist_ok=True)
    (out / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    with (out / "summary.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(("line", "ts_ms", "type", "key", "value"))
        for item in records:
            for key, value in item["fields"].items():
                writer.writerow((item["line"], item["ts_ms"], item["type"], key, value))
    (out / "warnings.txt").write_text("\n".join(warnings) + "\n", encoding="utf-8")
    (out / "session_report.md").write_text(
        f"# Sessão de bancada\n\n- Origem **{provenance}**\n"
        f"- Cobertura da análise **{status}**\n- SHA-256 do log `{sha}`\n"
        f"- Log `{raw_path.name}`, metadados `{metrics['metadata_file']}`\n"
        f"- Sessão `{meta.get('session_id')}`, placa `{meta.get('board_id')}`, perfil `{meta.get('profile')}`\n"
        f"- Firmware `{meta.get('firmware_sha256')}`, ELF `{meta.get('elf_sha256')}`\n"
        f"- Janela AUDIO observada (s) `{coverage['observed_seconds']}` e {coverage['periodic_snapshots']} relatórios completos.\n"
        f"- {len(records)} registros reconhecidos, {boots} boots e {len(resets)} regressões de timestamp.\n"
        "- Consulte [métricas](metrics.json), [CSV](summary.csv) e [avisos](warnings.txt).\n"
        "- O gate de release requer avaliação separada.\n", encoding="utf-8")
    return metrics


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--metadata", type=Path)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--fixture", action="store_true")
    args = parser.parse_args()
    analyze(args.log, args.out, args.metadata, args.fixture, args.bundle)
