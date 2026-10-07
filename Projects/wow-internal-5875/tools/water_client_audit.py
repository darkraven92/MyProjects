#!/usr/bin/env python3
"""Read-only build-5875 water evidence audit. Never attaches or invokes client code.

Matching these instructions proves only the documented static paths, not a
live breath snapshot, safe liquid height, or a releasable swim input adapter.
"""
import argparse
import hashlib
from pathlib import Path
import struct


CLIENT_SHA256 = "b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7"
SIGNATURES = {
    # Native movement consumer reads the same pointer/word as PlayerSnapshot/AFK.
    0x60E0D4: "8b8e18010000f7414000002000",
    # Mirror packet switch: 0x1D9 start / 0x1DA pause / 0x1DB stop.
    0x5E7996: "81ead9010000560f84820000004a743a4a",
    # Start: type/current/max/scale(uint32,int32)/paused(byte)/spell(uint32).
    0x5E7A25: "8b750c8d4dfc518bcee8fd13e3ff",
    0x5E7A33: "8d55ec528bcee8f213e3ff",
    0x5E7A3E: "8d45f0508bcee8e713e3ff",
    0x5E7A49: "8d4df4518bcee8dc13e3ff",
    0x5E7A54: "8d550f528bcee85112e3ff",
    0x5E7A5F: "8d45f8508bcee8c613e3ff",
    0x5E7A75: "0fb64d0f8b55f4508b45f0518b4dec525051",
    0x5E7A87: "8b4dfce851000000506810058600686a010000e8b1c41100",
    # Type1 -> BREATH; type0 -> EXHAUSTION; type2 -> FEIGNDEATH.
    0x5E7AE0: "83e900741849740f497406b844808300c3",
    0x5E7AF1: "b834058600c3b82c058600c3b820058600c3",
    0x51B3DD: "c7054017be0094268500c7054417be0080268500c7054817be006c268500",
    # Jump registration/call chain is NOT sufficient ascent-release proof.
    0x8500B8: "3cfa8400d03b5100",
    0x513D3B: "578bcee85da10f00",
    0x60DF52: "8b4508508d8ea8090000e8cf990000",
    0x617943: "6a016a0752",
    0x61795D: "e80efcffff",
}


class PeImage:
    def __init__(self, data):
        self.data = data
        try:
            if data[:2] != b"MZ":
                raise ValueError("Not a DOS/PE image")
            pe = struct.unpack_from("<I", data, 0x3c)[0]
            if data[pe:pe + 4] != b"PE\0\0":
                raise ValueError("Not a PE image")
            machine, count = struct.unpack_from("<HH", data, pe + 4)
            optional = struct.unpack_from("<H", data, pe + 20)[0]
            magic = struct.unpack_from("<H", data, pe + 24)[0]
            self.base = struct.unpack_from("<I", data, pe + 52)[0]
            if machine != 0x14c or magic != 0x10b or self.base != 0x400000:
                raise ValueError("Not the supported 32-bit image layout")
            if optional < 32 or not 0 < count <= 96:
                raise ValueError("Invalid PE headers")
            self.sections = []
            for index in range(count):
                header = pe + 24 + optional + index * 40
                _, rva, size, offset = struct.unpack_from("<IIII", data, header + 8)
                if offset + size > len(data):
                    raise ValueError("Truncated PE section")
                self.sections.append((rva, size, offset))
        except struct.error as error:
            raise ValueError("Truncated PE headers") from error

    def read(self, va, size):
        if size <= 0:
            raise ValueError("Invalid read size")
        relative = va - self.base
        for rva, raw_size, offset in self.sections:
            if rva <= relative and relative + size <= rva + raw_size:
                start = offset + relative - rva
                return self.data[start:start + size]
        raise ValueError(f"VA {va:#x} not file-backed")


def verify_signatures(image, signatures=SIGNATURES):
    for va, expected in signatures.items():
        expected = bytes.fromhex(expected)
        if image.read(va, len(expected)) != expected:
            raise ValueError(f"Signature mismatch {va:#x}")


def audit(data):
    if hashlib.sha256(data).hexdigest() != CLIENT_SHA256:
        raise ValueError("Unsupported client SHA256; re-audit before using evidence")
    image = PeImage(data)
    verify_signatures(image)
    for va, text in {0x86052C: b"BREATH\0", 0x860520: b"EXHAUSTION\0",
                     0x860534: b"FEIGNDEATH\0", 0x852694: b"MIRROR_TIMER_START\0"}.items():
        if image.read(va, len(text)) != text:
            raise ValueError(f"Unexpected event/type string {va:#x}")
    return [
        f"SHA256 {CLIENT_SHA256}",
        "SOURCE VERIFIED native movement+0x40 swimming-bit consumer (player+0x118)",
        "SOURCE VERIFIED mirror packet dispatcher 0x5e7990 opcodes 0x1d9/0x1da/0x1db",
        "SOURCE VERIFIED start payload -> event0x16a; BREATH type1; EXHAUSTION type0",
        "SOURCE VERIFIED Jump ->0x60dea0 ->0x617930 -> queued movement event7",
        "SOURCE NOT VERIFIED live submersion/surface-height snapshot; ascent/release adapter",
        "Breath events are not a polled native timer cache. No modern Lua getter assumed.",
        "No commands enabled. Water runtime: RUNTIME PENDING.",
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    args = parser.parse_args()
    try:
        print("\n".join(audit(args.client.read_bytes())))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Water source audit FAIL: {error}\n")


if __name__ == "__main__":
    main()
