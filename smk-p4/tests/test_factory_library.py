"""Catalog integrity and negative generator checks; no AMY mocks qualify audio."""
import unittest
from unittest.mock import patch
import generate_factory_library as gen

class FactoryCatalogTest(unittest.TestCase):
    def test_generated_files_current(self):
        for path,data in gen.products().items():
            self.assertEqual(path.read_text(encoding='utf-8'),data,str(path))
    def test_category_is_always_real_category(self):
        _,_,entries=gen.inventory()
        self.assertEqual(len([i for i in entries if i<256]),256)
        for i in range(256):self.assertIn(gen.category(entries[i][0]),('Piano','ElectricPiano','Organ','Bass','Brass','Strings','Pads','Leads','Plucks','Percussion','FX'))
    def test_reject_duplicate_factory_id(self):
        source,costs,entries=gen.inventory()
        with patch.object(gen,'inventory',return_value=(source+'\n/* 0: DUP */ "v0w0Z",',costs,entries)):
            with self.assertRaises(AssertionError):gen.products()
    def test_reject_missing_name(self):
        source,costs,entries=gen.inventory();entries=dict(entries);row=list(entries[0]);row[0]='';entries[0]=tuple(row)
        with patch.object(gen,'inventory',return_value=(source,costs,entries)):
            with self.assertRaises(AssertionError):gen.products()
    def test_reject_oversized_voice(self):
        source,costs,entries=gen.inventory();entries=dict(entries);row=list(entries[0]);row[1]=9;entries[0]=tuple(row)
        with patch.object(gen,'inventory',return_value=(source,costs,entries)):
            with self.assertRaises(AssertionError):gen.products()
if __name__=='__main__':unittest.main()
