"""TEST FIXTURE only. No test in this module qualifies physical hardware."""
import csv
import json
from pathlib import Path
import tempfile
import unittest

from analyze import analyze
from evaluate import evaluate, write_result
from firmware import image_descriptor, PARTITION, partitions

ROOT = Path(__file__).parent
COMPLETE = (ROOT / "fixtures/TEST_FIXTURE_complete.log").read_text(encoding="utf-8")
CONFIG = json.loads((ROOT / "criteria.json").read_text(encoding="utf-8"))


class BenchTest(unittest.TestCase):
    def session(self, log=COMPLETE, evidence=True, fixture=True, fixture_mode=True):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "serial.log"
            raw.write_text(log, encoding="utf-8")
            metrics = analyze(raw, out, fixture=fixture)
            proof = out / "fake-observation.txt"
            proof.write_text("TEST FIXTURE, synthetic data only", encoding="utf-8")
            observations = {r["id"]: {"value": r["limit"], "file": proof.name,
                                             "note": "Synthetic", "instrument": "TEST FIXTURE"}
                            for r in CONFIG["criteria"] if r["method"] == "manual"}
            manual = {"kind": "TEST FIXTURE", "raw_sha256": metrics["raw_sha256"],
                      "reviewer": "TEST FIXTURE", "observations": observations} if evidence else {}
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
        by_kind = {r["type"]: r["fields"] for r in reversed(metrics["records"])}
        self.assertEqual(0x7F, by_kind["SCENE"]["effective"])
        self.assertEqual(48000, by_kind["MEMORY"]["internal_free"])
        self.assertEqual(800000, by_kind["MEMORY"]["psram_largest"])
        self.assertEqual(5000, by_kind["OUTPUT08"]["write_max_us"])
        self.assertEqual(30000, by_kind["SWAP08"]["internal_before_largest"])
        self.assertEqual("UNMEASURED", by_kind["OUTPUT08"]["underruns"])

    def test_driver_error_and_failed_write(self):
        self.failed(COMPLETE + "E (61010) i2s_common: write failure\n", "driver_i2s_errors")
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
        metrics, _ = self.session(COMPLETE + "I (61010) P4_USB: VID=000a PID=000b address=3 descriptor_bytes=37\n"
                                  + "I (61011) P4_USB: MIDI1 interface=1 alt=0 IN=81 MPS=64\n")
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
                                     + "I (61005) future: FOO future_field=33\n")
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

    def test_identity_match_required(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            raw = out / "serial.txt"
            raw.write_text(COMPLETE, encoding="utf-8")
            import hashlib
            meta = {"kind": "REAL HARDWARE SESSION", "profile": "guition-editor-8-bench",
                    "session_id": "test", "board_id": "fixture", "capture_exit": 0,
                    "bytes": raw.stat().st_size, "raw_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
                    "firmware_sha256": "aa" * 32, "elf_sha256": "ac" * 32}
            path = out / "metadata.json"
            path.write_text(json.dumps(meta), encoding="utf-8")
            # The fixture flag wins even when provided capture-shaped metadata.
            self.assertEqual("TEST FIXTURE", analyze(raw, out, path, fixture=True)["provenance"])
            meta["elf_sha256"] = "00" * 32
            path.write_text(json.dumps(meta), encoding="utf-8")
            self.assertEqual("UNVERIFIED", analyze(raw, out, path)["provenance"])


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
