#!/usr/bin/env python3
"""Offline, build-5875 class trainer catalogue. Never changes the quest database.

Server contract: ObjectMgr::LoadTrainers, Player::GetTrainerSpellState and
NPCHandler::SendTrainerSpellHelper. UI availability remains authoritative.
Only simple, single LEARN_SPELL services without a skill requirement are exported.
"""
import argparse
import csv
import sqlite3
from pathlib import Path
import questdb

TABLES = {'creature_template', 'npc_trainer', 'npc_trainer_template',
          'spell_template', 'spell_chain', 'faction_template'}


def read_source(path, patch=10, build=5875):
    old = questdb.TARGET_TABLES
    questdb.TARGET_TABLES = TABLES
    rows = {t: [] for t in TABLES}
    schemas = {}
    try:
        for stmt in questdb.statements(path):
            c = questdb.parse_create(stmt)
            if c:
                schemas[c[0]] = c[1]
                continue
            h = questdb.parse_insert_header(stmt)
            if not h:
                continue
            table, columns, values = h
            for value in questdb.parse_tuples(values):
                d = questdb.rowdict(columns or schemas[table], value)
                if int(d.get('patch', 0)) > patch:
                    continue
                if not int(d.get('buildmin', 0)) <= build <= int(d.get('buildmax', build)):
                    continue
                if 'build' in d and int(d['build']) > build:
                    continue
                rows[table].append(d)
    finally:
        questdb.TARGET_TABLES = old
    return rows


def normalize(rows):
    creatures = {}
    for d in rows['creature_template']:
        if d['entry'] not in creatures or d.get('patch', 0) > creatures[d['entry']].get('patch', 0):
            creatures[d['entry']] = d
    # SpellMgr loads the newest version <= client build, not only exact-build rows.
    spells = {}
    for d in rows['spell_template']:
        if d['entry'] not in spells or d.get('build', 0) > spells[d['entry']].get('build', 0):
            spells[d['entry']] = d
    chains = {d['spellid']: d for d in rows['spell_chain']}
    result = []
    for entry, npc in sorted(creatures.items()):
        if not int(npc['npcflags']) & 16 or npc['trainertype'] != 0 or npc['trainerclass'] not in questdb.CLASS_IDS.values():
            continue
        services = {}
        for table, key in [('npc_trainer_template', npc.get('trainerid', 0)), ('npc_trainer', entry)]:
            for d in rows[table]:
                if key and d['entry'] == key:
                    # Conflicting duplicates are excluded, not silently resolved.
                    previous = services.get(d['spell'])
                    if previous is not None and any(previous.get(k) != d.get(k) for k in
                            ('spellcost', 'reqskill', 'reqskillvalue', 'reqlevel')):
                        services[d['spell']] = {}
                    elif previous != {}:
                        services[d['spell']] = d
        for service, d in sorted(services.items()):
            learn = spells.get(service, {})
            if not d or d['reqskill'] or d['reqskillvalue'] or learn.get('effect1') != 36:
                continue
            if learn.get('effect2', 0) or learn.get('effect3', 0):
                continue
            spell = learn.get('effecttriggerspell1', 0)
            ability = spells.get(spell, {})
            if not ability.get('name') or 36 in [ability.get('effect1'), ability.get('effect2'), ability.get('effect3')]:
                continue
            if (learn.get('name'), learn.get('namesubtext', '')) != (ability['name'], ability.get('namesubtext', '')):
                continue  # UI is keyed by learning-spell identity; never guess a name alias.
            chain = chains.get(spell, {})
            # Names/ranks for prerequisites must be present to prove known state.
            prev, req = chain.get('prevspell', 0), chain.get('reqspell', 0)
            if any(p and p not in spells for p in (prev, req)):
                continue
            result.append((entry, npc['trainerclass'], service, spell, ability['name'],
                           ability.get('namesubtext', ''), chain.get('firstspell', spell),
                           chain.get('rank', 0), prev, req,
                           d['reqlevel'] or learn.get('spelllevel', 0) or ability.get('spelllevel', 0),
                           d['spellcost']))
    # Runtime exact name/rank resolution must be unambiguous per actor.
    keys = {}
    for row in result:
        keys.setdefault((row[0], row[4], row[5]), set()).add(row[3])
    result = [r for r in result if len(keys[(r[0], r[4], r[5])]) == 1]
    return result, spells, chains


