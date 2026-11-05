import unittest
from audit_patch_library import inventory, report, ROOT

class PatchInventoryTest(unittest.TestCase):
    def test_table_bounds_and_reserved_slots(self):
        _, costs, entries = inventory()
        self.assertEqual(len(costs), 391)
        self.assertEqual(len(entries), 266)
        self.assertEqual(set(range(391)) - entries.keys(), set(range(259,384)))
        self.assertTrue(all(costs[i] == 0 for i in range(259,384)))

    def test_actual_synthesis_and_candidate_costs(self):
        _, _, entries = inventory()
        self.assertEqual(entries[17][2], "subtractive")
        self.assertEqual(entries[256][2], "partials")
        self.assertIn("BLOCKED", entries[256][-1])
        self.assertEqual(entries[258][2], "PCM")
        self.assertIn("BLOCKED", entries[258][-1])
        for patch in (128,135,138,142):
            self.assertEqual(entries[patch][1:3], (8,"FM"))
        for patch in (4,8,31,32):
            self.assertEqual(entries[patch][1:3], (6,"subtractive"))

    def test_inventory_is_current(self):
        self.assertEqual((ROOT / "smk-p4/docs/patch_library_inventory_07.md").read_text(encoding="utf-8"), report())

if __name__ == "__main__":
    unittest.main()
