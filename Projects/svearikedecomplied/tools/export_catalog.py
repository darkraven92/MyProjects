#!/usr/bin/env python3
"""Extract typed event/person tables, preserving original legacy text bytes."""
import hashlib
import json
from pathlib import Path


def c_bytes(value):
    # Hex escapes for every byte avoid both encoding guesses and C escape ambiguity.
    return '"'+''.join(f'\\x{byte:02x}' for byte in value)+'"'


def write_catalog(data):
    members = {m['number']: m for m in data['members']}
    provenance = []

    def lines(number):
        member = members[number]
        raw = bytes.fromhex(member['text_hex'])
        provenance.append({'member': number, 'stxt_resource': member['stxt_resource'],
                           'sha256': hashlib.sha256(raw).hexdigest()})
        return raw.split(b'\r')

    output = ['/* Generated from DATA.CST by tools/export_catalog.py. */']
    counts = {}
    for name, count_member, first in [('culture',100,101), ('science',200,201),
                                      ('commanders',400,401), ('events',299,300)]:
        count = int(lines(count_member)[0])
        counts[name] = count
        output.append(f'static const SrCatalogEntry sr_{name}[{count}] = {{')
        for number in range(first,first+count):
            row = lines(number)
            assert len(row) >= (9 if name == 'events' else 7), number
            periods = [int(x) for x in row[4].split(b',')]
            assert all(1 <= x <= 59 for x in periods), number
            # A mask is lossless only if no period occurs twice in the source.
            assert len(periods) == len(set(periods)), number
            mask = sum(1 << (x-1) for x in periods)
            if name == 'events':
                level, note, result, check, mini, filename = 0,row[3],row[5],row[6],row[7],row[8]
            else:
                level, note, result, check, mini, filename = int(row[3]),b'',b'-',b'-',row[5],row[6]
                assert 1 <= level <= 5
            strings = [row[1],row[2],note,result,check,mini,filename]
            output.append('    {'+f'{int(row[0])}, {level}, UINT64_C(0x{mask:x}), '+
                          ', '.join(c_bytes(x) for x in strings)+'},')
        output.append('};')
    for name, count_member, first in [('events_a',700,701), ('events_b',750,751)]:
        count = int(lines(count_member)[0])
        counts[name] = count
        output.append(f'static const SrDatedEvent sr_{name}[{count}] = {{')
        for index in range(count):
            row = lines(first+index)
            assert len(row) >= 6
            kind, check = (row[2],b'-') if name == 'events_a' else (b'event',row[2])
            output.append('    {'+f'{index+1}, {int(row[0])}, '+
                          ', '.join(c_bytes(x) for x in (row[1],kind,check,row[3],row[4],row[5]))+'},')
        output.append('};')
    Path('src/generated/catalog_data.h').write_text('\n'.join(output)+'\n', encoding='ascii')
    Path('analysis/catalog-provenance.json').write_text(json.dumps({
        'source': data['source'], 'sha256': data['sha256'], 'counts': counts,
        'text_encoding': 'Original bytes; mixed legacy encodings, not normalized.',
        'members': provenance}, indent=2)+'\n')
    print('Catalog records: '+', '.join(f'{name}={count}' for name,count in counts.items()))


if __name__ == '__main__':
    write_catalog(json.loads(Path('analysis/assets/DATA/scene.json').read_text()))
