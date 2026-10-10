#!/usr/bin/env python3
"""Versioned offline quest/profile contract. No runtime or database connection.

A successful load proves internal consistency and pinned artifact provenance,
not that an artifact matches a running server or that a step can execute.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
STATUSES = {'source-backed', 'derived', 'runtime-backed', 'manually-curated', 'unknown'}
STEP_TYPES = {'Pickup', 'MoveToObjective', 'KillObjective', 'ItemObjective', 'TurnIn', 'GrindFallback'}
OBJECTIVE_TYPES = {'KillObjective', 'ItemObjective', 'UseItemOnUnit'}
REWARDS = {'money_copper', 'choice_items', 'guaranteed_items', 'xp', 'spells', 'reputation'}
METADATA = {'method', 'flags', 'special_flags', 'exclusive_group', 'required_skill',
            'required_skill_value', 'start_script', 'complete_script',
            'reputation_objective', 'time_limit', 'money_copper', 'breadcrumb'}


@dataclass(frozen=True, order=True)
class Issue:
    path: str
    code: str
    message: str


class CatalogueError(ValueError):
    def __init__(self, issues):
        self.issues = tuple(sorted(set(issues)))
        super().__init__('\n'.join(f'{i.code} {i.path}: {i.message}' for i in self.issues))


def canonical(value):
    return json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False,
                      separators=(',', ':'))


def digest(data: bytes):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'duplicate JSON key {key}')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def pointer(document, path):
    if path == '':
        return document
    if not isinstance(path, str) or not path.startswith('/'):
        raise ValueError('expected JSON pointer')
    for part in path[1:].split('/'):
        part = part.replace('~1', '/').replace('~0', '~')
        document = document[int(part)] if isinstance(document, list) else document[part]
    return document


def read_quest_tsv(path):
    """Expose Q/M/N/R evidence by quest ID; keep signed AND/OR relationships.

    Only these records are used here. Other legacy rows remain owned by QuestDB.
    No objective semantics are inferred from the TSV's primary objective view.
    """
    rows = {}
    lines = Path(path).read_text().splitlines()
    if not lines or lines[0] != '# wow-internal questdb runtime catalog v2':
        raise ValueError('unsupported quest TSV version')
    q_fields = {2: 'title', 3: 'min_level', 5: 'giver', 6: 'turnin',
                20: 'quest_level', 21: 'prev_quest_id', 22: 'next_in_chain',
                23: 'breadcrumb_for_quest_id'}
    m_fields = ['required_races', 'required_classes', 'max_level', 'zone_or_sort',
                'required_condition', 'next_quest_id']
    n_fields = ['method', 'flags', 'special_flags', 'exclusive_group', 'required_skill',
                'required_skill_value', 'start_script', 'complete_script',
                'reputation_objective', 'time_limit', 'money_copper', 'breadcrumb']
    seen = set()
    for line in lines:
        cells = line.split('\t')
        tag = cells[0]
        if tag not in {'Q', 'M', 'N', 'R'}:
            continue
        qid = str(int(cells[1]))
        record = rows.setdefault(qid, {'id': int(qid), 'prerequisites': []})
        if tag != 'R':
            if (tag, qid) in seen:
                raise ValueError(f'duplicate {tag} record for {qid}')
            seen.add((tag, qid))
        if tag == 'Q':
            if len(cells) != 27:
                raise ValueError('unsupported Q shape')
            record.update({name: cells[i] if name == 'title' else int(cells[i])
                           for i, name in q_fields.items()})
        elif tag == 'M':
            if len(cells) != 8:
                raise ValueError('unsupported M shape')
            record.update(zip(m_fields, map(int, cells[2:])))
        elif tag == 'N':
            if len(cells) != 16:
                raise ValueError('unsupported N shape')
            record['metadata'] = dict(zip(n_fields,
                (None if v == '-' else int(v) for v in cells[2:14])))
            record['giver_type'], record['turnin_type'] = cells[14:16]
        elif cells[2] in {'active', 'rewarded'}:
            if len(cells) < 5 or cells[3] not in {'resolved', 'unresolved'}:
                raise ValueError('unsupported R prerequisite shape')
            record['prerequisites'].append({'state': cells[2],
                'resolved': cells[3] == 'resolved', 'all_of': list(map(int, cells[4:]))})
    for qid, record in rows.items():
        if ('Q', qid) not in seen:
            raise ValueError(f'orphan metadata {qid}')
        # Missing enrichment cannot prove an empty prerequisite set.
        if ('N', qid) not in seen:
            record['prerequisites'] = None
        else:
            for field in ('giver', 'turnin'):
                record[field + 's'] = [{'source_type': record[field + '_type'], 'entry': record[field]}]
    return rows


class Check:
    def __init__(self):
        self.issues = []

    def fail(self, path, code, message):
        self.issues.append(Issue(path, code, message))

    def require(self, condition, path, code, message):
        if not condition:
            self.fail(path, code, message)

    def shape(self, value, keys, path):
        self.require(isinstance(value, dict) and set(value) == set(keys), path,
                     'schema', 'expected fields: ' + ', '.join(sorted(keys)))
        return isinstance(value, dict) and set(value) == set(keys)

    def finish(self):
        if self.issues:
            raise CatalogueError(self.issues)


def integer(value, low=0, high=2**31-1):
    return type(value) is int and low <= value <= high


def load_sources(sources, root, check):
    documents = {}
    if not isinstance(sources, dict) or not sources:
        check.fail('/sources', 'source', 'nonempty source manifest required')
        return documents
    for name, source in sorted(sources.items()):
        path = '/sources/' + name
        if not check.shape(source, {'path', 'sha256', 'format', 'description'}, path):
            continue
        try:
            relative = Path(source['path'])
            target = (root / relative).resolve()
            if relative.is_absolute() or not target.is_relative_to(root.resolve()):
                raise ValueError('source path must stay within project root')
            raw = target.read_bytes()
            if digest(raw) != source['sha256']:
                raise ValueError('source SHA256 changed; regenerate and review catalogue')
            if source['format'] == 'json':
                documents[name] = read_json(target)
            elif source['format'] == 'quest-tsv-v2':
                documents[name] = read_quest_tsv(target)
            else:
                raise ValueError('unsupported source format')
        except (OSError, ValueError, TypeError, KeyError, IndexError) as error:
            check.fail(path, 'source', str(error))
    return documents


def validate_fact(fact, path, documents, check):
    if not check.shape(fact, {'value', 'status', 'refs', 'reason', 'transform'}, path):
        return
    status, refs, value = fact['status'], fact['refs'], fact['value']
    check.require(isinstance(status, str) and status in STATUSES, path, 'provenance', 'invalid status')
    check.require(isinstance(fact['reason'], str) and bool(fact['reason'].strip()),
                  path, 'provenance', 'reason is required')
    if not isinstance(refs, list):
        check.fail(path, 'provenance', 'refs must be an array')
        return
    if status == 'unknown':
        check.require(value is None and not refs and fact['transform'] == 'unknown',
                      path, 'provenance', 'unknown requires null, no refs, unknown transform')
        return
    check.require(value is not None and bool(refs), path, 'provenance', 'known value requires evidence')
    resolved = []
    for ref in refs:
        if not check.shape(ref, {'source', 'pointer'}, path + '/refs'):
            continue
        try:
            resolved.append(pointer(documents[ref['source']], ref['pointer']))
        except (KeyError, ValueError, IndexError, TypeError) as error:
            check.fail(path, 'provenance', f'unresolved source pointer: {error}')
    transform = fact['transform']
    if transform == 'direct':
        check.require(bool(resolved) and all(canonical(v) == canonical(value) for v in resolved),
                      path, 'source_conflict', 'value differs from cited source(s)')
    elif transform == 'collect':
        check.require(status == 'derived' and canonical(resolved) == canonical(value),
                      path, 'source_conflict', 'collect must equal the ordered referenced values')
    elif transform == 'relation_sets':
        def keys(relations):
            if not isinstance(relations, list) or not all(isinstance(r, dict) and
                    'source_type' in r and 'entry' in r for r in relations):
                return None
            return [f"{r['source_type']}:{r['entry']}" for r in relations]
        check.require(status == 'derived' and bool(resolved) and all(keys(r) == value for r in resolved),
                      path, 'actor_conflict', 'relation sets disagree with the cited actor sources')
    elif transform == 'unique_sorted':
        try:
            expected = sorted(set(resolved))
        except TypeError:
            expected = None
        check.require(status == 'derived' and canonical(expected) == canonical(value),
                      path, 'source_conflict', 'unique_sorted does not match references')
    else:
        check.fail(path, 'provenance', 'unsupported transform')


def walk_facts(value, path, documents, check):
    if isinstance(value, dict):
        if 'value' in value or 'status' in value:
            validate_fact(value, path, documents, check)
        else:
            for key, child in value.items():
                walk_facts(child, path + '/' + key, documents, check)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            walk_facts(child, path + '/' + str(index), documents, check)


def fact_value(record, key, path, check):
    fact = record.get(key)
    if not isinstance(fact, dict) or set(fact) != {'value', 'status', 'refs', 'reason', 'transform'}:
        check.fail(path + '/' + key, 'missing_provenance', 'expected a field with provenance')
        return None
    return fact['value']


def validate(document, root):
    check = Check()
    try:
        canonical(document)
    except (TypeError, ValueError) as error:
        raise CatalogueError([Issue('', 'json', str(error))]) from error
    if not check.shape(document, {'schema_version', 'sources', 'entities', 'quests',
                                  'external_quests', 'profiles'}, ''):
        check.finish()
    check.require(type(document['schema_version']) is int and document['schema_version'] == SCHEMA_VERSION,
                  '/schema_version', 'version', 'unsupported catalogue schema version')
    sources = load_sources(document['sources'], Path(root), check)
    check.finish()
    for key in ('entities', 'quests', 'external_quests', 'profiles'):
        check.require(isinstance(document[key], list), '/' + key, 'schema', 'expected array')
        walk_facts(document[key], '/' + key, sources, check)
    check.finish()

    entities, quests, external, profiles = {}, {}, {}, {}
    entity_fields = {'key', 'id', 'kind', 'name', 'locations', 'identity_scope'}
    quest_fields = {'id', 'title', 'min_level', 'max_level', 'quest_level', 'required_races',
        'required_classes', 'required_condition', 'zone_or_sort', 'patch', 'source_item_id',
        'source_item_count', 'source_spell', 'givers', 'turnins', 'prev_quest_id',
        'next_quest_id', 'next_in_chain', 'breadcrumb_for_quest_id', 'prerequisites',
        'metadata', 'rewards', 'objectives'}
    objective_fields = {'id', 'kind', 'count', 'item_id', 'creatures', 'gameobjects',
                        'slot', 'spell_id', 'source_mode'}
    profile_fields = {'id', 'min_level', 'max_level', 'race_mask', 'class_mask', 'quest_ids', 'steps'}
    for index, entity in enumerate(document['entities']):
        path = f'/entities/{index}'
        if not check.shape(entity, entity_fields, path):
            continue
        v = {k: fact_value(entity, k, path, check) for k in entity_fields}
        if not isinstance(v['key'], str) or v['key'] in entities:
            check.fail(path, 'duplicate_entity', 'entity key missing or duplicated')
            continue
        entities[v['key']] = v
        check.require(integer(v['id'], 1) and v['kind'] in ('creature', 'gameobject', 'item')
                      and v['key'] == f"{v['kind']}:{v['id']}", path, 'entity', 'invalid entity identity')
        check.require(v['identity_scope'] in ('reference-only', 'named-export-record'), path,
                      'entity', 'identity scope required; reference-only is not a template proof')
        check.require(v['name'] is None or isinstance(v['name'], str), path, 'entity', 'invalid name')
        locations = v['locations']
        check.require(locations is None or isinstance(locations, list), path, 'location', 'invalid locations')
        for loc in locations if isinstance(locations, list) else []:
            check.require(isinstance(loc, dict) and all(k in loc for k in ('map_id', 'x', 'y', 'z'))
                and integer(loc['map_id']) and all(type(loc[k]) in (int, float) and math.isfinite(loc[k])
                for k in ('x', 'y', 'z')) and loc.get('entry', v['id']) == v['id'],
                path, 'location', 'invalid map/XYZ/entity association')

    for index, record in enumerate(document['external_quests']):
        path = f'/external_quests/{index}'
        if not check.shape(record, {'id', 'title', 'required_classes', 'required_races', 'reason'}, path):
            continue
        v = {k: fact_value(record, k, path, check) for k in record}
        check.require(isinstance(v['title'], str) and bool(v['title']) and
                      isinstance(v['reason'], str) and bool(v['reason']),
                      path, 'quest', 'external reference needs a title and boundary reason')
        for field, mask in [('required_races', 0xff), ('required_classes', 0x5df)]:
            check.require(integer(v[field]) and v[field] & ~mask == 0,
                          path, 'restriction', f'invalid {field}')
        if not integer(v['id'], 1) or v['id'] in external:
            check.fail(path, 'quest_id', 'invalid or duplicate external quest ID')
        else:
            external[v['id']] = v

    for index, quest in enumerate(document['quests']):
        path = f'/quests/{index}'
        if not check.shape(quest, quest_fields, path):
            continue
        v = {k: fact_value(quest, k, path, check) for k in quest_fields - {'objectives', 'rewards'}}
        if not integer(v['id'], 1) or v['id'] in quests or v['id'] in external:
            check.fail(path, 'quest_id', 'invalid or duplicate quest ID')
            continue
        quests[v['id']] = v
        v['objectives'] = {}
        check.require(isinstance(v['title'], str) and bool(v['title']), path, 'quest', 'title required')
        check.require(integer(v['min_level'], 1, 60) and integer(v['max_level'], 0, 60)
            and (v['max_level'] == 0 or v['max_level'] >= v['min_level']), path, 'level', 'invalid level band')
        for field, mask in [('required_races', 0xff), ('required_classes', 0x5df)]:
            check.require(integer(v[field]) and v[field] & ~mask == 0, path, 'restriction', f'invalid {field}')
        for field in ('required_condition', 'patch', 'source_item_id', 'source_item_count', 'source_spell'):
            check.require(integer(v[field]), path + '/' + field, 'quest', 'expected nonnegative integer')
        check.require(integer(v['quest_level'], -1, 60) and integer(v['zone_or_sort'], -10000),
                      path, 'quest', 'invalid quest level or zone/sort')
        for field in ('prev_quest_id', 'next_quest_id', 'next_in_chain', 'breadcrumb_for_quest_id'):
            check.require(integer(v[field], -2**31), path + '/' + field, 'relationship', 'expected signed ID')
        for field in ('givers', 'turnins'):
            refs = v[field]
            check.require(isinstance(refs, list) and bool(refs), path, 'npc_reference', f'{field} required')
            if isinstance(refs, list):
                check.require(len(refs) == len(set(map(canonical, refs))), path, 'npc_reference', 'duplicate relation')
                for ref in refs:
                    check.require(isinstance(ref, str) and ref in entities and
                        entities[ref]['kind'] in ('creature', 'gameobject'), path, 'npc_reference', f'broken {field}: {ref}')
        if v['source_item_id']:
            check.require(f"item:{v['source_item_id']}" in entities, path, 'item_reference', 'missing source item')
        if v['metadata'] is not None and check.shape(v['metadata'], METADATA, path + '/metadata'):
            for field, value in v['metadata'].items():
                check.require(value is None or integer(value, -2**31), path + '/metadata/' + field,
                              'metadata', 'expected source integer or unknown')
        if check.shape(quest['rewards'], REWARDS, path + '/rewards'):
            for key in REWARDS:
                reward = fact_value(quest['rewards'], key, path + '/rewards', check)
                if reward is None:
                    continue
                if key in ('money_copper', 'xp'):
                    check.require(integer(reward, -2**31 if key == 'money_copper' else 0), path,
                                  'reward', 'invalid money/XP')
                elif key in ('choice_items', 'guaranteed_items'):
                    check.require(isinstance(reward, list), path, 'reward', 'expected reward item array')
                    slots = set()
                    for item in reward if isinstance(reward, list) else []:
                        if check.shape(item, {'slot', 'item_id', 'count'}, path + '/rewards/' + key):
                            check.require(integer(item['slot'], 1, 6 if key == 'choice_items' else 4) and
                                integer(item['item_id'], 1) and integer(item['count'], 1) and
                                f"item:{item['item_id']}" in entities, path, 'item_reference', 'invalid reward item')
                            check.require(canonical(item['slot']) not in slots, path, 'reward', 'duplicate reward slot')
                            slots.add(canonical(item['slot']))
                elif key == 'spells':
                    check.require(isinstance(reward, list) and all(isinstance(s, dict) and
                        set(s) == {'spell_id', 'mode'} and integer(s['spell_id'], 1) and
                        s['mode'] in ('learn', 'cast') for s in reward),
                        path, 'reward', 'invalid spell ID or learn/cast mode')
                else:
                    check.require(isinstance(reward, list) and all(isinstance(r, dict) and
                        set(r) == {'slot', 'faction_id', 'value'} and integer(r['slot'], 1, 5) and integer(r['faction_id'], 1) and
                        integer(r['value'], -2**31) for r in reward), path, 'reward', 'invalid reputation')
        if not isinstance(quest['objectives'], list):
            check.fail(path, 'schema', 'objectives must be an array')
            continue
        for oi, objective in enumerate(quest['objectives']):
            opath = path + f'/objectives/{oi}'
            if not check.shape(objective, objective_fields, opath):
                continue
            o = {k: fact_value(objective, k, opath, check) for k in objective_fields}
            if not isinstance(o['id'], str) or not o['id'] or o['id'] in v['objectives']:
                check.fail(opath, 'objective', 'invalid or duplicate objective ID')
                continue
            v['objectives'][o['id']] = o
            check.require(isinstance(o['kind'], str) and o['kind'] in OBJECTIVE_TYPES,
                          opath, 'unsupported_objective', 'unsupported objective type')
            check.require(integer(o['count'], 1) and integer(o['slot'], 1, 4) and integer(o['spell_id']),
                          opath, 'objective', 'invalid count, slot or spell ID')
            for field, kind in [('creatures', 'creature'), ('gameobjects', 'gameobject')]:
                refs = o[field]
                check.require(isinstance(refs, list), opath, 'entity_reference', 'expected entity list')
                for entry in refs if isinstance(refs, list) else []:
                    check.require(integer(entry, 1) and f'{kind}:{entry}' in entities,
                                  opath, kind + '_reference', f'broken {kind} {entry}')
            check.require(integer(o['item_id']), opath, 'item_reference', 'invalid item ID')
            if o['item_id']:
                check.require(f"item:{o['item_id']}" in entities, opath, 'item_reference', 'broken item reference')
            mode = o['source_mode']
            check.require(mode in ('kill', 'loot', 'world-object', 'quest-supplied', 'use-item'),
                          opath, 'objective', 'unsupported acquisition mode')
            check.require((o['kind'] != 'KillObjective' or (mode == 'kill' and o['creatures'] and not o['item_id']))
                and (o['kind'] != 'ItemObjective' or (o['item_id'] and mode in ('loot', 'world-object', 'quest-supplied')))
                and (o['kind'] != 'UseItemOnUnit' or (mode == 'use-item' and o['item_id'] and o['creatures'])),
                opath, 'objective', 'type/source mismatch')
            check.require(mode != 'loot' or bool(o['creatures']), opath, 'creature_reference', 'loot source missing')
            check.require(mode != 'world-object' or bool(o['gameobjects']), opath, 'gameobject_reference', 'world source missing')
            check.require(mode != 'quest-supplied' or (o['item_id'] == v['source_item_id'] and
                integer(v['source_item_count'], o['count'] if integer(o['count'], 1) else 1)),
                opath, 'objective', 'supplied item/count mismatch')
    check.finish()

    all_quests = set(quests) | set(external)
    dependencies = {}
    for qid, q in sorted(quests.items()):
        path = f'/quests/id={qid}'
        for field in ('prev_quest_id', 'next_quest_id', 'next_in_chain', 'breadcrumb_for_quest_id'):
            ref = abs(q[field])
            check.require(not ref or (ref in all_quests and ref != qid), path + '/' + field,
                          'quest_reference', f'missing/self quest reference {ref}')
        clauses = q['prerequisites']
        if clauses is None:
            continue
        check.require(isinstance(clauses, list), path, 'prerequisite', 'expected OR clauses or unknown')
        dependencies[qid] = []
        for clause in clauses if isinstance(clauses, list) else []:
            if not check.shape(clause, {'state', 'resolved', 'all_of'}, path + '/prerequisites'):
                continue
            members = clause['all_of']
            check.require(clause['state'] in ('active', 'rewarded') and type(clause['resolved']) is bool,
                          path, 'prerequisite', 'invalid prerequisite state')
            check.require(isinstance(members, list) and bool(members), path, 'prerequisite', 'empty AND group')
            if isinstance(members, list):
                check.require(all(integer(m, 1) and m in all_quests and m != qid for m in members)
                    and len(set(map(canonical, members))) == len(members), path,
                    'prerequisite_reference', 'missing, self or duplicate prerequisite')
                if all(integer(m, 1) for m in members):
                    dependencies[qid].append(set(members))
        if isinstance(clauses, list) and any(isinstance(clause, dict) and
                clause.get('resolved') is False for clause in clauses):
            dependencies.pop(qid, None)  # Unknown group expansion cannot prove impossibility.
    check.finish()
    def has_prerequisite(q, entry, state):
        return q['prerequisites'] is None or any(entry in clause['all_of'] and clause['state'] == state
                                                for clause in q['prerequisites'])
    for qid, q in sorted(quests.items()):
        if q['prev_quest_id']:
            prev = q['prev_quest_id']
            check.require(has_prerequisite(q, abs(prev), 'active' if prev < 0 else 'rewarded'),
                          f'/quests/id={qid}', 'relationship_conflict', 'raw previous quest missing from alternatives')
        if q['next_quest_id'] and abs(q['next_quest_id']) in quests:
            nxt = q['next_quest_id']
            check.require(has_prerequisite(quests[abs(nxt)], qid, 'active' if nxt < 0 else 'rewarded'),
                          f'/quests/id={qid}', 'relationship_conflict', 'reverse next quest missing from alternatives')
    # OR alternatives are not flattened to AND. External/unknown prerequisites
    # are potential roots, not proven eligibility or completion.
    reachable = set(external) | (set(quests) - set(dependencies))
    while True:
        expanded = reachable | {qid for qid, clauses in dependencies.items()
            if not clauses or any(clause <= reachable for clause in clauses)}
        if expanded == reachable:
            break
        reachable = expanded
    for qid in sorted(set(quests) - reachable):
        check.fail(f'/quests/id={qid}/prerequisites', 'prerequisite_cycle', 'no possible prerequisite alternative reaches a root')

    for index, profile in enumerate(document['profiles']):
        path = f'/profiles/{index}'
        if not check.shape(profile, profile_fields, path):
            continue
        p = {k: fact_value(profile, k, path, check) for k in profile_fields - {'steps'}}
        if not isinstance(p['id'], str) or not p['id'] or p['id'] in profiles:
            check.fail(path, 'profile', 'missing or duplicate profile ID')
            continue
        profiles[p['id']] = p
        levels_valid = (integer(p['min_level'], 1, 60) and integer(p['max_level'], 1, 60)
                        and p['min_level'] <= p['max_level'])
        masks_valid = (integer(p['race_mask'], 1, 0xff) and integer(p['class_mask'], 1)
                       and p['class_mask'] & ~0x5df == 0)
        check.require(levels_valid, path, 'level', 'invalid profile level band')
        check.require(masks_valid, path, 'restriction', 'invalid profile mask')
        if not levels_valid or not masks_valid:
            continue
        ids = p['quest_ids']
        if not isinstance(ids, list) or not all(integer(q, 1) for q in ids):
            check.fail(path, 'quest_reference', 'invalid quest IDs')
            continue
        check.require(len(ids) == len(set(ids)), path, 'quest_reference', 'duplicate quest IDs')
        for qid in ids:
            check.require(qid in quests, path, 'quest_reference', f'missing executable record {qid}')
            if qid in quests:
                q = quests[qid]
                check.require(all(q[field] == 0 or q[field] & p[mask] for field, mask in
                    [('required_races', 'race_mask'), ('required_classes', 'class_mask')]) and
                    q['min_level'] <= p['max_level'] and (not q['max_level'] or q['max_level'] >= p['min_level']),
                    path, 'restriction', f'quest {qid} cannot fit profile restrictions')
        if not isinstance(profile['steps'], list) or not profile['steps']:
            check.fail(path, 'step', 'nonempty steps required')
            continue
        step_ids, signatures, pickup, turnin, objectives, moved = set(), set(), set(), set(), set(), set()
        for si, step_fact in enumerate(profile['steps']):
            spath = path + f'/steps/{si}'
            step = fact_value({'step': step_fact}, 'step', spath, check)
            if not isinstance(step, dict) or not isinstance(step.get('type'), str) or step['type'] not in STEP_TYPES:
                check.fail(spath, 'unsupported_step', 'step cannot be represented by schema v1')
                continue
            kind = step['type']
            keys = {'id', 'type'} | ({'targets', 'location'} if kind == 'GrindFallback' else {'quest_id'})
            keys |= {'npc'} if kind in ('Pickup', 'TurnIn') else set()
            keys |= {'objective'} if kind in ('MoveToObjective', 'KillObjective', 'ItemObjective') else set()
            if not check.shape(step, keys, spath):
                continue
            sid = step['id']
            if not isinstance(sid, str) or not sid or sid in step_ids:
                check.fail(spath, 'duplicate_step', 'missing or duplicate step ID')
            else:
                step_ids.add(sid)
            signature = canonical({k: v for k, v in step.items() if k != 'id'})
            check.require(signature not in signatures, spath, 'duplicate_step', 'duplicate semantic step')
            signatures.add(signature)
            if kind == 'GrindFallback':
                check.require(isinstance(step['targets'], list) and bool(step['targets']) and
                    all(integer(e, 1) and f'creature:{e}' in entities for e in step['targets']),
                    spath, 'creature_reference', 'invalid grind targets')
                loc = step['location']
                check.require(isinstance(loc, str) and loc in entities and bool(entities[loc]['locations']),
                              spath, 'location_reference', 'missing source location')
                continue
            qid = step['quest_id']
            if not integer(qid, 1) or qid not in quests or qid not in ids:
                check.fail(spath, 'quest_reference', 'missing or undeclared quest')
                continue
            q = quests[qid]
            if kind in ('Pickup', 'TurnIn'):
                check.require(step['npc'] in q['givers' if kind == 'Pickup' else 'turnins'],
                              spath, 'actor_conflict', 'actor disagrees with quest relation')
                if kind == 'Pickup':
                    check.require(qid not in pickup, spath, 'duplicate_step', 'quest already has a pickup step')
                    pickup.add(qid)
                else:
                    check.require(qid not in turnin, spath, 'duplicate_step', 'quest already has a turn-in step')
                    check.require(qid in pickup and all((qid, oid) in objectives for oid in q['objectives']),
                                  spath, 'step_order', 'turn-in lacks pickup/objective coverage')
                    turnin.add(qid)
            else:
                oid = step['objective']
                if not isinstance(oid, str) or oid not in q['objectives']:
                    check.fail(spath, 'objective_reference', 'missing objective')
                    continue
                o = q['objectives'][oid]
                check.require(qid in pickup and qid not in turnin, spath, 'step_order', 'objective outside pickup/turn-in')
                if kind == 'MoveToObjective':
                    references = [f'creature:{e}' for e in o['creatures']] + [f'gameobject:{e}' for e in o['gameobjects']]
                    if o['source_mode'] == 'quest-supplied':
                        references += q['turnins']
                    check.require(any(entities[e]['locations'] for e in references), spath,
                                  'location_reference', 'no source-backed objective location')
                    moved.add((qid, oid))
                else:
                    check.require(kind == o['kind'], spath, 'unsupported_objective', 'step/objective semantics differ')
                    check.require((qid, oid) in moved, spath, 'step_order', 'objective lacks MoveToObjective')
                    objectives.add((qid, oid))
        check.require(set(ids) == pickup == turnin, path, 'step_coverage', 'every profile quest needs pickup and turn-in')
    check.finish()


@dataclass(frozen=True)
class Catalogue:
    """Immutable, validated snapshot. Return copies so callers cannot mutate it."""
    content: str
    revision: str

    @property
    def records(self):
        return json.loads(self.content)


def load(path, root):
    try:
        document = read_json(path)
    except (OSError, ValueError) as error:
        raise CatalogueError([Issue('', 'json', str(error))]) from error
    validate(document, root)
    # Lists with identity are normalized; steps retain authored sequence.
    document['quests'].sort(key=lambda q: q['id']['value'])
    document['external_quests'].sort(key=lambda q: q['id']['value'])
    document['entities'].sort(key=lambda e: e['key']['value'])
    document['profiles'].sort(key=lambda p: p['id']['value'])
    content = canonical(document)
    return Catalogue(content, digest(content.encode()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalogue', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    try:
        snapshot = load(args.catalogue, args.root)
    except CatalogueError as error:
        print(error)
        return 1
    records = snapshot.records
    print(f"PASS schema={SCHEMA_VERSION} revision={snapshot.revision} quests={len(records['quests'])} "
          f"external={len(records['external_quests'])} profiles={len(records['profiles'])}")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
