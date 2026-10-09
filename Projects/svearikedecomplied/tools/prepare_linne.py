"""Recover LINNE's fixed card layout, registration, original images and PCM."""
from pathlib import Path
import ctypes as C
import json
import struct
from export_assets import Archive, Resource, checked, export, load_library
from export_sounds import export_sounds


def main():
    lib = load_library(Path('build/libsvea_assets.so'))
    source = Path('SveaRike/LINNE.DIR')
    scene = export(lib, source, Path('analysis/assets/LINNE'))
    members = {m['number']: m for m in scene['members']}
    sprites = {s['channel']: s for s in scene['frames'][14]['sprites']}
    declarations = ['/* Generated from LINNE.DIR frame 15. Pair IDs are score channels, not cast order. */']
    for name,first in [('complete',11),('flowers',19),('names',26),('flower_high',33),('name_high',40)]:
        ids = [sprites[ch]['member'] for ch in range(first,first+6)]
        assert all(sprites[ch]['x']==320 and sprites[ch]['y']==240 and sprites[ch]['ink']==8
                   for ch in range(first,first+6))
        declarations.append(f'static const int sr_linne_{name}[6] = {{'+','.join(map(str,ids))+'};')
    Path('src/generated/linne_layout.h').write_text('\n'.join(declarations)+'\n')
    export_sounds(lib,source,members,(29,30,31,32),'linne')
    data=source.read_bytes();buffer=C.create_string_buffer(data);archive=Archive();resource=Resource()
    checked(lib.sr_archive_open(C.byref(archive),buffer,len(data)))
    checked(lib.sr_find(C.byref(archive),b'VWSC',C.byref(resource)))
    score=data[resource.offset+8:resource.offset+8+resource.size]
    state=C.create_string_buffer(1200);frames=C.c_uint32();channels={}
    for frame in (1,10,14,15,24,25,26):
        checked(lib.sr_score_frame(score,len(score),frame,state,C.byref(frames)))
        sounds=list(struct.unpack_from('>HHHH',state.raw,4))
        assert sounds==([0,0,1,30] if frame in (10,14,15) else [0,0,0,0])
        channels[frame]=sounds
    sounds=json.loads(Path('build/wasm/sounds/linne.json').read_text())
    assert [s['cast_flags'] for s in sounds]==[16,0,16,16]
    assert all(s['loop_start']==0 and s['loop_end']==s['frames'] for s in sounds)
    Path('analysis/linne-provenance.json').write_text(json.dumps({'source_sha256':scene['sha256'],
        'watch_frame':10,'test_frame':15,'script_watch_ticks':300,'script_test_ticks':1800,
        'score_sounds_cast_member_pairs':channels,
        'result_script':26,'result_rule':'attempt count only, even on timeout'},indent=2)+'\n')


if __name__=='__main__':
    main()
