import unittest
from rehearsal_windows import COUNTERS, compare, report


def sample():
    return dict.fromkeys(COUNTERS, 0) | dict(boot_id=7, elf_sha256="abc", uptime_us=1_000_000,
        block=256, rate=48000, dma_buffers=4, dma_frames=256, pcm_blocks=2, probe=1)


class WindowsTest(unittest.TestCase):
    def test_deltas_and_window_mean(self):
        a = sample()
        b = a | dict(uptime_us=2_000_000, frames=47872, probe_blocks=187, probe_service_us=187000)
        result = compare(a, b)
        self.assertEqual(result["delta"]["frames"], 47872)
        self.assertEqual(result["probe_service_mean_us"], 1000)

    def test_reboot_config_reset_and_gap_rejected(self):
        a = sample() | dict(frames=1000000)
        for change in (dict(boot_id=8), dict(elf_sha256="def"), dict(rate=44100),
                       dict(uptime_us=1), dict(uptime_us=70_000_000), dict(frames=0), dict(probe=0)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                compare(a, a | dict(uptime_us=2_000_000) | change)

    def test_u32_wrap(self):
        a = sample() | dict(frames=0xFFFFFF00, probe_service_us=0xFFFFFF00)
        b = a | dict(uptime_us=2_000_000, frames=256, probe_service_us=744, probe_blocks=1)
        self.assertEqual(compare(a, b)["delta"]["frames"], 512)
        self.assertEqual(compare(a, b)["probe_service_mean_us"], 1000)

    def test_complete_records_only(self):
        with self.assertRaises(ValueError):
            report("=== AUDIO QUAL ===\nframes=100\n", "A", "")
        a = sample()
        b = a | dict(uptime_us=2_000_000, frames=256)
        def encode(s):
            return "=== AUDIO QUAL ===\n" + " ".join(f"{k}={v}" for k, v in s.items()) + "\n=== AUDIO QUAL END ===\n"
        result = report("\x1b[32m" + encode(a).replace("\n", "\x1b[0m\n") + encode(b), "A", "not listened")
        self.assertEqual(len(result["windows"]), 1)
        result = report(encode(a) + encode(b | dict(boot_id=8)), "A", "")
        self.assertEqual(result["windows"][0]["status"], "BLOCKED")


if __name__ == "__main__":
    unittest.main()
