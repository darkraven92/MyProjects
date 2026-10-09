"""Read original SCVW film-loop frames with the C score decoder; no script VM."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import re
import struct
from export_assets import Archive,Cast,Resource,checked,load_library


def main():
    lib=load_library(Path('build/libsvea_assets.so'))
    casts=json.loads(Path('analysis/battle-assets.json').read_text())[1:]
    loops=[];frames=[];poses=[];evidence=[];officers=[]
    script=Path('analysis/decompiled/KRIG/casts/Internal/MovieScript 87.ls')
    artillery_scripts=[script.with_name('MovieScript 75 - Artilery.ls'),script.with_name('MovieScript 77 - EnemyArtilery.ls')]
    artillery_offsets=[]
    for source in artillery_scripts:
        value=re.search(r'set offList to (\[[^\n]+)',source.read_text(encoding='latin1'))[1]
        artillery_offsets.append(json.loads(value.replace('point(','[').replace(')',']')))
    offsets={}
    for name,value in re.findall(r'set (\w+) to (\[[^\n]+)',script.read_text(encoding='latin1')):
        if 'point(' in value:offsets[name.lower()]=json.loads(value.replace('point(','[').replace(')',']'))
    for cast in casts:
        path=Path('SveaRike')/(cast['name']+'.CST');data=path.read_bytes();buf=C.create_string_buffer(data)
        a=Archive();checked(lib.sr_archive_open(C.byref(a),buf,len(data)))
        report=json.loads((Path('analysis/assets')/cast['name']/'scene.json').read_text())
        members={m['number']:m for m in report['members']};names={}
        for m in sorted(report['members'],key=lambda m:m['number']):
            names.setdefault(m['name'].lower(),m['number'])
        for m in members.values():
            if m['type']!=2:continue
            c=Cast();r=Resource();checked(lib.sr_cast(C.byref(a),m['number'],C.byref(c)))
            meta=C.string_at(c.specific,c.specific_size);assert len(meta)>=14
            top,left,bottom,right,flags=struct.unpack_from('>hhhhI',meta)
            checked(lib.sr_child(C.byref(a),c.resource,b'SCVW',C.byref(r)))
            score=data[r.offset+8:r.offset+8+r.size];state=C.create_string_buffer(1200);count=C.c_uint32()
            checked(lib.sr_score_frame(score,len(score),0,state,C.byref(count)))
            first=len(frames)
            for frame in range(1,count.value+1):
                checked(lib.sr_score_frame(score,len(score),frame,state,C.byref(count)));sprites=[]
                for ch in range(1,49):
                    raw=state.raw[48+(ch-1)*24:48+ch*24];cl,member=struct.unpack_from('>HH',raw,2)
                    if not member:continue
                    y,x,h,w=struct.unpack_from('>hhhh',raw,12)
                    assert cl in (0,65535) and member in members and members[member]['type']==1
                    assert (raw[1]&63) in (0,36)
                    assert not raw[1]&128, "Stretched film-loop sprite needs scaled rendering"
                    sprites.append([member,x,y,w,h,raw[1]&63,cl])
                assert len(sprites)==1
                frames.append(sprites[0])
            loops.append([cast['id'],m['number'],left,top,right-left,bottom-top,flags,first,count.value])
            evidence.append({'cast':cast['name'],'member':m['number'],'name':m['name'],'frames':count.value,
                'resource':r.id,'sha256':hashlib.sha256(score).hexdigest()})
        prefix=cast['name'][:3].lower();level=int(cast['name'][-1]);enemy=prefix!='swe'
        # Source offset variable prefix is "swed", whereas the cast is SWE.
        off_prefix='swed' if not enemy else prefix
        for kind,type_name in enumerate(('inf','cav')):
            base=('enemy' if enemy else '')+type_name
            for pose,suffix in ((1,'anim'),(2,'shoot')):
                diffs=offsets[f'{off_prefix}{level}{type_name}{suffix}']
                for index,direction in enumerate((2,4,5,7)):
                    poses.append([cast['id'],kind,pose,direction,names[base+suffix+str(direction)],*diffs[index]])
            for pose,suffix in ((3,'die'),(4,'hit')):
                poses.append([cast['id'],kind,pose,0,names[base+suffix],*offsets[f'{off_prefix}{type_name}{suffix}'][level-1]])
        # ArtTroop constructors 75/77 supply these offsets. Name lookup uses
        # the earliest member (SWE3 contains two different ArtShoot loops).
        off=artillery_offsets[int(enemy)]
        poses.append([cast['id'],2,2,0,names[('enemy' if enemy else '')+'artshoot'],*off[level-1]])
        if not enemy:officers.append([cast['id'],names['artofficer'],names['artofficerwalk']])
    setup_source=script.with_name('MovieScript 1.ls')
    officer_positions=[json.loads(value.replace('point(','[').replace(')',']'))
        for value in re.findall(r'set gArtOfficerPosList to (\[[^\n]+)',setup_source.read_text(encoding='latin1'))]
    assert [len(row) for row in officer_positions]==[1,2,3,4,5]
    def array(a):return '{'+','.join(array(x) if isinstance(x,list) else str(x) for x in a)+'}'
    Path('src/generated/battle_animation.h').write_text('/* Original SCVW frames and MovieScript 87 pose offsets. */\n'+
        f'static const int sr_battle_loops[{len(loops)}][9]='+array(loops)+';\n'+
        f'static const int sr_battle_loop_frames[{len(frames)}][7]='+array(frames)+';\n'+
        f'static const int sr_battle_poses[{len(poses)}][7]='+array(poses)+';\n'+
        'static const int sr_battle_officers[3][3]='+array(officers)+';\n'+
        'static const int sr_battle_officer_positions[5][5][2]='+array(officer_positions)+';\n')
    Path('analysis/battle-animation.json').write_text(json.dumps({'offset_source':str(script),
        'offset_sha256':hashlib.sha256(script.read_bytes()).hexdigest(),'loops':evidence,
        'artillery_offset_sources':[{'file':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in artillery_scripts],
        'officer_position_source':{'file':str(setup_source),'sha256':hashlib.sha256(setup_source.read_bytes()).hexdigest()},
        'note':'All supplied loops have one bitmap per frame. One SWE3 InfShoot7 frame uses cast ID zero; kept in frame metadata.'},indent=2)+'\n')
    print(f'Prepared {len(loops)} original film loops / {len(frames)} frames / {len(poses)} troop poses.')


if __name__=='__main__':main()
