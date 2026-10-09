import pathlib
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import vendors


class VendorTest(unittest.TestCase):
    def test_source_export_deterministic_not_live_verification(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root/'source.sqlite'
            with sqlite3.connect(source) as db:
                db.execute('CREATE TABLE creature_spawn(entry,guid,map_id,x,y,z)')
                db.executemany('INSERT INTO creature_spawn VALUES (?,?,?,?,?,?)', [
                    (60001,1,1,1,2,3), (60002,2,1,4,5,6), (60003,3,1,7,8,9),
                    (60004,4,1,10,11,12), (60002,5,0,1,2,3)])
            fixture = pathlib.Path(__file__).with_name('vendor_fixture.sql')
            before = source.read_bytes()
            self.assertEqual(vendors.export(fixture, source, root/'a.tsv'), (2, 2))
            vendors.export(fixture, source, root/'b.tsv')
            self.assertEqual(source.read_bytes(), before)
            self.assertEqual((root/'a.tsv').read_bytes(), (root/'b.tsv').read_bytes())
            text = (root/'a.tsv').read_text()
            self.assertIn('N\t60001\t35\t4', text)
            self.assertIn('N\t60002\t35\t16388', text)
            self.assertNotIn('60003', text)
            self.assertNotIn('60004', text)
            self.assertIn('F\t35\t35\t1\t1\t0', text)
            self.assertNotIn('Available', text)


if __name__ == '__main__':
    unittest.main()
