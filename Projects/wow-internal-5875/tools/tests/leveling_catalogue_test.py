"""Offline contracts, source drift and adversarial catalogue fixtures."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/leveling'))
import catalogue as c
import build_orc_catalogue as b

CATALOGUE = ROOT / b.OUTPUT


class CatalogueTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.production = c.read_json(CATALOGUE)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.data = copy.deepcopy(self.production)

    def pin_fixture(self):
        """Synthetic values become synthetic source evidence for semantic tests.

        Production corruption tests do NOT repin. This separates provenance
        failures from structural/reference failures without weakening the loader.
        """
        values = []
        def visit(node):
            if isinstance(node, dict):
                if 'value' in node:
                    if node.get('status') != 'unknown':
                        node['refs'] = [{'source': 'fixture', 'pointer': '/' + str(len(values))}]
                        node['transform'] = 'direct'
                        values.append(copy.deepcopy(node['value']))
                else:
                    for value in node.values():
                        visit(value)
            elif isinstance(node, list):
                for value in node:
                    visit(value)
        visit(self.data)
        source = self.root / 'synthetic.json'
        source.write_text(json.dumps(values))
        self.data['sources'] = {'fixture': {'path': source.name, 'sha256': c.digest(source.read_bytes()),
            'format': 'json', 'description': 'Synthetic test only; never production evidence.'}}

    def validate_fixture(self):
        self.pin_fixture()
        c.validate(self.data, self.root)

    def fails(self, code, *, repin=True):
        if repin:
            self.pin_fixture()
        with self.assertRaises(c.CatalogueError) as caught:
            c.validate(self.data, self.root if repin else ROOT)
        self.assertIn(code, {i.code for i in caught.exception.issues}, str(caught.exception))
        return caught.exception

    def quest(self, qid):
        return next(q for q in self.data['quests'] if q['id']['value'] == qid)

    def test_production_reproducible_and_loaded(self):
        self.assertEqual(self.production, b.build())
        snapshot = c.load(CATALOGUE, ROOT)
        self.assertEqual(len(snapshot.records['quests']), 12)
        self.assertEqual({s['value']['type'] for p in snapshot.records['profiles'] for s in p['steps']}, c.STEP_TYPES)
        self.assertEqual(self.quest(788)['objectives'][0]['count']['value'], 10)
        self.assertEqual(self.quest(789)['objectives'][0]['creatures']['value'], [3124, 3281])
        self.assertEqual(self.quest(2383)['objectives'][0]['source_mode']['value'], 'quest-supplied')
        self.assertEqual(self.quest(2383)['required_classes']['value'], 1)
        self.assertEqual(self.quest(2383)['required_races']['value'], 50)
        self.assertEqual(self.quest(792)['objectives'][0]['count']['value'], 12)
        self.assertEqual(self.quest(794)['prerequisites']['value'], [
            {'state': 'rewarded', 'resolved': True, 'all_of': [792]},
            {'state': 'rewarded', 'resolved': True, 'all_of': [1499]}])
        self.assertEqual(self.quest(5441)['objectives'][0]['kind']['value'], 'UseItemOnUnit')
        self.assertIsNone(self.quest(788)['rewards']['choice_items']['value'])
        self.assertEqual(self.quest(4402)['rewards']['money_copper']['value'], 50)
        self.assertEqual(self.quest(6394)['rewards']['money_copper']['value'], 150)

    def test_reward_shapes_preserve_slots_and_spell_modes(self):
        rewards = self.quest(788)['rewards']
        for key, value in {
            'choice_items': [{'slot': 2, 'item_id': 12635, 'count': 1}],
            'guaranteed_items': [{'slot': 1, 'item_id': 12635, 'count': 2}],
            'spells': [{'spell_id': 42, 'mode': 'cast'}, {'spell_id': 43, 'mode': 'learn'}],
            'reputation': [{'slot': 1, 'faction_id': 76, 'value': 100}],
        }.items():
            rewards[key]['status'] = 'source-backed'  # Synthetic source, not an Orc reward claim.
            rewards[key]['value'] = value
        self.validate_fixture()
        rewards['choice_items']['value'][0]['item_id'] = 999999
        self.fails('item_reference')
        rewards['choice_items']['value'][0]['item_id'] = 12635
        rewards['choice_items']['value'].append(copy.deepcopy(rewards['choice_items']['value'][0]))
        self.fails('reward')
        rewards['choice_items']['value'].pop()
        rewards['spells']['value'][0]['mode'] = 'unspecified'
        self.fails('reward')

    def test_missing_and_duplicate_quest_ids(self):
        self.data['quests'].remove(self.quest(788))
        self.fails('prerequisite_reference')
        self.data = copy.deepcopy(self.production)
        self.data['quests'].append(copy.deepcopy(self.quest(788)))
        self.fails('quest_id')

    def test_missing_profile_quest_and_external_reference(self):
        self.data['profiles'][0]['quest_ids']['value'].append(999999)
        self.fails('quest_reference')
        self.data = copy.deepcopy(self.production)
        self.data['external_quests'] = []
        self.fails('prerequisite_reference')

    def test_broken_actor_creature_item_and_gameobject(self):
        for key, code in [('creature:3143', 'npc_reference'), ('creature:3098', 'creature_reference'),
                          ('item:4862', 'item_reference'), ('gameobject:171938', 'gameobject_reference')]:
            with self.subTest(key=key):
                self.data = copy.deepcopy(self.production)
                self.data['entities'] = [e for e in self.data['entities'] if e['key']['value'] != key]
                self.fails(code)

    def test_actor_conflicts(self):
        steps = self.data['profiles'][0]['steps']
        next(s for s in steps if s['value']['type'] == 'Pickup')['value']['npc'] = 'creature:3153'
        self.fails('actor_conflict')
        self.data = copy.deepcopy(self.production)
        next(s for s in self.data['profiles'][0]['steps'] if s['value']['type'] == 'TurnIn')['value']['npc'] = 'creature:3153'
        self.fails('actor_conflict')

    def test_duplicate_id_and_semantic_step(self):
        step = copy.deepcopy(self.data['profiles'][0]['steps'][0])
        self.data['profiles'][0]['steps'].insert(1, step)
        self.fails('duplicate_step')
        step['value']['id'] = 'different-id-same-action'
        self.fails('duplicate_step')

    def test_unrepresentable_step_and_objective(self):
        self.data['profiles'][0]['steps'][0]['value']['type'] = 'Teleport'
        self.fails('unsupported_step')
        self.data = copy.deepcopy(self.production)
        self.quest(788)['objectives'][0]['kind']['value'] = 'Escort'
        self.fails('unsupported_objective')
        self.data = copy.deepcopy(self.production)
        self.data['profiles'][0]['steps'][0]['value']['run_lua'] = 'hidden side effect'
        self.fails('schema')

    def test_use_item_cannot_be_lowered_to_kill(self):
        self.quest(788)['objectives'][0]['kind']['value'] = 'UseItemOnUnit'
        self.quest(788)['objectives'][0]['source_mode']['value'] = 'use-item'
        self.quest(788)['objectives'][0]['item_id']['value'] = 16114
        self.fails('unsupported_objective')

    def test_missing_provenance_and_unknown_not_zero(self):
        del self.quest(788)['title']['refs']
        self.fails('schema', repin=False)
        self.data = copy.deepcopy(self.production)
        self.quest(788)['title'] = 'Cutting Teeth'
        self.fails('missing_provenance')
        self.data = copy.deepcopy(self.production)
        self.quest(788)['rewards']['xp']['value'] = 0
        self.fails('provenance')

    def test_value_conflict_and_source_pointer(self):
        self.quest(788)['title']['value'] = 'Wrong title'
        self.fails('source_conflict', repin=False)
        self.data = copy.deepcopy(self.production)
        self.quest(788)['title']['refs'][0]['pointer'] = '/quests/9999/title'
        self.fails('provenance', repin=False)

    def test_source_actor_disagreement_even_after_digest_update(self):
        for source in self.data['sources'].values():
            target = self.root / source['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / source['path']).read_bytes())
        source = self.data['sources']['enriched']
        path = self.root / source['path']
        lines = path.read_text().splitlines()
        for i, line in enumerate(lines):
            if line.startswith('Q\t788\t'):
                cells = line.split('\t')
                cells[5] = '3153'
                lines[i] = '\t'.join(cells)
        path.write_text('\n'.join(lines) + '\n')
        source['sha256'] = c.digest(path.read_bytes())
        with self.assertRaisesRegex(c.CatalogueError, 'actor_conflict'):
            c.validate(self.data, self.root)
        with self.assertRaisesRegex(ValueError, 'actor conflict'):
            b.build(self.root)

    def test_raw_relationship_conflict_and_unresolved_cycle(self):
        self.quest(789)['prerequisites']['value'] = []
        self.fails('relationship_conflict')
        self.data = copy.deepcopy(self.production)
        self.quest(788)['prerequisites']['value'] = [
            {'state': 'rewarded', 'resolved': False, 'all_of': [789]}]
        self.validate_fixture()  # An unresolved group is not proof of an impossible graph.

    def test_signed_relationships_and_or_cycle(self):
        q = self.quest(788)
        q['prerequisites']['value'] = [{'state': 'active', 'resolved': True, 'all_of': [789]}]
        self.fails('prerequisite_cycle')
        q['prerequisites']['value'].append({'state': 'rewarded', 'resolved': True, 'all_of': [790]})
        self.validate_fixture()  # Alternative can break a cycle.
        q['prerequisites']['value'] = [{'state': 'rewarded', 'resolved': True, 'all_of': [789, 790]}]
        self.fails('prerequisite_cycle')  # AND group cannot.
        q['prev_quest_id']['value'] = -999999
        self.fails('quest_reference')

    def test_self_prerequisite_and_chain_hint_not_requirement(self):
        self.quest(788)['prerequisites']['value'] = [{'state': 'rewarded', 'resolved': True, 'all_of': [788]}]
        self.fails('prerequisite_reference')
        self.data = copy.deepcopy(self.production)
        self.assertEqual(self.quest(4641)['next_in_chain']['value'], 788)
        self.assertEqual(self.quest(788)['prerequisites']['value'], [])

    def test_bad_counts_locations_and_profile_restrictions(self):
        self.quest(788)['objectives'][0]['count']['value'] = 0
        self.fails('objective')
        self.data = copy.deepcopy(self.production)
        e = next(e for e in self.data['entities'] if e['locations']['value'])
        e['locations']['value'][0]['map_id'] = -1
        self.fails('location')
        self.data = copy.deepcopy(self.production)
        self.data['profiles'][0]['class_mask']['value'] = 256
        self.fails('restriction')

    def test_step_order_and_coverage(self):
        steps = self.data['profiles'][0]['steps']
        steps[:] = [s for s in steps if s['value']['type'] != 'MoveToObjective']
        self.fails('step_order')
        self.data = copy.deepcopy(self.production)
        self.data['profiles'][0]['steps'].pop(0)
        self.fails('step_coverage')

    def test_version_and_unknown_fields(self):
        self.data['schema_version'] = 2
        self.fails('version', repin=False)
        self.data = copy.deepcopy(self.production)
        self.quest(788)['unrecognized_requirement'] = 123
        self.fails('schema')

    def test_source_drift_and_path_escape(self):
        self.pin_fixture()
        (self.root / 'synthetic.json').write_text('[]')
        with self.assertRaisesRegex(c.CatalogueError, 'SHA256 changed'):
            c.validate(self.data, self.root)
        self.data['sources']['fixture']['path'] = '../outside.json'
        with self.assertRaisesRegex(c.CatalogueError, 'within project root'):
            c.validate(self.data, self.root)

    def test_snapshot_identity_ordering_copy_and_reload(self):
        self.pin_fixture()
        path = self.root / 'catalogue.json'
        path.write_text(json.dumps(self.data))
        first = c.load(path, self.root)
        self.data['quests'].reverse()
        self.data['entities'].reverse()
        path.write_text(json.dumps(self.data))
        self.assertEqual(c.load(path, self.root).revision, first.revision)
        first.records['quests'].clear()
        self.assertEqual(len(first.records['quests']), 12)
        self.quest(788)['title']['value'] = 'New source revision'
        self.pin_fixture()
        path.write_text(json.dumps(self.data))
        self.assertNotEqual(c.load(path, self.root).revision, first.revision)

    def test_duplicate_json_keys_and_nonfinite(self):
        path = self.root / 'bad.json'
        for text in ('{"schema_version":1,"schema_version":1}', '{"v":NaN}'):
            path.write_text(text)
            with self.assertRaises(c.CatalogueError):
                c.load(path, self.root)

    def test_malformed_field_types_are_diagnostics(self):
        # Exercise typed boundaries independently of source pinning.
        paths = [
            '/quests/0/min_level', '/quests/0/required_races', '/quests/0/metadata',
            '/quests/0/prerequisites', '/quests/0/objectives/0/count',
            '/quests/0/objectives/0/creatures', '/quests/0/objectives/0/source_mode',
            '/profiles/0/min_level', '/profiles/0/class_mask', '/profiles/0/race_mask',
            '/external_quests/0/required_classes', '/entities/0/locations',
        ]
        for path in paths:
            for value in ([], {}, 'invalid', True):
                if value == [] and path.endswith(('/prerequisites', '/locations')):
                    continue  # Known empty collections are valid.
                with self.subTest(path=path, value=value):
                    self.data = copy.deepcopy(self.production)
                    c.pointer(self.data, path)['value'] = value
                    self.pin_fixture()
                    with self.assertRaises(c.CatalogueError):
                        c.validate(self.data, self.root)

    def test_tsv_missing_enrichment_and_duplicate_records(self):
        path = self.root / 'fixture.tsv'
        line = next(line for line in (ROOT / b.INPUTS['enriched'][0]).read_text().splitlines() if line.startswith('Q\t788\t'))
        path.write_text('# wow-internal questdb runtime catalog v2\n' + line + '\n')
        self.assertIsNone(c.read_quest_tsv(path)['788']['prerequisites'])
        path.write_text(path.read_text() + line + '\n')
        with self.assertRaisesRegex(ValueError, 'duplicate Q'):
            c.read_quest_tsv(path)


if __name__ == '__main__':
    unittest.main()
