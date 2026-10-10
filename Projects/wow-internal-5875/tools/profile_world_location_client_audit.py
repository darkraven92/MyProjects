#!/usr/bin/env python3
"""Offline P0.7.3 location-source research for the exact build-5875 executable.

No process attachment, client calls, Lua, memory writes, or runtime execution.
PASS pins the documented instruction paths, NOT current-world qualification.
See docs/P07_WORLD_LEVELING_PROFILE_AUDIT.md for the unresolved lifetime gaps.
"""
import argparse
import hashlib
import json
from pathlib import Path

from water_client_audit import CLIENT_SHA256, PeImage


# Research anchors: four also pin P0.7.4 raw diagnostic reads, never profile authority.
SIGNATURES = {
    # Selected-map registrations/indices versus native current-context map use.
    0x845090: "ec528400207e4a00e0528400307e4a00c8528400d07e4a00b4528400007f4a00",
    0x4a7ed4: "a16c50840040",
    0x4a7f04: "a170508400408945fcdb45fc",
    0x4a6697: "e8644014008b1d68e6b60039041f",
    0x5ea700: "e97bdee7ff",
    # Native GUID/map getters and map setter. Null map getter returns zero.
    0x468550: "8b0d1414b40085c9750533c033d2c38b81c00000008b91c4000000c3",
    0x468580: "a11414b40085c074078b80cc000000c333c0c3",
    0x4685a0: "a11414b4008988cc000000c3",
    # Constructor initializes zero, publishes root, then assigns map.
    0x4650db: "89bec000000089bec400000089bec800000089becc00000089bed000000089bed400000089bed8000000",
    0x465109: "a11481c20089351414b40089b0d81a00008b0d1481c200e88b340000e8160000008bcbe86f340000",
    # GUID publication is two DWORD stores. Teardown precedes root clearing.
    0x466237: "f645c001741d8b55f0a11414b4008990c00000008b4df48b151414b400898ac4000000",
    0x467700: "a11414b400565733ff3bc70f84df000000e8ea0000008b351414b4003bf7893d1414b4000f84c6000000",
    # Root context can be saved/switched/restored; it is not a generation.
    0x464fa0: "890d1414b400c3",
    0x464fb0: "a11814b40085c07513a11414b4003bc8740aa31814b400e9d4ffffffc3",
    0x464fd0: "8b0d1814b40085c9740fe8c1ffffffc7051814b40000000000c3",
    # NEW_WORLD packet, pending map, replacement owner before terrain load.
    0x40174a: "6a00ba001b4000b93e000000e8f59e1a00",
    0x401b00: "558bec568b750c682c2688008bcee89d730100",
    0x401bf2: "e8f9cc2100e8045b0600e83fe026008b0d6c32c6003bce7405e8b09e270033c9e8996a0500e8b47e2e008b0d2c268800e8c9330600",
    0x401c49: "a12c2688008b0de42688006a0150ba8c268800e87fdf2600",
    # Terrain map initialization/load and late resource-marker clearing.
    0x66fc14: "8b55088bcea3f0b2c700e8cd4502005e5dc20800",
    0x69441e: "893dcca28600891d80e3c900893584e3c900891dd0a28600",
    0x691bb7: "c705cca28600ffffffff",
    0x697be0: "e88b8e020033c05fa380e3c900a384e3c9005ec3",
    0x4017e5: "c7053427880001000000",
    0x40204d: "c7053427880000000000",
    0x407e70: "8b0de02b880033c085c90f95c0c3",
    # Zone-text registrations and full callbacks: only string results.
    0x83e0a8: "3cee8300a0a048002cee8300c0a048001cee8300e0a04800",
    0x48a0a0: "8b15f8b3b40085d27505ba48278800e8dc972600b801000000c3",
    0x48a0c0: "8b1504b4b40085d27505ba48278800e8bc972600b801000000c3",
    0x48a0e0: "8b1580e2b40085d27505ba48278800e89c972600b801000000c3",
    # Sequential numeric zone/child-area publication; eventual zeroing.
    0x494787: "8b3514e3b40085f60f94c033db3bf1570f95c3890d14e3b400891518e3b400",
    0x491236: "33ff33f6",
    0x491266: "893d14e3b400893d18e3b400",
    # Spatial lookup failure skips publication; row parent +8 / IDs +0.
    0x67e546: "e8551effff85c00f840e010000",
    0x67e57d: "8b47083bc674137c0b3bc37f078b1c828b13eb1133db8b13eb0b8bd133c98bdf894df833ff",
    0x67e62f: "85ff74048b17eb0233d28b7dec5756518b4df48b0950e83661e1ff",
    # Periodic update prefers alternate GUID/type8 before local-player/type10.
    0x5db900: "a190d7c4004083f80aa390d7c4007c658bc8a19cdac40083e90a68c0000000",
    0x5db925: "8b0d98dac4005051ba8c018600b908000000e824cbe8ff85c0751f6890000000e806cce8ff5250ba48f98500b910000000e805cbe8ff85c07416568bb0e0000000e815cce8ff8bd68bc8e89c2b0a00",
    # Deduplication equality is not generation; reset/OM teardown/UI cleanup.
    0x67e80b: "3b35fc858600751a3b150086860075123b0504868600750a",
    0x401fcb: "e880c42700",
    0x402011: "e8ea560600",
    0x402039: "e842f10800",
    0x51ada7: "c705c414be00b8328500c705c814be00a0328500c705cc14be0088328500",
}

STRINGS = {
    0x8452ec: b"SetMapToCurrentZone\0",
    0x8452c8: b"GetCurrentMapContinent\0",
    0x8452b4: b"GetCurrentMapZone\0",
    0x83ee3c: b"GetZoneText\0",
    0x83ee2c: b"GetRealZoneText\0",
    0x83ee1c: b"GetSubZoneText\0",
    0x857ecc: b"DBFilesClient\\AreaTable.dbc\0",
    0x82e3b0: b"Bad SMSG_NEW_WORLD zoneID\n\0",
    0x85d71c: b"Map:\t\t%u (%s)\n\0",
    0x8532b8: b"ZONE_CHANGED\0",
    0x8532a0: b"ZONE_CHANGED_INDOORS\0",
    0x853288: b"ZONE_CHANGED_NEW_AREA\0",
}


def verify_research_signatures(image):
    for address, expected in SIGNATURES.items():
        wanted = bytes.fromhex(expected)
        if image.read(address, len(wanted)) != wanted:
            raise ValueError(f"Location research signature mismatch at {address:#x}")
    for address, wanted in STRINGS.items():
        if image.read(address, len(wanted)) != wanted:
            raise ValueError(f"Location research string mismatch at {address:#x}")


def audit(data):
    if hashlib.sha256(data).hexdigest() != CLIENT_SHA256:
        raise ValueError("Unsupported client SHA256; source re-audit required")
    image = PeImage(data)  # Also requires PE32/i386 and base 0x400000.
    verify_research_signatures(image)
    return {
        "clientSha256": CLIENT_SHA256,
        "peLayout": "PE32 i386 base 0x400000",
        "researchSignatures": "PASS",
        "sourceScope": "documented instruction provenance only",
        "currentMapQualified": False,
        "currentZoneQualified": False,
        "currentAreaQualified": False,
        "worldLifetimeQualified": False,
        # Implementation metadata only; this offline audit cannot run/qualify it.
        "readerImplemented": True,
        "readerScope": "unqualified_raw_observe_only",
        "runtimeObserved": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.client.read_bytes()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Profile location source audit FAIL: {error}\n")


if __name__ == "__main__":
    main()
