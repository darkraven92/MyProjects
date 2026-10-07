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
        # autoClearAFK registration (default "1") and non-forced clear gate.
        0x5e24d4: '6848e782006a00badc028600b9cc028600',
        0x5e24ea: 'e8a1b60500a38cd6c400',
        0x5eb830: '558bec83ec18a1cce5b6005633f63bc60f84b1000000',
        0x5eb846: '397508750ea18cd6c4003970280f849e000000',
        0x5eb87d: '6895000000',
        0x5eb8aa: '6a148d4de8e8dcc8e2ff',
        0x5eb8bd: '68482788008d4de8e866cbe2ff',
        0x5eb8f7: '5e8be55dc20400',
    }
    for va, signature in expected.items():
        wanted = bytes.fromhex(signature)
        if read(va, len(wanted)) != wanted:
            raise SystemExit(f'Signature mismatch at {va:#x}')
    threshold = -struct.unpack('<i', read(0x482ecf, 4))[0]
    for va, text in {0x8602cc: b'autoClearAFK', 0x82e748: b'1',
                     0x8602dc: b'Automatically clear AFK when moving or chatting',
                     0x860678: b'CLEARED_AFK', 0x882748: b''}.items():
        if read(va, len(text)+1) != text+b'\0':
            raise SystemExit(f'String mismatch at {va:#x}')
    for caller, target in {0x513d36: 0x5eb830, 0x514e23: 0x5eb830,
                           0x514f0b: 0x5eb830, 0x514fca: 0x5eb830,
                           0x49f3d6: 0x5eb830, 0x49f553: 0x5eb830,
                           0x5eb8d0: 0x5ab630}.items():
        instruction = read(caller, 5)
        if instruction[0] != 0xe8 or caller+5+struct.unpack('<i', instruction[1:])[0] != target:
            raise SystemExit(f'Call target mismatch at {caller:#x}')
    for api in ('GetBindingAction', 'EnumerateFrames', 'IsKeyboardEnabled',
                'UnitAffectingCombat', 'UnitIsDeadOrGhost', 'SendChatMessage'):
        if api.encode()+b'\0' not in data:
            raise SystemExit(f'Missing API registration string: {api}')
    print('SHA256', hashlib.sha256(data).hexdigest())
    print('SOURCE VERIFIED client idle threshold ms=', threshold)
    print('SOURCE VERIFIED input timestamp VA=0xcf0bc8 local AFK VA=0xb6e5cc')
    print('SOURCE VERIFIED local AFK encodings: 0 clear, 1 explicit mark, 2 server flag synchronization')
    print('SOURCE VERIFIED autoClearAFK default=1 pointer=0xc4d68c integerField=+0x28 (runtime value NOT inferred)')
    print('SOURCE VERIFIED clear=0x5eb830 force=0 respects CVar; CMSG_MESSAGECHAT=0x95 type=AFK/0x14 empty text; send=0x5ab630')
    print('SOURCE VERIFIED movement/chat callers are distinct from generic input timestamp writer')
    print('RUNTIME PENDING: paired F12 + native auto-clear composite, two prevention windows')


if __name__ == '__main__':
    main()
