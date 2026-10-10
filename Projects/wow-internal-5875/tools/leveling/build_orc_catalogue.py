#!/usr/bin/env python3
"""Reproduce the bounded Orc catalogue from tracked, offline artifacts."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from catalogue import digest, read_json, read_quest_tsv, validate, CatalogueError

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path('data/leveling/orc_starting_catalogue.json')
INPUTS = {
    'valley': ('data/questdb/generated/valley_of_trials_orc_warrior.json', 'json',
        'Tracked QuestDB export; original SQL/archive digest unavailable. Artifact authority only.'),
    'enriched': ('data/questdb/runtime/early_horde.tsv', 'quest-tsv-v2',
        'Tracked enriched QuestDB export; Q/M/N/R parsed by catalogue.py; not a live server assertion.'),
    'authoring': ('data/leveling/orc_starting_authoring.json', 'json',
        'Reviewed offline profile sequence and objective semantics; no runtime qualification.'),
}


def ref(source, pointer):
    return {'source': source, 'pointer': pointer}


def fact(value, refs, *, status='source-backed', reason='Exact value in pinned repository artifact.', transform='direct'):
    return {'value': value, 'status': status, 'refs': refs, 'reason': reason, 'transform': transform}


def unknown(reason):
    return fact(None, [], status='unknown', reason=reason, transform='unknown')


def build(root=ROOT):
    manifest = {name: {'path': path, 'format': fmt, 'description': desc,
                       'sha256': digest((root / path).read_bytes())}
                for name, (path, fmt, desc) in INPUTS.items()}
    valley = read_json(root / INPUTS['valley'][0])
    enriched = read_quest_tsv(root / INPUTS['enriched'][0])
    authoring = read_json(root / INPUTS['authoring'][0])
    def authored(value, path, reason='Authored offline policy; not a server prerequisite or runtime observation.'):
        return fact(value, [ref('authoring', path)], status='manually-curated', reason=reason)
    def direct(value, path):
        return fact(value, [ref('valley', path)])
    def exported(value, path):
        return fact(value, [ref('enriched', path)])
    def collected(values, refs):
        return fact(values, refs, status='derived', transform='unique_sorted',
                    reason='Sorted unique entity IDs from all exported resource sources; no preferred-source guess.')
    def relations(values, path, tpath):
        return fact([f"{s['source_type']}:{s['entry']}" for s in values],
            [ref('valley', path), ref('enriched', tpath)],
            status='derived', transform='relation_sets', reason='Typed entity references; JSON and TSV actors must agree.')

    result = {'schema_version': 1, 'sources': manifest, 'entities': [], 'quests': [],
              'external_quests': [], 'profiles': []}
    entities = {}
    def entity(kind, entry, id_path, source=None, source_path=None):
        key = f'{kind}:{entry}'
        if not entry:
            return
        if key in entities:
            entities[key]['id']['refs'].append(ref('valley', id_path))
            if source and 'name' in source:
                entities[key]['name']['refs'].append(ref('valley', source_path + '/name'))
            return
        policy = authoring['entities'][key]
        prefix = '/entities/' + key
        record = {k: authored(policy[k], prefix + '/' + k)
                  for k in ('key', 'kind', 'identity_scope')}
        record['id'] = direct(entry, id_path)
        record['name'] = direct(source['name'], source_path + '/name') if source and 'name' in source else unknown(
            'The export contains an item/target reference, not an item template/name proof.')
        record['locations'] = direct(source['spawns'], source_path + '/spawns') if source and 'spawns' in source else unknown(
            'No independent world spawn in this export. Do not invent an item coordinate.')
        entities[key] = record

    overlap = {'title', 'min_level', 'max_level', 'quest_level', 'required_races', 'required_classes',
               'required_condition', 'zone_or_sort', 'prev_quest_id', 'next_quest_id',
               'next_in_chain', 'breadcrumb_for_quest_id'}
    scalar_fields = overlap | {'patch', 'source_item_id', 'source_item_count', 'source_spell'}
    for qi, q in enumerate(valley['quests']):
        qid = q['quest_id']
        base, tbase = f'/quests/{qi}', f'/{qid}'
        metadata = enriched[str(qid)]
        record = {'id': direct(qid, base + '/quest_id')}
        for field in sorted(scalar_fields):
            record[field] = direct(q[field], base + '/' + field)
            if field in overlap:
                # Multiple direct citations must agree; disagreement stops export.
                record[field]['refs'].append(ref('enriched', tbase + '/' + field))
        record['givers'] = relations(q['givers'], base + '/givers', tbase + '/givers')
        record['turnins'] = relations(q['turnins'], base + '/turnins', tbase + '/turnins')
        for name in ('givers', 'turnins'):
            field = 'giver' if name == 'givers' else 'turnin'
            if len(q[name]) != 1 or q[name][0]['entry'] != metadata[field] or q[name][0]['source_type'] != metadata[field + '_type']:
                raise ValueError(f'actor conflict between exports for quest {qid}')
            for si, s in enumerate(q[name]):
                path = base + '/' + name + '/' + str(si)
                entity(s['source_type'], s['entry'], path + '/entry', s, path)
        record['prerequisites'] = exported(metadata['prerequisites'], tbase + '/prerequisites')
        record['metadata'] = exported(metadata['metadata'], tbase + '/metadata')
        record['rewards'] = {'money_copper': exported(metadata['metadata']['money_copper'], tbase + '/metadata/money_copper')}
        for field in ('choice_items', 'guaranteed_items', 'xp', 'spells', 'reputation'):
            record['rewards'][field] = unknown('Legacy tracked exports omit this reward field. Null is not zero/none; a pinned SQL reimport is required.')
        if q['source_item_id']:
            entity('item', q['source_item_id'], base + '/source_item_id')
        record['objectives'] = []
        semantics = authoring['objectives'][str(qid)]
        if len(semantics) != len(q['objectives']):
            raise ValueError(f'objective review is stale for {qid}')
        for oi, (o, policy) in enumerate(zip(q['objectives'], semantics)):
            obase = base + f'/objectives/{oi}'
            abase = f'/objectives/{qid}/{oi}'
            objective = {k: authored(policy[k], abase + '/' + k, policy['review'])
                         for k in ('id', 'kind', 'source_mode')}
            objective.update({k: direct(o[src], obase + '/' + src) for k, src in
                [('count', 'required_count'), ('slot', 'slot'), ('spell_id', 'spell_id'), ('item_id', 'item_id')]})
            creature_refs, creature_ids, go_refs, go_ids = [], [], [], []
            if o['kind'] == 'creature':
                creature_ids.append(o['target_entry']); creature_refs.append(ref('valley', obase + '/target_entry'))
                # Target name uses a different JSON key from a loot relation.
                entity('creature', o['target_entry'], obase + '/target_entry')
                e = entities[f"creature:{o['target_entry']}"]
                e['name'] = direct(o['target_name'], obase + '/target_name')
                e['locations'] = direct(o['spawns'], obase + '/spawns')
            if o['item_id']:
                entity('item', o['item_id'], obase + '/item_id')
            for li, s in enumerate(o.get('loot_sources', [])):
                spath = obase + f'/loot_sources/{li}'
                entity(s['source_type'], s['entry'], spath + '/entry', s, spath)
                ids, refs = (creature_ids, creature_refs) if s['source_type'] == 'creature' else (go_ids, go_refs)
                ids.append(s['entry']); refs.append(ref('valley', spath + '/entry'))
            for key, ids, refs in [('creatures', creature_ids, creature_refs), ('gameobjects', go_ids, go_refs)]:
                objective[key] = collected(sorted(set(ids)), refs) if refs else authored([], '/empty')
            if policy['kind'] == 'UseItemOnUnit':
                objective['item_id'] = direct(q['source_item_id'], base + '/source_item_id')
            record['objectives'].append(objective)
        result['quests'].append(record)
    result['entities'] = sorted(entities.values(), key=lambda e: e['key']['value'])
    for qid, reason in sorted(authoring['external_quests'].items(), key=lambda kv: int(kv[0])):
        record = {key: exported(enriched[qid][key], f'/{qid}/{key}')
                  for key in ('id', 'title', 'required_classes', 'required_races')}
        record['reason'] = authored(reason, '/external_quests/' + qid)
        result['external_quests'].append(record)
    for pi, profile in enumerate(authoring['profiles']):
        base = f'/profiles/{pi}'
        record = {key: authored(value, base + '/' + key) for key, value in profile.items() if key != 'steps'}
        record['steps'] = [authored(step, base + f'/steps/{si}') for si, step in enumerate(profile['steps'])]
        result['profiles'].append(record)
    result['quests'].sort(key=lambda q: q['id']['value'])
    validate(result, root)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail if committed output differs; never write.')
    args = parser.parse_args()
    try:
        content = json.dumps(build(), indent=2, sort_keys=True, ensure_ascii=False, allow_nan=False) + '\n'
        path = ROOT / OUTPUT
        if args.check:
            if path.read_text() != content:
                raise ValueError('catalogue differs; regenerate and review')
            print('PASS reproducible Orc catalogue')
        else:
            path.write_text(content)
            print(f'Wrote {OUTPUT}')
    except (CatalogueError, ValueError, OSError) as error:
        print(error)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
