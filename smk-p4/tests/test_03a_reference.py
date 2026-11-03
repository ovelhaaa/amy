import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from verify_03a_reference import config_values


class ReferenceTests(unittest.TestCase):
    script = Path(__file__).with_name("verify_03a_reference.py")
    frozen = script.parents[1] / "docs/baselines/standalone_03a.sdkconfig"

    def run_check(self, *args):
        return subprocess.run([sys.executable, "-O", str(self.script), *map(str, args)],
                              capture_output=True, text=True)

    def test_kconfig_values(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "sdkconfig"
            config.write_text('CONFIG_ON=y\n# CONFIG_OFF is not set\nCONFIG_ID="esp32p4"\nCONFIG_PIN=50\n')
            self.assertEqual(config_values(config), {"CONFIG_ON": "y", "CONFIG_OFF": "n",
                                                     "CONFIG_ID": '"esp32p4"', "CONFIG_PIN": "50"})

    def test_frozen_config_and_absent_optional_bundle(self):
        result = self.run_check("--sdkconfig", self.frozen)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("NOT RUN: exact binary", result.stdout)
        self.assertIn("PASS: effective musical", result.stdout)

    def test_no_implicit_bundle_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_check("--image-dir", directory)
            self.assertNotEqual(result.returncode, 0)

    def test_different_usb_controller_rejected_even_with_python_optimization(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "sdkconfig"
            config.write_text(self.frozen.read_text().replace("CONFIG_SMKP4_USB_CONTROLLER=0",
                                                             "CONFIG_SMKP4_USB_CONTROLLER=1"))
            result = self.run_check("--sdkconfig", config)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("CONFIG_SMKP4_USB_CONTROLLER", result.stderr)


if __name__ == "__main__":
    unittest.main()
