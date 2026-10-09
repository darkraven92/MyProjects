#!/usr/bin/env python3
"""Export original bitmaps and score metadata through the native C decoder."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import zlib


class Archive(C.Structure):
    _fields_ = [('data', C.c_void_p), ('size', C.c_size_t), ('entries', C.c_size_t),
                ('count', C.c_uint32), ('version', C.c_uint32), ('stride', C.c_uint16),
                ('little_endian', C.c_int), ('form', C.c_char*5)]


class Resource(C.Structure):
    _fields_ = [('tag', C.c_char*5), ('id', C.c_uint32), ('offset', C.c_uint32),
                ('size', C.c_uint32), ('active', C.c_int)]


class Cast(C.Structure):
    _fields_ = [('resource', C.c_uint32), ('type', C.c_uint32), ('number', C.c_uint16),
                ('name', C.c_uint8*256), ('specific', C.c_void_p), ('specific_size', C.c_size_t)]


class Bitmap(C.Structure):
    _fields_ = [(k, C.c_uint16) for k in ('pitch', 'width', 'height', 'depth')] + [
        (k, C.c_int16) for k in ('top', 'left', 'reg_x', 'reg_y', 'palette')]


class Text(C.Structure):
    _fields_ = [('bytes', C.c_void_p), ('styles', C.c_void_p), ('length', C.c_uint32), ('style_count', C.c_uint16)]


class TextStyle(C.Structure):
    _fields_ = [('start', C.c_uint32)] + [(k,C.c_uint16) for k in
        ('height','ascent','font_id','size','red','green','blue')] + [('face',C.c_uint8)]


class TextBox(C.Structure):
    _fields_ = [(k,C.c_uint8) for k in ('border','gutter','box_shadow','type','text_shadow','flags')] + [
        (k,C.c_int16) for k in ('alignment','scroll','top','left','bottom','right')] + [
        ('background',C.c_uint16*3),('max_height',C.c_uint16),('text_height',C.c_uint16)]


class FontMapping(C.Structure):
    _fields_ = [('platform',C.c_uint16),('id',C.c_uint16),('name',C.c_void_p),('name_length',C.c_uint32)]


def checked(error):
    if error:
        raise ValueError(error.decode())


def png(path, width, height, rgba):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag+data))
    rows = b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def load_library(path):
    lib = C.CDLL(str(path.resolve()))
    signatures = {
        'sr_archive_open': [C.POINTER(Archive), C.c_void_p, C.c_size_t],
        'sr_find': [C.POINTER(Archive), C.c_char_p, C.POINTER(Resource)],
        'sr_cast_range': [C.POINTER(Archive), C.POINTER(C.c_uint16), C.POINTER(C.c_uint32)],
        'sr_cast': [C.POINTER(Archive), C.c_uint16, C.POINTER(Cast)],
        'sr_child': [C.POINTER(Archive), C.c_uint32, C.c_char_p, C.POINTER(Resource)],
        'sr_bitmap_info': [C.POINTER(Cast), C.POINTER(Bitmap)],
        'sr_text_open': [C.c_void_p,C.c_size_t,C.POINTER(Text)],
        'sr_text_style': [C.POINTER(Text),C.c_uint16,C.POINTER(TextStyle)],
        'sr_text_box': [C.POINTER(Cast),C.POINTER(TextBox)],
        'sr_fontmap_count': [C.c_void_p,C.c_size_t,C.POINTER(C.c_uint32)],
        'sr_fontmap_entry': [C.c_void_p,C.c_size_t,C.c_uint32,C.POINTER(FontMapping)],
        'sr_bitd_unpack': [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t],
        'sr_bitmap_rgba': [C.POINTER(Bitmap), C.c_void_p, C.c_size_t, C.c_int, C.c_void_p, C.c_size_t],
        'sr_score_frame': [C.c_void_p, C.c_size_t, C.c_uint32, C.c_void_p, C.POINTER(C.c_uint32)],
    }
    for name, args in signatures.items():
        fn = getattr(lib, name)
        fn.argtypes = args
        fn.restype = C.c_char_p
    return lib


def export(lib, path, output, metadata_only=False, selected_names=None):
    data = path.read_bytes()
    source = C.create_string_buffer(data)
    archive = Archive()
    checked(lib.sr_archive_open(C.byref(archive), source, len(data)))

    def payload(tag):
        resource = Resource()
        checked(lib.sr_find(C.byref(archive), tag.encode(), C.byref(resource)))
        return data[resource.offset+8:resource.offset+8+resource.size]

    output.mkdir(parents=True, exist_ok=True)
    config = payload('VWCF')
    top, left, bottom, right = struct.unpack_from('>hhhh', config, 4)
    report = {'source': str(path), 'sha256': hashlib.sha256(data).hexdigest(),
              'stage': {'top': top, 'left': left, 'width': right-left, 'height': bottom-top},
              'frame_rate': struct.unpack_from('>H', config, 54)[0],
              'members': [], 'frames': []}
    font_resource = Resource()
    if not lib.sr_find(C.byref(archive),b'Fmap',C.byref(font_resource)):
        font_ptr=C.byref(source,font_resource.offset+8)
        font_count=C.c_uint32()
        checked(lib.sr_fontmap_count(font_ptr,font_resource.size,C.byref(font_count)))
        report['fonts']=[]
        for index in range(font_count.value):
            font=FontMapping()
            checked(lib.sr_fontmap_entry(font_ptr,font_resource.size,index,C.byref(font)))
            raw=C.string_at(font.name,font.name_length)
            report['fonts'].append({'platform':font.platform,'id':font.id,
                'name':raw.decode('cp1252' if font.platform==2 else 'mac_roman'),'name_hex':raw.hex()})
    first, count = C.c_uint16(), C.c_uint32()
    cast_table = Resource()
    cast_error=lib.sr_find(C.byref(archive),b'CAS*',C.byref(cast_table))
    if cast_error != b'resource not found':
        checked(cast_error)
        checked(lib.sr_cast_range(C.byref(archive), C.byref(first), C.byref(count)))
    for number in range(first.value, first.value+count.value):
        cast = Cast()
        checked(lib.sr_cast(C.byref(archive), number, C.byref(cast)))
        if not cast.type:
            continue
        name = bytes(cast.name).split(b'\0', 1)[0]
        member = {'number': number, 'resource': cast.resource, 'type': cast.type,
                  'name': name.decode('mac_roman'), 'name_hex': name.hex()}
        report['members'].append(member)
        if cast.type == 3:
            box=TextBox()
            checked(lib.sr_text_box(C.byref(cast),C.byref(box)))
            member['text_box']={k:list(box.background) if k=='background' else getattr(box,k) for k,_ in TextBox._fields_}
            text_resource = Resource()
            error = lib.sr_child(C.byref(archive), cast.resource, b'STXT', C.byref(text_resource))
            if not error:
                text=Text()
                checked(lib.sr_text_open(C.byref(source,text_resource.offset+8),text_resource.size,C.byref(text)))
                text_bytes=C.string_at(text.bytes,text.length)
                member['text_styles']=[]
                for index in range(text.style_count):
                    style=TextStyle()
                    checked(lib.sr_text_style(C.byref(text),index,C.byref(style)))
                    member['text_styles'].append({k:getattr(style,k) for k,_ in TextStyle._fields_})
                member['text'] = text_bytes.decode('cp1252', errors='replace').replace('\r', '\n')
                member['text_hex'] = text_bytes.hex()
                member['stxt_resource'] = text_resource.id
        if cast.type != 1:
            continue
        if metadata_only or (selected_names is not None and member['name'].lower() not in selected_names):
            continue
        bitmap, bitd = Bitmap(), Resource()
        checked(lib.sr_bitmap_info(C.byref(cast), C.byref(bitmap)))
        checked(lib.sr_child(C.byref(archive), cast.resource, b'BITD', C.byref(bitd)))
        raw = C.create_string_buffer(bitmap.pitch*bitmap.height)
        ptr = C.byref(source, bitd.offset+8)
        checked(lib.sr_bitd_unpack(ptr, bitd.size, raw, len(raw)))
        rgba = C.create_string_buffer(bitmap.width*bitmap.height*4)
        checked(lib.sr_bitmap_rgba(C.byref(bitmap), raw, len(raw), bitd.size != len(raw), rgba, len(rgba)))
        member.update({k: getattr(bitmap, k) for k, _ in Bitmap._fields_})
        member.update({'bitd_resource': bitd.id, 'rgba_sha256': hashlib.sha256(rgba.raw).hexdigest(),
                       'rgba': f'{number:04d}.rgba', 'png': f'{number:04d}.png'})
        (output / member['rgba']).write_bytes(rgba.raw)
        png(output / member['png'], bitmap.width, bitmap.height, rgba.raw)

    if path.suffix.upper() == '.CST':
        (output / 'scene.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        print(f"{path.name}: {len(report['members'])} members, {sum('rgba' in m for m in report['members'])} decoded bitmaps")
        return report
    score = payload('VWSC')
    state = C.create_string_buffer(1200)
    frames = C.c_uint32()
    checked(lib.sr_score_frame(score, len(score), 0, state, C.byref(frames)))
    for frame in range(1, frames.value+1):
        checked(lib.sr_score_frame(score, len(score), frame, state, C.byref(frames)))
        sprites = []
        for channel in range(1, 49):
            raw = state.raw[48+(channel-1)*24:48+channel*24]
            if not any(raw):
                continue
            cl, member, sl, script = struct.unpack_from('>HHHH', raw, 2)
            y, x, height, width = struct.unpack_from('>hhhh', raw, 12)
            sprites.append(dict(channel=channel, type=raw[0], ink=raw[1]&63,
                                stretch=bool(raw[1]&128), cast_lib=cl, member=member,
                                script_lib=sl, script=script, x=x, y=y, width=width, height=height))
        report['frames'].append({'number': frame, 'script': struct.unpack_from('>H', state.raw, 2)[0],
                                 'sprites': sprites})
    (output / 'scene.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f"{path.name}: {sum(m['type']==1 for m in report['members'])} bitmaps, {frames.value} frames, {right-left}×{bottom-top}")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('movies', nargs='*', type=Path, default=[Path('SveaRike/HMENY.DIR'), Path('SveaRike/SETUP.DIR')])
    parser.add_argument('--library', type=Path, default=Path('build/libsvea_assets.so'))
    parser.add_argument('--output', type=Path, default=Path('analysis/assets'))
    parser.add_argument('--metadata-only', action='store_true')
    parser.add_argument('--names', type=Path, help='JSON list of bitmap names to decode')
    args = parser.parse_args()
    lib = load_library(args.library)
    for path in args.movies:
        if path.parent.resolve() == args.output.resolve() or path.parent.resolve() in args.output.resolve().parents:
            parser.error('Output must be outside original game directory')
        names = {s.lower() for s in json.loads(args.names.read_text())} if args.names else None
        export(lib, path, args.output / path.stem, args.metadata_only, names)


if __name__ == '__main__':
    main()
