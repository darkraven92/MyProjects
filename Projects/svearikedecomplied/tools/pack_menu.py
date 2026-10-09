#!/usr/bin/env python3
"""Package decoded original artwork and original score hit regions for the C menu."""
from collections import deque
import json
from pathlib import Path
import struct

ROOT = Path('analysis/assets')
OUT = Path('build/wasm')


def matte(rgba, width, height):
    """Director matte candidate: white connected to an image edge is transparent.

    Interior whites remain opaque. Keep unmodified exports as the reference.
    This behavior still needs a visual comparison against the original runtime.
    """
    data = bytearray(rgba)
    queue = deque()

    def visit(x, y):
        i = (y*width+x)*4
        if data[i:i+4] == b'\xff\xff\xff\xff':
            data[i+3] = 0
            queue.append((x, y))
    for x in range(width):
        visit(x, 0)
        visit(x, height-1)
    for y in range(height):
        visit(0, y)
        visit(width-1, y)
    while queue:
        x, y = queue.popleft()
        for xx, yy in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
            if 0 <= xx < width and 0 <= yy < height:
                visit(xx, yy)
    return bytes(data)


def main():
    images, regions = [], []
    for scene, name, frame in ((0, 'HMENY', 21), (1, 'SETUP', 3)):
        folder = ROOT/name
        report = json.loads((folder/'scene.json').read_text())
        for member in report['members']:
            if member['type'] != 1:
                continue
            rgba = (folder/member['rgba']).read_bytes()
            if scene == 1 and 33 <= member['number'] <= 37:
                rgba = matte(rgba, member['width'], member['height'])
            images.append((scene, member, rgba))
        for sprite in report['frames'][frame-1]['sprites']:
            if sprite['script']:
                regions.append((scene, sprite))
    # PICTS is an asset namespace, not a runtime screen/Director interpreter.
    folder = ROOT/'PICTS'
    report = json.loads((folder/'scene.json').read_text())
    copy_members = {1, 401, 402, 403, 404, 406}
    copy_members.update(range(401,429))
    copy_members.update((228,229,230))
    matte_members = {355,356,357,358,359,498,499,881,689}
    matte_members.add(165)  # SVEA frame 48: Stad norm, ink 8.
    matte_members.update((17,193,217,218,219))
    matte_members.update((90,111,125))
    matte_members.update((273,277))
    matte_members.add(382)
    matte_members.add(231)
    matte_members.update((133,151))
    matte_members.update((281,295))
    matte_members.add(287)
    matte_members.update((240,241,252,258,259,262,264,266,491))
    copy_members.update(range(242,246))
    copy_members.update(range(254,258))
    copy_members.update((290,291,292,293))  # Puppet channel 29 retains ink 0.
    copy_members.update(range(323,328))
    copy_members.update(range(336,341))
    copy_members.update(range(346,351))
    copy_members.update(range(835,847))
    matte_members.update(range(136,141))
    copy_members.update(range(360,365))
    matte_members.update(range(689,749))
    matte_members.update(range(96,101))
    copy_members.update(range(117,122))
    copy_members.update(range(173,178))  # CityLevel replacement pictures, ink 0.
    copy_members.update(range(200,205))  # MilitaryLevel replacement pictures.
    for member in report['members']:
        if 'rgba' not in member:
            continue
        rgba = (folder/member['rgba']).read_bytes()
        if member['number'] in matte_members or member['name'].endswith('.pct'):
            rgba = matte(rgba, member['width'], member['height'])
        elif member['number'] not in copy_members:
            # Score ink 36: background-transparent; remove all palette-white.
            pixels = bytearray(rgba)
            for i in range(0,len(pixels),4):
                if pixels[i:i+3] == b'\xff\xff\xff': pixels[i+3] = 0
            rgba = bytes(pixels)
        images.append((2,member,rgba))
    # ARMBORST's own cast. Keep copy, matte and background-transparent inks.
    folder = ROOT/'ARMBORST'
    report = json.loads((folder/'scene.json').read_text())
    for member in report['members']:
        if 'rgba' not in member:
            continue
        rgba = (folder/member['rgba']).read_bytes()
        number = member['number']
        if number in (40,42,43,44,45,46,100):
            rgba = matte(rgba, member['width'], member['height'])
        elif number not in (7,22,37,91,92,93,94,95,96,97,98):
            pixels = bytearray(rgba)
            for i in range(0,len(pixels),4):
                if pixels[i:i+3] == b'\xff\xff\xff': pixels[i+3] = 0
            rgba = bytes(pixels)
        images.append((3,member,rgba))
    # LINNE's used original cards/panels. Exclude three unused authoring images.
    folder = ROOT/'LINNE'
    report = json.loads((folder/'scene.json').read_text())
    for member in report['members']:
        if 'rgba' not in member or member['number'] in (12,14,34):
            continue
        rgba = (folder/member['rgba']).read_bytes()
        if member['number'] in (35,36):
            pixels=bytearray(rgba)
            for i in range(0,len(pixels),4):
                if pixels[i:i+3]==b'\xff\xff\xff': pixels[i+3]=0
            rgba=bytes(pixels)
        elif member['number'] not in (1,2,13,33):
            rgba=matte(rgba,member['width'],member['height'])
        images.append((4,member,rgba))
    # SVEA's original city controls; runtime screen SR_CITY = 6.
    city = json.loads((ROOT/'SVEA'/'scene.json').read_text())['frames'][47]
    for sprite in city['sprites']:
        if sprite['channel'] in (25,26,28,29):
            regions.append((6,sprite))
    army = json.loads((ROOT/'SVEA'/'scene.json').read_text())['frames'][87]
    for sprite in army['sprites']:
        if sprite['channel'] in (27,28,29,30,31,35,38,39,40):
            regions.append((7,sprite))
    frames=json.loads((ROOT/'SVEA'/'scene.json').read_text())['frames']
    for screen,frame in ((8,78),(9,83)):
        for sprite in frames[frame-1]['sprites']:
            if sprite['channel'] in (25,26):
                regions.append((screen,sprite))
    for sprite in frames[102]['sprites']:
        if sprite['channel'] in (24,35,36,37,38,39,40,43):
            regions.append((12,sprite))
    for sprite in frames[97]['sprites']:
        if sprite['channel'] in (27,35,36):
            regions.append((13,sprite))
    for sprite in frames[52]['sprites']:
        if sprite['channel'] in (24,25,26,29):
            regions.append((14,sprite))
    for sprite in frames[68]['sprites']:
        if sprite['channel'] in (23,24,36,37):
                regions.append((15,sprite))
    for sprite in frames[11]['sprites']:
        if sprite['script'] in (110,111,112,113): regions.append((4,sprite))
    for scene,frame in ((17,112),(18,118)):
        for sprite in frames[frame-1]['sprites']:
            if sprite['script'] and sprite['channel']!=48: regions.append((scene,sprite))
    # War screen resolves regions by 100 + its C phase; no Director execution.
    for phase,frame in enumerate((201,197,204,207,214,219,221,225)):
        for sprite in frames[frame-1]['sprites']:
            if sprite['script'] and sprite['channel']!=48: regions.append((100+phase,sprite))
    # Fixed LE records, with byte offsets relative to the beginning of the pack.
    offset = 16+len(images)*36+len(regions)*32
    descriptors, pixels = bytearray(), bytearray()
    for scene, member, rgba in images:
        descriptors += struct.pack('<IIIIiiIII', scene, member['number'], member['width'], member['height'],
                                   member['reg_x']-member['left'], member['reg_y']-member['top'],
                                   offset, len(rgba), 0)
        pixels += rgba
        offset += len(rgba)
    for scene, sprite in regions:
        descriptors += struct.pack('<IIIIiiii', scene, sprite['channel'], sprite['script'], 0,
                                   sprite['x'], sprite['y'], sprite['width'], sprite['height'])
    OUT.mkdir(parents=True, exist_ok=True)
    pack = b'SRM1'+struct.pack('<III', len(images), len(regions), offset)+descriptors+pixels
    assert len(pack) == offset
    (OUT/'menu.pack').write_bytes(pack)
    print(f'Packed {len(images)} original images and {len(regions)} score hit regions ({len(pack):,} bytes)')


if __name__ == '__main__':
    main()
