"""Extract KRIG's literal board/range tables without executing Lingo."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path('analysis/decompiled/KRIG/casts/Internal')


def array(value):
    return '{'+','.join(array(v) if isinstance(v,list) else str(v) for v in value)+'}'


def main():
    init=ROOT/'MovieScript 1.ls'
    ranges=ROOT/'MovieScript 58.ls'
    a=init.read_text(encoding='latin1');b=ranges.read_text(encoding='latin1')
    rows=re.findall(r'  set row[1-9A-G] to (\[[^\n]+)',a)
    tables=[json.loads(row) for row in rows]
    assert len(tables)==32 and all(len(row)==16 for row in tables)
    points=json.loads(re.search(r'set gSquarePointList to (\[[^\n]+)',a)[1])
    assert len(points)==63 and all(len(p)==2 for p in points)
    mods=[json.loads(x) for x in re.findall(r'set modList to (\[[^\n]+)',b)]
    assert [len(x) for x in mods]==[4,24,4,24,48,35]
    assert mods[0]==mods[2] and mods[1]==mods[3]
    los=json.loads(re.search(r'set losList to (\[[^\n]+)',b)[1])
    assert len(los)==24 and all(len(x)<=2 for x in los)
    los_rows=[[len(row),*sum(row,[]),*([0]*(4-2*len(row)))] for row in los]
    formations=[json.loads(x) for x in re.findall(r'set pList to (\[[^\n]+)',a)]
    assert len(formations)==12 and all(len(row)==10 for row in formations)
    definitions=[('unsigned char','sr_battle_halves[16][16]',tables[:16]),
        ('unsigned char','sr_battle_cells[16][16][2]',tables[16:]),
        ('short','sr_battle_points[63][2]',points),
        *[('signed char',f'sr_battle_range{n}[{len(v)}][2]',v) for n,v in zip((1,2,3,5),(mods[0],mods[1],mods[4],mods[5]))],
        ('signed char','sr_battle_los[24][5]',los_rows),
        ('unsigned char','sr_battle_formations[12][10][2]',formations)]
    Path('src/generated/battle_rules.h').write_text('/* Generated from original KRIG MovieScripts 1 and 58. */\n'+
        '\n'.join(f'static const {kind} {name} = {array(values)};' for kind,name,values in definitions)+'\n')
    Path('analysis/battle-rules-provenance.json').write_text(json.dumps({
        'sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (init,ranges)},
        'points':points,'movement':[mods[0],mods[1]],'attacks':mods[2:],'line_of_sight':los,'formations':formations,
        'note':'Source lookup order and parity-toggle line-of-sight preserved; runtime parity unverified.'},indent=2)+'\n')
    print('Prepared 63 original board positions and four ordered attack ranges.')


if __name__=='__main__':
    main()
