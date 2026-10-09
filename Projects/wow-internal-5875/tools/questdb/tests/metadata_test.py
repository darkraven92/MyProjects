import pathlib
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import questdb
import quest_metadata


class MetadataTest(unittest.TestCase):
    def test_authored_metadata_union_is_not_regional_discovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = pathlib.Path(tmp) / "fixture.sqlite"
            questdb.import_sql(pathlib.Path(__file__).with_name("metadata_fixture.sql"), db, 10)
            conn = sqlite3.connect(db)
            cfg = {"map_id": 1, "bounds": {"min_x": -10,"max_x": 10,"min_y": -10,"max_y": 10},
                   "quest_ids": [10001], "authored_metadata_ids": [10003,10001,10003]}
            target=pathlib.Path(tmp)/"enrichment.tsv"
            questdb.export_runtime_catalog(conn,cfg,None,None,target)
            text=target.read_text()
            self.assertEqual(len([r for r in text.splitlines() if r.startswith("Q\t")]),2)
            self.assertIn("H\t10003\tenrichment_only",text)
            self.assertNotIn("H\t10001\t",text)
            self.assertIn("R\t10003\trewarded\tresolved\t10001\t10002",text)
            self.assertIn("M\t10003\t2\t1",text)
            questdb.export_runtime_catalog(conn,cfg,None,None,target)
            self.assertEqual(target.read_text(),text)
            conn.close()

    def test_trigger_cross_validation(self):
        t={"id":17,"build":5875,"mapid":1,"x":1,"y":2,"z":3,"radius":4,
           "boxx":0,"boxy":0,"boxz":0,"boxorientation":0,"scripted":False}
        q={"area_triggers":[t]}
        dbc={17:(17,1,1,2,3,4,0,0,0,0)}
        self.assertEqual(quest_metadata.verified_spheres(q,dbc),[t])
        self.assertEqual(quest_metadata.verified_spheres(q,{}),[])
        t["x"]=50; self.assertEqual(quest_metadata.verified_spheres(q,dbc),[])
        t["x"]=1; t["scripted"]=True
        self.assertEqual(quest_metadata.verified_spheres(q,dbc),[])
        t["scripted"]=False; t["radius"]=0
        self.assertEqual(quest_metadata.verified_spheres(q,dbc),[])

    def test_generic_gossip_credit_requires_proof(self):
        conn=sqlite3.connect(":memory:")
        conn.executescript(quest_metadata.SCHEMA)
        quest_metadata.preserve(conn,"creature_template",{"entry":17,"scriptname":"","gossipmenuid":4,"npcflags":1})
        quest_metadata.preserve(conn,"gossip_menu",{"entry":4,"textid":1,"scriptid":0,"conditionid":0})
        option={"menuid":4,"id":0,"optionid":1,"optiontext":"Credit","actionmenuid":-1,
                "actionscriptid":0,"actionpoiid":0,"boxcoded":0,"boxmoney":0,"conditionid":0}
        quest_metadata.preserve(conn,"gossip_menu_option",option)
        self.assertEqual(quest_metadata.gossip_credit(conn,17),"Credit")
        option["actionscriptid"]=7
        quest_metadata.preserve(conn,"gossip_menu_option",option)
        self.assertIsNone(quest_metadata.gossip_credit(conn,17))
        q={"metadata":{"SpecialFlags":0},"objectives":[{"kind":"creature","target_entry":17,"required_count":1}]}
        steps=questdb._runtime_objective_steps(conn,q,{"bounds":{}})
        self.assertTrue(steps[0]["semantic_ambiguous"])
        conn.close()

    def test_catalogue_deterministic(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = pathlib.Path(tmp) / "fixture.sqlite"
            questdb.import_sql(pathlib.Path(__file__).with_name("metadata_fixture.sql"), db, 10)
            conn = sqlite3.connect(db)
            cfg = {"map_id": 1, "bounds": {"min_x": -10,"max_x": 10,"min_y": -10,"max_y": 10},
                   "zone_or_sort_ids": [14,17], "maximum_min_level": 25,"include_other_classes": True}
            one, two = pathlib.Path(tmp)/"one.tsv", pathlib.Path(tmp)/"two.tsv"
            questdb.export_runtime_catalog(conn,cfg,None,"orc",one)
            questdb.export_runtime_catalog(conn,cfg,None,"orc",two)
            self.assertEqual(one.read_bytes(),two.read_bytes())
            records = [r.split("\t") for r in one.read_text().splitlines()]
            ids = [r[1] for r in records if r[0]=="Q"]
            self.assertEqual(len(ids),5)
            self.assertEqual(len(ids),len(set(ids)))
            self.assertIn("R\t10003\trewarded\tresolved\t10001\t10002",one.read_text())
            self.assertIn("M\t10003\t2\t1",one.read_text())
            self.assertIn("ExploreOrAreaTrigger",one.read_text())
            self.assertIn("\t-\t",one.read_text()) # absent breadcrumb, not invented zero
            conn.close()

    def test_pipeline(self):
        with tempfile.TemporaryDirectory() as tmp:
            db = pathlib.Path(tmp) / "fixture.sqlite"
            questdb.import_sql(pathlib.Path(__file__).with_name("metadata_fixture.sql"), db, 10)
            conn = sqlite3.connect(db)
            q = questdb.quest_record(conn, 10003, 1)
            self.assertEqual(q["metadata"]["SpecialFlags"], 2)
            self.assertIsNone(q["metadata"]["BreadcrumbForQuestId"])
            self.assertEqual(q["prerequisite_clauses"], [{"active": False, "resolved": True, "ids": [10001, 10002]}])
            self.assertEqual(q["area_triggers"][0]["x"], 1)
            self.assertEqual(q["area_trigger_ids"], [90001])
            active = questdb.quest_record(conn, 10004, 1)
            self.assertTrue(active["prerequisite_clauses"][0]["active"])
            self.assertEqual(active["exclusive_peers"], [10005])
            missing = questdb.quest_record(conn, 10005, 1)
            self.assertFalse(missing["prerequisite_clauses"][0]["resolved"])
            self.assertEqual(quest_metadata.rows(conn, "conditions")[0]["conditionentry"], 12)
            self.assertEqual((q["required_races"], q["required_classes"]), (2, 1))
            self.assertEqual(questdb.quest_record(conn, 10001)["next_quest_id"], 10003)
            conn.close()


if __name__ == "__main__":
    unittest.main()
