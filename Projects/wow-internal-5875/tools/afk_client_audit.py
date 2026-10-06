#!/usr/bin/env python3
"""Read-only audit of the local 5875 PE; does not attach or send input."""
import argparse
import hashlib
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client', type=Path)
    args = parser.parse_args()
    data = args.client.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise SystemExit('Not a PE file')
    sections, optional_size = struct.unpack_from('<H', data, pe+6)[0], struct.unpack_from('<H', data, pe+20)[0]
    base = struct.unpack_from('<I', data, pe+24+28)[0]
    if base != 0x400000:
        raise SystemExit('Unsupported image base')

    def read(va, size):
        rva = va-base
        for i in range(sections):
            h = pe+24+optional_size+40*i
            _, section_rva, raw_size, offset = struct.unpack_from('<IIII', data, h+8)
            if section_rva <= rva and rva+size <= section_rva+raw_size:
                return data[offset+rva-section_rva:offset+rva-section_rva+size]
        raise ValueError('Address not backed by file')

    expected = {
        0x482ec3: '8b3dc80bcf008bd82bc78d88206cfbff85c9',
        0x5eb836: 'a1cce5b600',
        0x5ee9ef: '83e102890dcce5b600',
        0x765f34: '890dc80bcf00',
        0x42c010: 'e97bf7ffff',
    }
    for va, signature in expected.items():
        wanted = bytes.fromhex(signature)
        if read(va, len(wanted)) != wanted:
            raise SystemExit(f'Signature mismatch at {va:#x}')
    threshold = -struct.unpack('<i', read(0x482ecf, 4))[0]
    for api in ('GetBindingAction', 'EnumerateFrames', 'IsKeyboardEnabled',
                'UnitAffectingCombat', 'UnitIsDeadOrGhost', 'SendChatMessage'):
        if api.encode()+b'\0' not in data:
            raise SystemExit(f'Missing API registration string: {api}')
    print('SHA256', hashlib.sha256(data).hexdigest())
    print('SOURCE VERIFIED client idle threshold ms=', threshold)
    print('SOURCE VERIFIED input timestamp VA=0xcf0bc8 local AFK VA=0xb6e5cc')
    print('SOURCE VERIFIED local AFK encodings: 0 clear, 1 explicit mark, 2 server flag synchronization')
    print('RUNTIME PENDING: keyboard message path, observed timeout, two prevention windows')


if __name__ == '__main__':
    main()
