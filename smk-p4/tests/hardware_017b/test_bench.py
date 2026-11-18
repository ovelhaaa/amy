"""TEST FIXTURE only. No test in this module qualifies physical hardware."""
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from analyze import analyze
from coverage_gate import BINDING
from evaluate import evaluate, manual, write_result
from firmware import image_descriptor, PARTITION, partitions

ROOT = Path(__file__).parent
COMPLETE = (ROOT / "fixtures/TEST_FIXTURE_complete.log").read_text(encoding="utf-8")
CONFIG = json.loads((ROOT / "criteria.json").read_text(encoding="utf-8"))


class BenchTest(unittest.TestCase):
    def session(self, log=COMPLETE, evidence=True, fixture=True, fixture_mode=True,
                stress=(11000, 121000), metadata=None):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "serial.log"
            raw.write_text(log, encoding="utf-8")
            meta_path = out / "metadata.json"
            meta_path.write_text(json.dumps(metadata or {}), encoding="utf-8")
            metrics = analyze(raw, out, meta_path, fixture=fixture)
            proof = out / "fake-observation.txt"
            proof.write_text("TEST FIXTURE, synthetic data only", encoding="utf-8")
            observations = {r["id"]: {"value": r["limit"], "file": proof.name,
                                             "note": "Synthetic", "instrument": "TEST FIXTURE"}
                            for r in CONFIG["criteria"] if r["method"] == "manual"}
            manual = {"kind": "TEST FIXTURE", "raw_sha256": metrics["raw_sha256"],
                      "reviewer": "TEST FIXTURE", "observations": observations} if evidence else {}
            if evidence and stress:
                manual["stress"] = dict(zip(("start_ms", "end_ms"), stress))
            gate = evaluate(metrics, CONFIG, manual, out, fixture_mode=fixture_mode)
            write_result(gate, out)
            self.assertEqual(metrics, json.loads((out / "metrics.json").read_text(encoding="utf-8")))
            self.assertEqual(gate, json.loads((out / "gate.json").read_text(encoding="utf-8")))
            self.assertEqual(log, raw.read_text(encoding="utf-8"))
            with (out / "summary.csv").open(newline="", encoding="utf-8") as file:
                rows = list(csv.DictReader(file))
            self.assertEqual(len(rows), sum(len(r["fields"]) for r in metrics["records"]))
            self.assertTrue((out / "warnings.txt").is_file())
            self.assertTrue((out / "session_report.md").is_file())
            self.assertTrue((out / "release_gate.md").is_file())
            return metrics, gate

    def test_outputs_cannot_replace_raw_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "metrics.json"
            raw.write_bytes(b"raw serial bytes")
            with self.assertRaises(ValueError):
                analyze(raw, out, fixture=True)
            self.assertEqual(b"raw serial bytes", raw.read_bytes())

    def failed(self, log, id):
        _, gate = self.session(log)
        self.assertEqual(gate["status"], "FAIL")
        self.assertEqual(next(r["status"] for r in gate["results"] if r["id"] == id), "FAIL")

    def test_complete(self):
        metrics, gate = self.session()
        self.assertEqual("PASS", gate["status"], gate)
        self.assertEqual("NO-GO", gate["release"])
        self.assertEqual("TEST FIXTURE", metrics["provenance"])
        self.assertEqual(125, metrics["coverage"]["observed_seconds"])
        self.assertEqual(26, metrics["coverage"]["periodic_snapshots"])
        self.assertEqual(5.002, self.rule(gate, "MAXIMUM_TELEMETRY_GAP")["value"])
        by_kind = {r["type"]: r["fields"] for r in reversed(metrics["records"])}
        self.assertEqual(0x7F, by_kind["SCENE"]["effective"])
        self.assertEqual(48000, by_kind["MEMORY"]["internal_free"])
        self.assertEqual(800000, by_kind["MEMORY"]["psram_largest"])
        self.assertEqual(5000, by_kind["OUTPUT08"]["write_max_us"])
        self.assertEqual(30000, by_kind["SWAP08"]["internal_before_largest"])
        self.assertEqual("UNMEASURED", by_kind["OUTPUT08"]["underruns"])

    def test_driver_error_and_failed_write(self):
        self.failed(COMPLETE + "E (131010) i2s_common: write failure\n", "driver_i2s_errors")
        self.failed(COMPLETE.replace("10500 gaps=500 barriers=200 errors=0",
                                     "10500 gaps=500 barriers=200 errors=1"), "i2s_write_errors")

    def test_low_stack(self):
        self.failed(COMPLETE.replace("stack_free_bytes=3500", "stack_free_bytes=1200"), "owner_stack")

    def test_timeout(self):
        self.failed(COMPLETE.replace("errors=0 timeouts=0 refill_risks=7",
                                     "errors=0 timeouts=1 refill_risks=7"), "i2s_timeouts")

    def test_incomplete_or_unmeasured(self):
        for log in (COMPLETE[:COMPLETE.index("I (11000)")], "", "efi garbage\n"):
            with self.subTest(log_length=len(log)):
                _, gate = self.session(log)
                self.assertEqual("INCONCLUSIVE", gate["status"])
                self.assertTrue(any(r["status"] == "NOT MEASURED" for r in gate["results"]))

    def test_tag_names_without_fields_are_not_parser_pass(self):
        log = "\n".join(f"I (100) P4_AMY: {tag}" for tag in ("AUDIO", "OUTPUT08", "BENCH_SCENE", "USB"))
        metrics, gate = self.session(log)
        self.assertEqual("INCONCLUSIVE", metrics["parser_status"])
        self.assertEqual("INCONCLUSIVE", gate["status"])

    def test_usb_descriptor_metadata(self):
        metrics, _ = self.session(COMPLETE + "I (131010) P4_USB: VID=000a PID=000b address=3 descriptor_bytes=37\n"
                                  + "I (131011) P4_USB: MIDI1 interface=1 alt=0 IN=81 MPS=64\n")
        fields = [r["fields"] for r in metrics["records"] if r["type"] == "USB_DRIVER"]
        self.assertEqual(10, fields[0]["VID"])
        self.assertEqual(129, fields[1]["IN"])

    def test_partial_apply(self):
        self.failed(COMPLETE.replace("token=103 generation=214 phase=2",
                                     "token=103 generation=214 phase=5"), "scene_result")

    def test_generation(self):
        self.failed(COMPLETE.replace("generation_after=214", "generation_after=213"), "patch_generation")

    def test_corruption(self):
        self.failed(COMPLETE.replace("status=ESP_OK used_entries=420",
                                     "status=ESP_ERR_NVS_INVALID_STATE used_entries=420"), "nvs_status")

    def test_unknown_fields(self):
        metrics, gate = self.session(COMPLETE.replace("stack_free=2900", "stack_free=2900 future_field=abc")
                                     + "I (131006) future: FOO future_field=33\n")
        self.assertEqual("PASS", gate["status"])
        self.assertEqual(1, metrics["unknown_lines"])
        out = [r for r in metrics["records"] if r["type"] == "OUTPUT08"][-1]
        self.assertEqual("abc", out["fields"]["future_field"])

    def test_clock_proxy_is_not_physical_evidence(self):
        _, gate = self.session(evidence=False)
        self.assertEqual("INCONCLUSIVE", gate["status"])
        self.assertEqual("NOT MEASURED", next(r["status"] for r in gate["results"]
                                             if r["id"] == "physical_clock_interruptions"))

    def test_fixture_cannot_release(self):
        _, gate = self.session(fixture_mode=False)
        self.assertEqual("NO-GO", gate["release"])
        self.assertEqual("INCONCLUSIVE", gate["status"])

    def test_early_and_late_boundaries(self):
        for phase in ("6911232000", "6921600000"):
            self.failed(COMPLETE.replace("applied_phase=6912240000", "applied_phase=" + phase), "next_bar_40")

    def test_reboot_in_primary(self):
        self.failed(COMPLETE + "I (20) SMK_P4_GATE0: IDF=5.5.2 reset_reason=2\n", "unexpected_reboots")

    @staticmethod
    def rule(gate, name):
        return next(r for r in gate["results"] if r["id"] == name)

    def test_short_session_with_zero_errors(self):
        log = COMPLETE[:COMPLETE.index("I (16000)")]
        metrics, gate = self.session(log, stress=None, metadata={"requested_seconds": 1200})
        self.assertEqual(5, metrics["coverage"]["observed_seconds"])
        self.assertEqual("FAIL", self.rule(gate, "MINIMUM_SESSION_DURATION")["status"])
        self.assertEqual("INCONCLUSIVE", self.rule(gate, "i2s_write_errors")["status"])
        self.assertEqual("NO-GO", gate["release"])

    def test_host_capture_interrupted(self):
        for changes in ({"capture_exit": 1}, {"requested_seconds": 1200},
                        {"requested_seconds": 1200, "ended_utc": "2026-11-17T12:20:00Z"}):
            with self.subTest(changes=changes):
                metadata = {"role": "primary", "capture_exit": 0, "requested_seconds": 130,
                            "started_utc": "2026-11-17T12:00:00Z", "ended_utc": "2026-11-17T12:02:10Z"}
                metadata.update(changes)
                _, gate = self.session(metadata=metadata)
                self.assertEqual("INCONCLUSIVE", self.rule(gate, "capture_continuity")["status"])
                self.assertEqual("NO-GO", gate["release"])

    def test_large_gap_between_reports(self):
        log = re.sub(r"^I \((?:4100\d|4600\d)\).*\n", "", COMPLETE, flags=re.M)
        _, gate = self.session(log)
        gap = self.rule(gate, "MAXIMUM_TELEMETRY_GAP")
        self.assertEqual(15, gap["value"])
        self.assertEqual("FAIL", gap["status"])

    def test_single_snapshot(self):
        metrics, gate = self.session(COMPLETE[:COMPLETE.index("I (11000)")])
        self.assertEqual(1, metrics["coverage"]["periodic_snapshots"])
        self.assertEqual("NOT MEASURED", self.rule(gate, "MINIMUM_SESSION_DURATION")["status"])
        self.assertEqual("NO-GO", gate["release"])

    def test_missing_timestamp(self):
        _, gate = self.session(COMPLETE.replace("I (41000) P4_AMY: AUDIO", "I () P4_AMY: AUDIO"))
        self.assertTrue(gate["coverage"]["untimed_lines"])
        self.assertEqual("INCONCLUSIVE", self.rule(gate, "capture_continuity")["status"])

    def test_timestamp_regression(self):
        _, gate = self.session(COMPLETE.replace("I (41000) P4_AMY: AUDIO", "I (35999) P4_AMY: AUDIO"))
        self.assertTrue(gate["coverage"]["families"]["AUDIO"]["non_increasing_lines"])
        self.assertEqual("NO-GO", gate["release"])

    def test_expected_reboot_in_persistence_capture(self):
        metrics, gate = self.session(COMPLETE + "I (20) SMK_P4_GATE0: IDF=5.5.2 reset_reason=2\n",
                                     metadata={"role": "persistence"})
        self.assertEqual(2, metrics["boots"])
        self.assertEqual("NOT MEASURED", self.rule(gate, "unexpected_reboots")["status"])
        self.assertEqual("INCONCLUSIVE", gate["status"])
        self.assertEqual("NO-GO", gate["release"])

    def test_missing_baseline(self):
        _, gate = self.session(stress=(6000, 121000))
        self.assertEqual("FAIL", self.rule(gate, "PRE_STRESS_BASELINE")["status"])

    def test_missing_recovery(self):
        _, gate = self.session(stress=(11000, 131000))
        self.assertEqual("FAIL", self.rule(gate, "POST_STRESS_RECOVERY")["status"])

    def test_markers_missing_or_unanchored(self):
        for stress in (None, (11000, 121001), (121000, 11000)):
            with self.subTest(stress=stress):
                _, gate = self.session(stress=stress)
                self.assertEqual("NOT MEASURED", self.rule(gate, "PRE_STRESS_BASELINE")["status"])
                self.assertEqual("NO-GO", gate["release"])

    def test_missing_telemetry_family(self):
        log = re.sub(r"^.*: BENCH_AUDIO .*\n", "", COMPLETE, flags=re.M)
        metrics, gate = self.session(log)
        self.assertEqual(0, metrics["coverage"]["families"]["BENCH_AUDIO"]["count"])
        self.assertIn("BENCH_AUDIO", self.rule(gate, "MAXIMUM_TELEMETRY_GAP")["reason"])
        self.assertEqual("NO-GO", gate["release"])

    def test_sparse_family_with_continuous_audio(self):
        log = re.sub(r"^I \((?:4100\d|4600\d)\) .*: BENCH_AUDIO .*\n", "", COMPLETE, flags=re.M)
        metrics, gate = self.session(log)
        self.assertEqual(5, metrics["coverage"]["families"]["AUDIO"]["max_gap_seconds"])
        self.assertEqual(15, metrics["coverage"]["families"]["BENCH_AUDIO"]["max_gap_seconds"])
        self.assertEqual("FAIL", self.rule(gate, "MAXIMUM_TELEMETRY_GAP")["status"])

    def test_identity_match_required(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "serial.txt"
            raw.write_text(COMPLETE, encoding="utf-8")
            meta = {"kind": "REAL HARDWARE SESSION", "profile": "guition-editor-8-bench",
                    "session_id": "test", "board_id": "fixture", "capture_exit": 0,
                    "bytes": raw.stat().st_size, "raw_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
                    "firmware_sha256": "aa" * 32, "elf_sha256": "ac" * 32, "commit": "a" * 40,
                    "role": "primary", "port": "COM1", "baud": 115200, "requested_seconds": 130,
                    "reset_requested": False,
                    "started_utc": "2026-11-17T12:00:00Z", "ended_utc": "2026-11-17T12:02:10Z"}
            manifest = out / "manifest.json"
            manifest.write_text("mocked bundle manifest", encoding="utf-8")
            meta["manifest_sha256"] = hashlib.sha256(manifest.read_bytes()).hexdigest()
            path = out / "metadata.json"
            path.write_text(json.dumps(meta), encoding="utf-8")
            # The fixture flag wins even when provided capture-shaped metadata.
            self.assertEqual("TEST FIXTURE", analyze(raw, out, path, fixture=True)["provenance"])
            with patch("analyze.validate", return_value=meta.copy()):
                self.assertEqual("REAL HARDWARE SESSION", analyze(raw, out, path, bundle=out)["provenance"])
                for field in ("elf_sha256", "firmware_sha256", "profile", "board_id", "capture_exit",
                              "manifest_sha256", "session_id"):
                    with self.subTest(field=field):
                        path.write_text(json.dumps(meta | {field: ""}), encoding="utf-8")
                        metrics = analyze(raw, out, path, bundle=out)
                        self.assertEqual("UNVERIFIED", metrics["provenance"])
                        self.assertEqual("INCONCLUSIVE", evaluate(metrics, CONFIG)["status"])
                path.write_text(json.dumps(meta), encoding="utf-8")
                raw.write_text(COMPLETE + "appended bytes\n", encoding="utf-8")
                spoiled = analyze(raw, out, path, bundle=out)
                self.assertEqual("UNVERIFIED", spoiled["provenance"])
                self.assertTrue(any("log bruto" in issue for issue in spoiled["identity_issues"]))

    def test_cli_metadata_to_release_gate(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "serial.log"
            raw.write_bytes((ROOT / "fixtures/TEST_FIXTURE_complete.log").read_bytes())
            digest = hashlib.sha256(raw.read_bytes()).hexdigest()
            metadata = out / "serial.log.metadata.json"
            metadata.write_text(json.dumps({"kind": "TEST FIXTURE", "session_id": "fixture-01",
                                            "board_id": "synthetic", "raw_sha256": digest,
                                            "capture_exit": 0, "bytes": raw.stat().st_size,
                                            "role": "primary", "requested_seconds": 130,
                                            "started_utc": "2026-11-17T12:00:00Z",
                                            "ended_utc": "2026-11-17T12:02:10Z"}), encoding="utf-8")
            proof = out / "fixture.txt"
            proof.write_text("Synthetic fixture", encoding="utf-8")
            evidence = out / "evidence.json"
            evidence.write_text(json.dumps({
                "kind": "TEST FIXTURE", "raw_sha256": digest, "reviewer": "test",
                "stress": {"start_ms": 11000, "end_ms": 121000},
                "observations": {r["id"]: {"value": r["limit"], "file": proof.name,
                                           "note": "synthetic", "instrument": "fixture"}
                                 for r in CONFIG["criteria"] if r["method"] == "manual"},
            }), encoding="utf-8")
            subprocess.run([sys.executable, str(ROOT / "analyze.py"), str(raw), "--metadata",
                            str(metadata), "--fixture", "--out", temp], check=True, capture_output=True)
            subprocess.run([sys.executable, str(ROOT / "evaluate.py"), str(out / "metrics.json"),
                            "--evidence", str(evidence), "--raw", str(raw), "--metadata", str(metadata),
                            "--test-fixture", "--out", temp], check=True, capture_output=True)
            gate = json.loads((out / "gate.json").read_text(encoding="utf-8"))
            self.assertEqual(("TEST FIXTURE", "PASS", "NO-GO"),
                             (gate["provenance"], gate["status"], gate["release"]))
            self.assertEqual("fixture-01", gate["identity"]["session_id"])
            self.assertEqual(digest, gate["raw_sha256"])
            self.assertEqual(digest, hashlib.sha256(raw.read_bytes()).hexdigest())
            for name in ("metrics.json", "summary.csv", "warnings.txt", "session_report.md",
                         "gate.json", "release_gate.md"):
                self.assertTrue((out / name).stat().st_size, name)

    def test_instrument_attachment_must_match_hash_session_and_type(self):
        rule = next(r for r in CONFIG["criteria"] if r["id"] == "physical_clock_interruptions")
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            # Exercise validation only. This file is synthetic and is never evaluated for release.
            sample = out / "fixture.vcd"
            sample.write_bytes(b"synthetic bytes")
            metadata = {key: f"fixture-{key}" for key in BINDING}
            metrics = {"provenance": "REAL HARDWARE SESSION", "metadata": metadata, "raw_sha256": "fixture"}
            observation = {"value": 0, "file": sample.name, "note": "synthetic",
                           "instrument": "fixture", "sha256": hashlib.sha256(sample.read_bytes()).hexdigest()}
            evidence = metadata | {"kind": "REAL HARDWARE SESSION", "raw_sha256": "fixture",
                                   "reviewer": "fixture", "observations": {rule["id"]: observation}}
            self.assertTrue(manual(rule, evidence, out, metrics, False)[2])
            self.assertFalse(manual(rule, evidence | {"board_id": "different"}, out, metrics, False)[2])
            text_file = sample.with_suffix(".txt")
            sample.rename(text_file)
            observation["file"] = text_file.name
            self.assertFalse(manual(rule, evidence, out, metrics, False)[2])
            text_file.rename(sample)
            observation["file"] = sample.name
            sample.write_bytes(b"replaced after review")
            self.assertFalse(manual(rule, evidence, out, metrics, False)[2])


class FirmwareFormatTest(unittest.TestCase):
    def test_read_descriptor_and_partition(self):
        image = bytearray(288)
        image[0] = 0xE9
        image[32:36] = bytes.fromhex("3254cdab")
        image[48:51] = b"0.1"
        image[144:149] = b"5.5.2"
        image[176:208] = bytes(range(32))
        self.assertEqual("5.5.2", image_descriptor(image)["idf_version"])
        self.assertEqual(bytes(range(32)).hex(), image_descriptor(image)["elf_sha256"])
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "partition-table.bin"
            path.write_bytes(PARTITION.pack(0x50AA, 0, 0, 0x10000, 0x300000, b"factory", 0)
                             + b"\xff" * PARTITION.size)
            self.assertEqual({"name": "factory", "type": 0, "subtype": 0,
                              "offset": 0x10000, "size": 0x300000}, partitions(path)[0])


if __name__ == "__main__":
    unittest.main()
