import tempfile
import unittest
from pathlib import Path

from compare_amy_pcm import metrics


class PcmComparisonTests(unittest.TestCase):
    def read(self, text, require_result=True):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.log"
            path.write_text(text, encoding="utf-8")
            return metrics(path, require_result)

    @staticmethod
    def capture():
        return (
            "I (20447) P4_AMY: RAM_SELF_TEST patch=128 peak=2148 rms=572.0152 clipped=0 release=1 panic=1\n"
            "I (20447) P4_AMY: RAM_SELF_TEST patch=135 peak=1810 rms=558.2924 clipped=0 release=1 panic=1\n"
            "I (20467) P4_AMY: RAM_SELF_TEST result=PASS I2S=DISABLED restarted=1\n"
        )

    def test_completed_target(self):
        self.assertEqual(set(self.read(self.capture())), {128, 135})

    def test_reject_truncated_before_restart(self):
        with self.assertRaises(ValueError):
            self.read(self.capture().split("I (20467)")[0])

    def test_reject_failed_restart(self):
        with self.assertRaises(ValueError):
            self.read(self.capture().replace("result=PASS", "result=FAIL"))

    def test_reject_mixed_or_repeated_boots(self):
        with self.assertRaises(ValueError):
            self.read(self.capture() * 2)

    def test_reject_missing_patch(self):
        with self.assertRaises(ValueError):
            self.read(self.capture().replace("patch=135", "patch=136"))


if __name__ == "__main__":
    unittest.main()
