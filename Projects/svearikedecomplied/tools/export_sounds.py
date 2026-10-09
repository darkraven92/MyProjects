"""Export original inline PCM through the C decoder, without resampling."""
from pathlib import Path
import ctypes as C
import hashlib
import json
import struct
import wave
from export_assets import Archive, Cast, Resource, checked


def export_sounds(lib, source, members, numbers, prefix):
    class Sound(C.Structure):
        _fields_ = [('samples', C.c_void_p)]+[(k, C.c_uint32) for k in
            ('frames', 'rate_fixed', 'loop_start', 'loop_end')]+[('channels', C.c_uint16), ('bits', C.c_uint16)]
    lib.sr_sound_open.argtypes = [C.c_void_p, C.c_size_t, C.POINTER(Sound)]
    lib.sr_sound_open.restype = C.c_char_p
    lib.sr_archive_resource.argtypes = [C.POINTER(Archive), C.c_uint32, C.POINTER(Resource)]
    lib.sr_archive_resource.restype = C.c_char_p
    data = source.read_bytes(); buffer = C.create_string_buffer(data); archive = Archive()
    checked(lib.sr_archive_open(C.byref(archive), buffer, len(data)))
    output = Path('build/wasm/sounds'); output.mkdir(exist_ok=True, parents=True)
    sounds = []
    for number in numbers:
        cast = Cast(); resource = Resource(); sound = Sound()
        checked(lib.sr_cast(C.byref(archive), number, C.byref(cast)))
        checked(lib.sr_archive_resource(C.byref(archive), cast.resource, C.byref(resource)))
        cast_data = data[resource.offset+8:resource.offset+8+resource.size]
        assert cast.type == 6 and struct.unpack_from('>I', cast_data, 4)[0] >= 20
        # Director 5 CASt info: offset, two reserved words, flags, script ID.
        flags = struct.unpack_from('>I', cast_data, 24)[0]
        checked(lib.sr_child(C.byref(archive), cast.resource, b'snd ', C.byref(resource)))
        checked(lib.sr_sound_open(C.byref(buffer, resource.offset+8), resource.size, C.byref(sound)))
        assert sound.rate_fixed % 65536 == 0, 'WAV requires an integer sample rate'
        pcm = C.string_at(sound.samples, sound.frames*sound.channels*(sound.bits//8))
        if sound.bits == 16:
            swapped = bytearray(pcm); swapped[0::2] = pcm[1::2]; swapped[1::2] = pcm[0::2]; pcm = bytes(swapped)
        filename = f'{prefix}-{number}.wav'
        with wave.open(str(output/filename), 'wb') as wav:
            wav.setnchannels(sound.channels); wav.setsampwidth(sound.bits//8)
            wav.setframerate(sound.rate_fixed//65536); wav.writeframes(pcm)
        sounds.append({'member': number, 'name': members[number]['name'], 'file': filename,
                       'frames': sound.frames, 'rate': sound.rate_fixed//65536, 'channels': sound.channels,
                       'bits': sound.bits, 'pcm_sha256': hashlib.sha256(pcm).hexdigest(),
                       'cast_flags': flags, 'loop': not bool(flags & 16),
                       'loop_start': sound.loop_start, 'loop_end': sound.loop_end,
                       'source_resource': resource.id})
    (output/f'{prefix}.json').write_text(json.dumps(sounds, indent=2)+'\n')
