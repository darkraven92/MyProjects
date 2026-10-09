#!/usr/bin/env python3
"""Offline merchant audit candidates, not verified live service claims.

VMaNGOS 5875 NPCFlags: vendor=0x4, repair=0x4000. Uses the existing SQL
parser and normalized spawn database. MerchantFrame remains authoritative.
"""
import argparse
import csv
import sqlite3
from pathlib import Path
import trainers


def normalize(rows):
    actors = {}
    for row in rows['creature_template']:
        if row['entry'] not in actors or row.get('patch', 0) > actors[row['entry']].get('patch', 0):
            actors[row['entry']] = row
    return {entry: row for entry, row in sorted(actors.items()) if int(row.get('npcflags', 0)) & 4}


def export(sql, quest_db, output, map_id=1):
    old = trainers.TABLES
    trainers.TABLES = {'creature_template', 'faction_template'}
    try:
        rows = trainers.read_source(sql)
    finally:
        trainers.TABLES = old
    actors, factions = normalize(rows), {}
    for row in rows['faction_template']:
        if row['id'] not in factions or row['build'] > factions[row['id']]['build']:
            factions[row['id']] = row
    source = sqlite3.connect(f'file:{quest_db}?mode=ro', uri=True)
    try:
        spawns = [row for row in source.execute(
            'SELECT entry,guid,map_id,x,y,z FROM creature_spawn WHERE map_id=? ORDER BY entry,guid', (map_id,))
            if row[0] in actors and actors[row[0]]['faction'] in factions]
    finally:
        source.close()
    ids = {row[0] for row in spawns}
    with output.open('w', encoding='utf-8', newline='') as file:
        writer = csv.writer(file, delimiter='\t', lineterminator='\n', quoting=csv.QUOTE_NONE)
        writer.writerow(('# SERVICE_HUBS_V1', 'build=5875', 'patch=10'))
        for entry in sorted(ids):
            actor = actors[entry]
            writer.writerow(('N', entry, actor['faction'], actor['npcflags']))
        for fid, row in sorted(factions.items()):
            writer.writerow(('F', fid, row['factionid'], row['ourmask'], row['friendlymask'], row['hostilemask'],
                             *[row['enemyfaction'+str(i)] for i in range(1, 5)]))
        for row in spawns:
            writer.writerow(('P',) + row)
    return len(ids), len(spawns)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sql', type=Path, required=True)
    parser.add_argument('--db', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--map', type=int, default=1)
    args = parser.parse_args()
    count, spawns = export(args.sql, args.db, args.out, args.map)
    print(f'SERVICE HUB CATALOGUE actors={count} spawns={spawns} services=unverified map={args.map}')
