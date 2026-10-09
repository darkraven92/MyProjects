"""Generate live-panel field geometry and font references from original SVEA."""
from pathlib import Path
import json


def main():
    scene=json.loads(Path('analysis/assets/SVEA/scene.json').read_text())
    data=json.loads(Path('analysis/assets/DATA/scene.json').read_text())
    data_fonts={f['id']:f for f in data['fonts']}
    person_names=[]
    for m in data['members']:
        if m['number'] not in (*range(101,133),*range(201,234),*range(401,428)):
            continue
        raw=bytes.fromhex(m['text_hex']);name=raw.split(b'\r')[1]
        # These 92 name rows use Windows Arial, with Western high bytes only.
        # Event rows have mixed legacy bytes and are deliberately not normalized.
        start=raw.index(b'\r')+1
        for i,s in enumerate(m['text_styles']):
            end=m['text_styles'][i+1]['start'] if i+1<len(m['text_styles']) else len(raw)
            if s['start']<start+len(name) and end>start:
                assert data_fonts[s['font_id']]['platform']==2
                assert data_fonts[s['font_id']]['name']=='Arial'
        assert not any(0x80<=c<0xa0 for c in name)
        person_names.append({'member':m['number'],'name_hex':name.hex(),'windows_name':name.decode('cp1252')})
    assert len(person_names)==92
    members={m['number']:m for m in scene['members']}
    fonts={f['id']:f['name'] for f in scene['fonts'] if f['platform']==2}
    frames=(12,48,53,69,78,83,88,98,103,112,118,132)
    live={130,131,132,133,140,141,142,143,144,145,146,147,148,149,
          180,181,183,184,185,207,208,222,223,224,225,226,227,228,229,332,333,334}
    live.update(range(304,312))
    live.update(range(319,331))
    live.update((159,161,163,165))
    live.update(range(170,178))
    live.update(range(255,263))
    records=[];lines=['/* Generated from SVEA score, STXT and Fmap; no glyph substitutions. */',
                      'static const SrFieldLayout layouts[] = {']
    for frame in frames:
        for sprite in scene['frames'][frame-1]['sprites']:
            number=sprite['member']
            if sprite['cast_lib']!=1 or number not in live:
                continue
            m=members[number];assert m['type']==3 and len(m['text_styles'])==1
            style=m['text_styles'][0];box=m['text_box']
            assert style['face']==(1 if 255<=number<=262 else 0)
            assert style['start']==0 and not any(box[k] for k in
                ('border','gutter','box_shadow','text_shadow','scroll'))
            font=fonts[style['font_id']]
            color=(style['red']>>8)<<16|(style['green']>>8)<<8|(style['blue']>>8)
            values=[frame,number,sprite['channel'],sprite['x'],sprite['y'],sprite['width'],sprite['height'],sprite['ink'],
                    style['font_id'],style['size'],style['height'],style['ascent'],box['alignment'],color]
            lines.append('    {'+','.join(map(str,values))+','+json.dumps(font)+','+str(style['face'])+'},')
            records.append({'frame':frame,'member':number,'name':m['name'],'font':font,'style':style,'sprite':sprite})
    lines.append('};')
    Path('src/generated/field_layout.h').write_text('\n'.join(lines)+'\n')
    Path('analysis/field-provenance.json').write_text(json.dumps({'source_sha256':scene['sha256'],
        'fields':records,'person_names':person_names,'data_sha256':data['sha256'],
        'glyphs':'not supplied; rasterization pending'},indent=2)+'\n')
    print(f'Prepared {len(records)} field placements for {len(live)} dynamic members.')


if __name__=='__main__':
    main()
