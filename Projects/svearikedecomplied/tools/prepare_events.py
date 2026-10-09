#!/usr/bin/env python3
"""Keep original event screens in a separately streamed bitmap bank."""
import json
from pathlib import Path
from export_assets import export, load_library


def main():
    folder=Path('analysis/assets/EVENT')
    scene=export(load_library(Path('build/libsvea_assets.so')),Path('SveaRike/EVENT.CST'),folder)
    members={m['name'].lower():m for m in scene['members'] if m['type']==1}
    output=Path('build/wasm/events'); output.mkdir(parents=True,exist_ok=True)
    rows=[]; provenance=[]
    for prefix,count in [('CultInfo.',32),('SciInfo.',33),('ComInfo.',27),
                         ('EventCInfo.',24),('EventAInfo.',22),('EventBInfo.',6)]:
        row=['        {0},']
        for n in range(1,count+1):
            m=members[(prefix+str(n)).lower()]
            assert m['width']<=641 and m['height']<=481
            row.append('        {'+','.join(map(str,[m['number'],m['width'],m['height'],
                m['reg_x']-m['left'],m['reg_y']-m['top']]))+'},')
            (output/f"{m['number']}.rgba").write_bytes((folder/m['rgba']).read_bytes())
            provenance.append({'member':m['number'],'name':m['name'],'width':m['width'],
                'height':m['height'],'rgba_sha256':m['rgba_sha256']})
        rows.append('    {\n'+'\n'.join(row)+'\n    },')
    Path('src/generated/event_images.h').write_text(
        '/* Generated from original EVENT.CST names and bitmap registration. */\n'
        'typedef struct { int member,width,height,reg_x,reg_y; } SrEventImage;\n'
        'static const SrEventImage sr_event_images[6][34] = {\n'+'\n'.join(rows)+'\n};\n')
    (output/'manifest.json').write_text(json.dumps({'source_sha256':scene['sha256'],
        'images':provenance},ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':
    main()