def export(sql, quest_db, output, normalized):
    rows = read_source(sql)
    services, spells, chains = normalize(rows)
    # Dedicated normalized DB; caller must explicitly choose a new output.
    if normalized.exists():
        raise ValueError('refusing to overwrite existing normalized trainer database')
    db = sqlite3.connect(normalized)
    db.execute('CREATE TABLE service(entry,class_id,service_id,spell_id,name,rank_text,chain_id,rank,previous_id,required_id,level,cost)')
    db.executemany('INSERT INTO service VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', services)
    source = sqlite3.connect(f'file:{quest_db}?mode=ro', uri=True)
    ids = {r[0] for r in services}
    spawns = [r for r in source.execute('SELECT entry,guid,map_id,x,y,z FROM creature_spawn ORDER BY entry,guid') if r[0] in ids]
    db.execute('CREATE TABLE spawn(entry,guid,map_id,x,y,z)')
    db.executemany('INSERT INTO spawn VALUES (?,?,?,?,?,?)', spawns)
    db.execute('CREATE TABLE meta(key,value)')
    db.executemany('INSERT INTO meta VALUES (?,?)', [('source', str(sql)), ('build', '5875'), ('patch', '10')])
    source.close()
    with output.open('w', encoding='utf-8', newline='') as f:
        writer = csv.writer(f, delimiter='\t', lineterminator='\n', quoting=csv.QUOTE_NONE, escapechar='\\')
        writer.writerow(['# CLASS_TRAINERS_V1', 'build=5875', 'patch=10'])
        actors = {}
        for d in rows['creature_template']:
            if d['entry'] not in actors or d.get('patch', 0) > actors[d['entry']].get('patch', 0):
                actors[d['entry']] = d
        for entry in sorted(ids):
            a = actors[entry]
            writer.writerow(('N', entry, a['name'], a['faction']))
        db.execute('CREATE TABLE actor(entry PRIMARY KEY,name,faction_template)')
        db.executemany('INSERT INTO actor VALUES (?,?,?)',
                       [(e, actors[e]['name'], actors[e]['faction']) for e in sorted(ids)])
        factions = {}
        for d in rows['faction_template']:
            if d['id'] not in factions or d['build'] > factions[d['id']]['build']:
                factions[d['id']] = d
        for fid, d in sorted(factions.items()):
            writer.writerow(('F', fid, d['factionid'], d['ourmask'], d['friendlymask'], d['hostilemask'],
                             *[d['enemyfaction'+str(i)] for i in range(1, 5)]))
        db.execute('CREATE TABLE faction(id PRIMARY KEY,faction_id,our_mask,friendly_mask,hostile_mask,enemy1,enemy2,enemy3,enemy4)')
        db.executemany('INSERT INTO faction VALUES (?,?,?,?,?,?,?,?,?)',
                       [(fid, d['factionid'], d['ourmask'], d['friendlymask'], d['hostilemask'],
                         *[d['enemyfaction'+str(i)] for i in range(1, 5)]) for fid, d in sorted(factions.items())])
        for row in services:
            writer.writerow(('T',) + row)
        for row in spawns:
            writer.writerow(('P',) + row)
        # Include the full rank chain and prerequisite identities, not just sold ranks.
        needed = {r[3] for r in services} | {r[8] for r in services} | {r[9] for r in services}
        families = {r[6] for r in services}
        needed |= {s for s, c in chains.items() if c['firstspell'] in families}
        db.execute('CREATE TABLE spell_identity(id PRIMARY KEY,name,rank_text,chain_id,rank)')
        for sid in sorted(needed - {0}):
            if sid in spells:
                s, c = spells[sid], chains.get(sid, {})
                row = (sid, s['name'], s.get('namesubtext', ''), c.get('firstspell', sid), c.get('rank', 0))
                writer.writerow(('S',) + row)
                db.execute('INSERT INTO spell_identity VALUES (?,?,?,?,?)', row)
    db.commit()
    db.close()
    print(f'TRAINER CATALOGUE trainers={len(ids)} services={len(services)} spawns={len(spawns)}')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sql', type=Path, required=True)
    p.add_argument('--quest-db', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--normalized', type=Path, required=True)
    a = p.parse_args()
    export(a.sql, a.quest_db, a.out, a.normalized)
