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


# R0.1b.2 OFFLINE RESEARCH ONLY. These exact instruction paths explain why
# native callbacks / Lua globals cannot yet be used as a live snapshot. They
# are not runtime offsets, callable adapters, or a visibility qualification.
# See RUNTIME_CONNECTION_RELIABILITY_AUDIT.md for control flow and MPQ hashes.
GLUE_RESEARCH_SIGNATURES = {
    # Create publishes the Lua state before completing initialization.
    0x7039e0: "56e83a33ffffba000100008bc8a374efce00e8090affff",
    # Reset closes/recreates; close clears the singleton AFTER the close call.
    0x703b80: "a174efce0085c0740ae812000000e94dfeffffc3",
    0x703ba0: "8b0d74efce00e8d533ffffc70574efce0000000000c3",
    0x7040d0: "a174efce00c3",
    # Both Glue setup and FrameXML setup call the same reset routine.
    0x46a877: "3bf37405e800932900",
    0x48fe97: "e8e43c2700e8af030000",
    0x491231: "e84a292700",
    # Named-frame lookup uses Lua; its push-string path writes the Lua stack.
    0x76c760: "5356578bda8bf9e86479f9ff8bf08bd78bcee81971f8ffbaefd8ffff8bcee8bd72f8ff83caff8bce",
    0x6f387b: "8947088b460883c0105f8946085e5b5dc20400",
    # Registered frame IsVisible and IsShown callbacks read DIFFERENT fields
    # after extracting/type-checking the native object. No live object binding.
    0x878fd0: "f0928700d0587700e892870090597700",
    0x775955: "8b87d400000085c05f5b8bce7413680000f03f6a00e8a1def7ffb8010000005ec3e875def7ffb8010000005ec3",
    0x775a15: "8b87d000000085c05f5b8bce7413680000f03f6a00e8e1ddf7ffb8010000005ec3e8b5ddf7ffb8010000005ec3",
    # GlueParent lookup receives a lazily allocated type token, not a proven
    # Glue interpreter generation; do not use CEEF6C/CF0C10 as a lifetime guard.
    0x46ac1d: "8b15100ccf0085d275138b156cefce004289156cefce008915100ccf00b95c708300e81c1b3000",
    # LoadingScreen query tests a resource handle, not a qualified phase enum.
    0x407e70: "8b0de02b880033c085c90f95c0c3",
}


def verify_glue_research_signatures(image):
    for address, expected in GLUE_RESEARCH_SIGNATURES.items():
        wanted = bytes.fromhex(expected)
        if image.read(address, len(wanted)) != wanted:
            raise ValueError(f"Glue research signature mismatch at {address:#x}")
    for address, name in {0x8792f0: b"IsVisible", 0x8792e8: b"IsShown",
                          0x83705c: b"GlueParent"}.items():
        if image.read(address, len(name)+1) != name+b'\0':
            raise ValueError("Glue research registration/name mismatch")


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
    verify_glue_research_signatures(image)
    return {"sourceSignatures": "PASS", "glueResearchSignatures": "PASS",
            "liveGlueQualified": False, "runtimeQualified": False,
            "reconnectImplemented": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    args = parser.parse_args()
    header = Path(__file__).resolve().parents[1] / "src/Bot/ConnectionEvidence5875.h"
    print(audit(args.client.read_bytes(), header.read_text()))
