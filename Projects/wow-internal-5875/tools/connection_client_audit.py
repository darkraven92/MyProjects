#!/usr/bin/env python3
"""Offline 5875 connection provenance audit. No attach, Lua execution or login.

Verifies the exact signatures used by the diagnostic reader, not UI visibility,
disconnect cause, safe action dispatch, or runtime reconnect qualification.
"""
import argparse
import hashlib
from pathlib import Path
import re

from water_client_audit import CLIENT_SHA256, PeImage


def reader_signatures(source):
    matches = re.findall(r"Match\(read, (0x[0-9a-f]+), std::array<unsigned char,(\d+)>\{([^}]+)\}", source)
    if len(matches) != 5:
        raise ValueError("Unexpected reader signature manifest")
    result = {}
    for address, size, values in matches:
        data = bytes(int(v.strip(), 0) for v in values.split(',') if v.strip())
        if len(data) != int(size):
            raise ValueError("Signature size mismatch")
        result[int(address, 16)] = data
    return result


def audit(data, source):
    if hashlib.sha256(data).hexdigest() != CLIENT_SHA256:
        raise ValueError("Client fingerprint mismatch; audit required")
    image = PeImage(data)
    for address, wanted in reader_signatures(source).items():
        if image.read(address, len(wanted)) != wanted:
            raise ValueError(f"Signature mismatch at {address:#x}")
    for address, name in {0x8377b0: b"IsConnectedToServer", 0x837800: b"DefaultServerLogin",
                          0x8377a4: b"EnterWorld", 0x837bec: b"SET_GLUE_SCREEN",
                          0x837bbc: b"DISCONNECTED_FROM_SERVER"}.items():
        if image.read(address, len(name)+1) != name+b'\0':
            raise ValueError("Registration/event name mismatch")
    return {"sourceSignatures": "PASS", "runtimeQualified": False,
            "reconnectImplemented": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    args = parser.parse_args()
    header = Path(__file__).resolve().parents[1] / "src/Bot/ConnectionEvidence5875.h"
    print(audit(args.client.read_bytes(), header.read_text()))
