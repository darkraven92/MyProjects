from pathlib import Path
import hashlib
import re
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from profile_world_location_client_audit import (
    CLIENT_SHA256, SIGNATURES, STRINGS, audit, verify_research_signatures)

ROOT = Path(__file__).resolve().parents[2]


class ResearchImage:
    def __init__(self):
        self.memory = {address: bytes.fromhex(data)
                       for address, data in SIGNATURES.items()}
        self.memory.update(STRINGS)

    def read(self, address, size):
        if address not in self.memory:
            raise ValueError("Unreadable fixture region")
        return self.memory[address][:size]


class ProfileWorldLocationClientAuditTest(unittest.TestCase):
    def test_wrong_client_rejected_before_interpreting_addresses(self):
        self.assertEqual(CLIENT_SHA256,
                         "b4756d38ef207c02ed651f4952bd89a70b4857b73a33413339e1b285b28d2dc7")
        with patch("profile_world_location_client_audit.PeImage") as image:
            for data in (b"", b"not WoW", b"MZ" + bytes(256)):
                with self.assertRaisesRegex(ValueError, "SHA256"):
                    audit(data)
            image.assert_not_called()

    def test_malformed_pe_rejected_even_with_fixture_fingerprint(self):
        # Synthetic fingerprint only tests audit control flow. The exact local
        # executable is separately tested through the CLI with the real hash.
        for data in (b"not PE", b"MZ" + bytes(256)):
            with patch("profile_world_location_client_audit.CLIENT_SHA256",
                       hashlib.sha256(data).hexdigest()):
                with self.assertRaises(ValueError):
                    audit(data)

    def test_every_signature_and_string_fails_closed(self):
        verify_research_signatures(ResearchImage())
        for address, original in ResearchImage().memory.items():
            with self.subTest(address=hex(address)):
                for replacement in (bytes([original[0] ^ 1]) + original[1:],
                                    original[:-1], None):
                    image = ResearchImage()
                    if replacement is None:
                        del image.memory[address]
                    else:
                        image.memory[address] = replacement
                    with self.assertRaises(ValueError):
                        verify_research_signatures(image)

    def test_pass_never_qualifies_current_location_or_runtime(self):
        data = b"synthetic research fixture"
        image = ResearchImage()
        with patch("profile_world_location_client_audit.CLIENT_SHA256",
                   hashlib.sha256(data).hexdigest()), \
             patch("profile_world_location_client_audit.PeImage", return_value=image):
            result = audit(data)
            self.assertEqual(result["researchSignatures"], "PASS")
            self.assertEqual(result["sourceScope"], "documented instruction provenance only")
            for key in ("currentMapQualified", "currentZoneQualified",
                        "currentAreaQualified", "worldLifetimeQualified",
                        "readerImplemented", "runtimeObserved"):
                self.assertIs(result[key], False, key)
            self.assertEqual(audit(data), result)
            image.memory[0x491266] = b"changed cleanup"
            with self.assertRaises(ValueError):
                audit(data)

    def test_adapter_dependency_closure_has_no_reader_or_action_adapter(self):
        pending = [ROOT / "src/Bot/ProfileWorldEvidenceAdapter.h"]
        visited = set()
        while pending:
            path = pending.pop().resolve()
            if path in visited:
                continue
            visited.add(path)
            source = path.read_text()
            for forbidden in ("ReadProcessMemory", "WriteProcessMemory",
                              "Core::Memory", "GameThreadDispatcher",
                              "ExecuteLua", "ExecuteScript", "SendInput",
                              "keybd_event", "mouse_event", "Controller::",
                              "ConnectionEvidence5875", "Wow5875::Client"):
                self.assertNotIn(forbidden, source, str(path.relative_to(ROOT)))
            for include in re.findall(r'#include "([^"]+)"', source):
                pending.append(path.parent / include)
        self.assertEqual({str(p.relative_to(ROOT)) for p in visited}, {
            "src/Bot/ProfileWorldEvidenceAdapter.h", "src/Bot/ProfileWorldEvidence.h"})


if __name__ == "__main__":
    unittest.main()
