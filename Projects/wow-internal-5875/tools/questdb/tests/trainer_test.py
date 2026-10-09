import copy
import pathlib
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import trainers


class TrainerTest(unittest.TestCase):
    def setUp(self):
        self.fixture = pathlib.Path(__file__).with_name('trainer_fixture.sql')
        self.rows = trainers.read_source(self.fixture)

    def test_build_and_patch_resolution(self):
        rows, spells, _ = trainers.normalize(self.rows)
        self.assertEqual(len(rows), 2)
        self.assertEqual(spells[50001]['name'], 'Test Ability')
        self.assertEqual(spells[50002]['name'], 'Test Ability')
        self.assertEqual(rows[1][8], 50001)
        self.assertEqual({r[1] for r in rows}, {8})

    def test_profession_not_class_trainer(self):
        self.rows['creature_template'][0]['trainertype'] = 2
        self.assertEqual(trainers.normalize(self.rows)[0], [])

    def test_skill_service_rejected(self):
        self.rows['npc_trainer_template'][0]['reqskill'] = 100
        self.assertEqual(len(trainers.normalize(self.rows)[0]), 1)

    def test_nonlearning_service_rejected(self):
        self.rows['spell_template'][0]['effect1'] = 2
        self.assertEqual(len(trainers.normalize(self.rows)[0]), 1)

    def test_unknown_trigger_rejected(self):
        self.rows['spell_template'][0]['effecttriggerspell1'] = 999999
        self.assertEqual(len(trainers.normalize(self.rows)[0]), 1)

    def test_service_conflict_not_overwritten(self):
        duplicate = copy.deepcopy(self.rows['npc_trainer_template'][0])
        duplicate['spellcost'] = 99
        self.rows['npc_trainer_template'].append(duplicate)
        self.assertEqual(len(trainers.normalize(self.rows)[0]), 1)

    def test_export_normalized_and_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            db = root/'quests.sqlite'
            with sqlite3.connect(db) as conn:
                conn.execute('CREATE TABLE creature_spawn(entry,guid,map_id,x,y,z)')
                conn.execute('INSERT INTO creature_spawn VALUES (60001,1,1,1,2,3)')
            for n in (1, 2):
                trainers.export(self.fixture, db, root/f'{n}.tsv', root/f'{n}.sqlite')
            self.assertEqual((root/'1.tsv').read_bytes(), (root/'2.tsv').read_bytes())
            with sqlite3.connect(root/'1.sqlite') as conn:
                self.assertEqual(conn.execute('SELECT count(*) FROM service').fetchone()[0], 2)
            text = (root/'1.tsv').read_text()
            self.assertIn('S\t50002\tTest Ability\tRank 2\t50001\t2', text)
            self.assertIn('P\t60001\t1\t1\t1\t2\t3', text)
            with self.assertRaises(ValueError):
                trainers.export(self.fixture, db, root/'1.tsv', root/'1.sqlite')


if __name__ == '__main__':
    unittest.main()
