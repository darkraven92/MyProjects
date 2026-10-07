#!/usr/bin/env python3
"""Read-only 5875 corpse-cache control-flow audit; no process or packet writes."""
import argparse
import hashlib
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    data = parser.parse_args().client.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("Not a PE")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 52)[0]
    if base != 0x400000:
        raise SystemExit("Unsupported image base")

    def read(va, size):
        for index in range(count):
            header = pe + 24 + optional + index * 40
            _, rva, raw_size, offset = struct.unpack_from("<IIII", data, header + 8)
            if rva <= va - base and va - base + size <= rva + raw_size:
                return data[offset + va - base - rva:offset + va - base - rva + size]
        raise ValueError("VA not file-backed")

    signatures = {
        0x48f494: "57ba90f64800b916020000",  # opcode registration
        0x48f6bb: "0fb680f8fa4800ff2485dcfa4800",  # two-level dispatch
        0x48f754: "8d45ff508bcfe85195f8ff",  # found byte
        0x48f766: "85f674798b8e680e00008b5108c1ea04f6c201",  # Ghost required
        0x48f7cd: "528d55e4e83a280000",  # actual map + XYZ to cache writer
        0x492019: "893d1ce3b4008bc68b08890d84e2b400",
        0x49202f: "891588e2b4008b40086890000000a38ce2b400890d20e3b400",
        0x491f57: "6aff8d55f483c9ff",  # invalidate before query
        0x491f74: "e897000000",
        0x491fb2: "6816020000",  # normal client request (not invoked by bot)
        0x5eeac1: "e88a34eaff",  # Ghost-change callback reset/query
        0x490a28: "e823150000",  # world initialization reset/query
    }
    for va, signature in signatures.items():
        expected = bytes.fromhex(signature)
        if read(va, len(expected)) != expected:
            raise SystemExit(f"Signature mismatch {va:#x}")
    index = read(0x48faf8 + 0x216 - 0x1e7, 1)[0]
    target = struct.unpack("<I", read(0x48fadc + index * 4, 4))[0]
    if target != 0x48f734:
        raise SystemExit("Unexpected corpse-query handler")
    print("SHA256", hashlib.sha256(data).hexdigest())
    print("SOURCE VERIFIED MSG_CORPSE_QUERY=0x216 -> 0x48f734")
    print("SOURCE VERIFIED found/ghost-gated display map, XYZ, actual corpse map -> 0x492010")
    print("SOURCE VERIFIED displayMap=0xb4e31c XYZ=0xb4e284 actualMap=0xb4e320")
    print("SOURCE VERIFIED reset/query=0x491f50 on Ghost flag change and world initialization")
    print("Corpse GUID and cache age are NOT supplied by this packet/cache.")
    print("Runtime publication delay and death-to-alive qualification remain PENDING.")


if __name__ == "__main__":
    main()
