"""Synthetic SQL reward preservation, not Orc/runtime reward evidence."""
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import questdb
import quest_metadata


class RewardsTest(unittest.TestCase):
    def test_absence_zero_signed_cost_and_version_selection(self):
        with tempfile.TemporaryDirectory() as tmp:
            sql, db = Path(tmp) / 'rewards.sql', Path(tmp) / 'quest.sqlite'
            sql.write_text('''CREATE TABLE `quest_template` (`entry` int, `patch` int,
                `Title` text, `RewChoiceItemId1` int, `RewChoiceItemCount1` int,
                `RewItemId1` int, `RewItemCount1` int, `RewOrReqMoney` int,
                `RewSpell` int, `RewSpellCast` int, `RewRepFaction1` int, `RewRepValue1` int);
                INSERT INTO `quest_template` VALUES
                (100,10,'Synthetic',123,2,456,1,-50,0,42,76,100),
                (100,0,'Older',999,1,0,0,999,0,0,0,0),
                (100,11,'Later',888,1,0,0,888,0,0,0,0);''')
            questdb.import_sql(sql, db, 10)
            with sqlite3.connect(db) as conn:
                q = questdb.quest_record(conn, 100)
                r = q['reward_source']
                self.assertEqual(q['title'], 'Synthetic')
                self.assertEqual((r['RewChoiceItemId1'], r['RewChoiceItemCount1']), (123, 2))
                self.assertEqual((r['RewItemId1'], r['RewItemCount1']), (456, 1))
                self.assertEqual(r['RewOrReqMoney'], -50)
                self.assertEqual((r['RewSpell'], r['RewSpellCast']), (0, 42))
                self.assertEqual((r['RewRepFaction1'], r['RewRepValue1']), (76, 100))
                self.assertIsNone(r['RewXP'])
                self.assertIsNone(r['RewChoiceItemId2'])
                self.assertEqual(conn.execute("SELECT value FROM meta WHERE key='source_sql_sha256'").fetchone()[0],
                                 hashlib.sha256(sql.read_bytes()).hexdigest())
                self.assertEqual(json.dumps(q, sort_keys=True), json.dumps(questdb.quest_record(conn, 100), sort_keys=True))

    def test_old_database_is_unknown(self):
        with sqlite3.connect(':memory:') as conn:
            self.assertEqual(quest_metadata.rewards(conn, 100), dict.fromkeys(quest_metadata.REWARD_FIELDS))
