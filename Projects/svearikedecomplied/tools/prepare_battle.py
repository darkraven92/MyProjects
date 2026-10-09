"""Decode and separately pack original KRIG and troop casts for browser streaming."""
from pathlib import Path
import json
import re
import struct
from export_assets import export,load_library


def main():
    lib=load_library(Path('build/libsvea_assets.so'))
    out=Path('build/wasm/battle');out.mkdir(parents=True,exist_ok=True)
    names=[(1,'KRIG')]+[(10+level,f'SWE{level}') for level in range(1,4)]
    names += [(20+country*10+level,f'{name}{level}') for country,name in enumerate(('DAN','PRU','POL','RUS')) for level in range(1,4)]
    casts=[];provenance=[];scenes={}
    for identifier,name in names:
        folder=Path('analysis/assets')/name
        scene=export(lib,Path('SveaRike')/(name+('.DIR' if name=='KRIG' else '.CST')),folder)
        scenes[name]=scene
        members=[m for m in scene['members'] if 'rgba' in m]
        offset=16+len(members)*32;records=bytearray();pixels=bytearray()
        for m in members:
            rgba=bytearray((folder/m['rgba']).read_bytes())
            # KRIG score ink 36 and troop puppet channels: background transparent.
            # Full-stage backgrounds and the splash use copy ink.
            if not (name=='KRIG' and (m['name'].startswith('Bak.') or m['number']==80 or 105<=m['number']<=108)):
                for n in range(0,len(rgba),4):
                    if rgba[n:n+3]==b'\xff\xff\xff':rgba[n+3]=0
            records+=struct.pack('<IIIiiIII',m['number'],m['width'],m['height'],
                m['reg_x']-m['left'],m['reg_y']-m['top'],offset,len(rgba),0)
            pixels+=rgba;offset+=len(rgba)
        assert offset<=16*1024*1024
        (out/f'{identifier}.pack').write_bytes(b'SRB1'+struct.pack('<III',len(members),identifier,offset)+records+pixels)
        by_name={m['name'].lower():m['number'] for m in members}
        if name!='KRIG':
            prefix='' if name.startswith('SWE') else 'enemy'
            casts.append([identifier,*[by_name[prefix+t] for t in ('inf','cav','art')]])
        provenance.append({'id':identifier,'name':name,'sha256':scene['sha256'],'images':len(members),'bytes':offset})
    scene=scenes['KRIG'];members={m['number']:m for m in scene['members']}
    static=[]
    for frame in (21,55):
        row=[]
        for s in scene['frames'][frame-1]['sprites']:
            if s['cast_lib']==1 and s['member'] in members and 'rgba' in members[s['member']] and s['channel'] not in (1,3,42) and s['member']!=90:
                row.append([s['member'],s['x'],s['y']])
        static.append(row)
    control=Path('analysis/decompiled/KRIG/casts/Internal/MovieScript 65 - troopControl.ls').read_text(encoding='latin1')
    order=json.loads(re.search(r'set sortOrder to (\[[^\n]+)',control)[1])
    assert sorted(order)==list(range(1,64))
    def array(a):return '{'+','.join(array(x) if isinstance(x,list) else str(x) for x in a)+'}'
    lines=['/* Generated from original KRIG score and troop casts. */',
        'static const int sr_battle_casts[15][4]='+array(casts)+';',
        'static const int sr_battle_sort[63]='+array(order)+';']
    for name,row in zip(('deploy','ready'),static):
        lines.append(f'static const int sr_battle_{name}_sprites[{len(row)}][3]='+array(row)+';')
    Path('src/generated/battle_scene.h').write_text('\n'.join(lines)+'\n')
    Path('analysis/battle-assets.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print(f'Prepared {len(names)} independently streamed battle image banks.')


if __name__=='__main__':main()
