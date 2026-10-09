#!/usr/bin/env python3
"""Extract the crossbow artwork and its actual target silhouettes, not circles."""
from pathlib import Path
import hashlib
import json
import ctypes as C
import wave
from export_assets import export, load_library, Archive, Cast, Resource, checked
from pack_menu import matte


def main():
    folder = Path('analysis/assets/ARMBORST')
    lib = load_library(Path('build/libsvea_assets.so'))
    source = Path('SveaRike/ARMBORST.DIR')
    scene = export(lib, source, folder)
    members = {m['number']: m for m in scene['members']}
    sprites = {s['channel']: s for s in scene['frames'][24]['sprites']}
    declarations, records, sources = [], [], []
    for channel in range(6, 12):
        sprite = sprites[channel]
        member = members[sprite['member']]
        assert sprite['ink'] == 8 and not sprite['stretch']
        w, h = member['width'], member['height']
        rgba = matte((folder/member['rgba']).read_bytes(), w, h)
        mask = bytearray((w*h+7)//8)
        for i in range(w*h):
            if rgba[4*i+3]:
                mask[i//8] |= 1 << (i % 8)
        name = f'sr_crossbow_mask_{channel}'
        declarations.append(f'static const unsigned char {name}[] = {{\n' +
                            '\n'.join('    '+','.join(str(b) for b in mask[i:i+24])+','
                                      for i in range(0, len(mask), 24))+'\n};')
        x = sprite['x'] - (member['reg_x']-member['left'])
        y = sprite['y'] - (member['reg_y']-member['top'])
        records.append(f'    {{{x},{y},{w},{h},{name}}},')
        sources.append({'channel': channel, 'member': member['number'],
                        'rgba_sha256': member['rgba_sha256'],
                        'mask_sha256': hashlib.sha256(mask).hexdigest()})
    Path('src/generated/crossbow_targets.h').write_text(
        '/* Generated from ARMBORST.DIR frame 25 and original BITD silhouettes.\n'
        '   Edge-connected white matte still needs original-runtime comparison. */\n'+
        '\n'.join(declarations)+'\nstatic const SrCrossbowMask sr_crossbow_targets[6] = {\n'+
        '\n'.join(records)+'\n};\n')
    Path('analysis/crossbow-provenance.json').write_text(json.dumps(
        {'source_sha256': scene['sha256'], 'targets': sources}, indent=2)+'\n')
    class Sound(C.Structure):
        _fields_ = [('samples',C.c_void_p)]+[(k,C.c_uint32) for k in
            ('frames','rate_fixed','loop_start','loop_end')]+[('channels',C.c_uint16),('bits',C.c_uint16)]
    lib.sr_sound_open.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(Sound)]
    lib.sr_sound_open.restype=C.c_char_p
    data=source.read_bytes(); buffer=C.create_string_buffer(data); archive=Archive()
    checked(lib.sr_archive_open(C.byref(archive),buffer,len(data)))
    output=Path('build/wasm/sounds'); output.mkdir(exist_ok=True,parents=True)
    sounds=[]
    for number in (31,32,33):
        cast=Cast(); resource=Resource(); sound=Sound()
        checked(lib.sr_cast(C.byref(archive),number,C.byref(cast)))
        checked(lib.sr_child(C.byref(archive),cast.resource,b'snd ',C.byref(resource)))
        checked(lib.sr_sound_open(C.byref(buffer,resource.offset+8),resource.size,C.byref(sound)))
        assert sound.rate_fixed % 65536 == 0, 'WAV requires an integer sample rate'
        pcm=C.string_at(sound.samples,sound.frames*sound.channels*(sound.bits//8))
        if sound.bits==16:
            swapped=bytearray(pcm); swapped[0::2]=pcm[1::2]; swapped[1::2]=pcm[0::2]; pcm=bytes(swapped)
        filename=f'crossbow-{number}.wav'
        with wave.open(str(output/filename),'wb') as wav:
            wav.setnchannels(sound.channels); wav.setsampwidth(sound.bits//8)
            wav.setframerate(sound.rate_fixed//65536); wav.writeframes(pcm)
        sounds.append({'member':number,'name':members[number]['name'],'file':filename,
                       'frames':sound.frames,'rate':sound.rate_fixed//65536,'channels':sound.channels,
                       'bits':sound.bits,'pcm_sha256':hashlib.sha256(pcm).hexdigest(),
                       'source_resource':resource.id})
    (output/'crossbow.json').write_text(json.dumps(sounds,indent=2)+'\n')


if __name__ == '__main__':
    main()
