from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import hashlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from connection_client_audit import (audit, reader_signatures,
                                     GLUE_RESEARCH_SIGNATURES,
                                     verify_glue_research_signatures)


class ResearchImage:
    def __init__(self):
        self.memory = {address: bytes.fromhex(data)
                       for address, data in GLUE_RESEARCH_SIGNATURES.items()}
        self.memory.update({0x8792f0: b"IsVisible\0", 0x8792e8: b"IsShown\0",
                            0x83705c: b"GlueParent\0"})

    def read(self, address, size):
        if address not in self.memory:
            raise ValueError("Unreadable fixture region")
        return self.memory[address][:size]


class ConnectionClientAuditTest(unittest.TestCase):
    def test_manifest_matches_expected_callbacks(self):
        source = (Path(__file__).resolve().parents[2] / "src/Bot/ConnectionEvidence5875.h").read_text()
        signatures = reader_signatures(source)
        self.assertEqual(set(signatures), {0x8374a0, 0x46d380, 0x5ab490, 0x46ce8f, 0x46b860})
        self.assertEqual(len(signatures[0x46d380]), 51)

    def test_unknown_image_rejected(self):
        with self.assertRaises(ValueError):
            audit(b'not the client', '')
        with self.assertRaises(ValueError):
            reader_signatures('')

    def test_research_signatures_fail_closed(self):
        verify_glue_research_signatures(ResearchImage())
        for address in ResearchImage().memory:
            with self.subTest(address=hex(address)):
                changed = ResearchImage()
                original = changed.memory[address]
                changed.memory[address] = bytes([original[0] ^ 1]) + original[1:]
                with self.assertRaises(ValueError):
                    verify_glue_research_signatures(changed)
                changed.memory[address] = original[:-1]
                with self.assertRaises(ValueError):
                    verify_glue_research_signatures(changed)
                del changed.memory[address]
                with self.assertRaises(ValueError):
                    verify_glue_research_signatures(changed)

    def test_static_research_never_qualifies_live_glue_or_reconnect(self):
        source = (Path(__file__).resolve().parents[2] / "src/Bot/ConnectionEvidence5875.h").read_text()
        image = ResearchImage()
        image.memory.update(reader_signatures(source))
        image.memory.update({0x8377b0: b"IsConnectedToServer\0",
                             0x837800: b"DefaultServerLogin\0", 0x8377a4: b"EnterWorld\0",
                             0x837bec: b"SET_GLUE_SCREEN\0", 0x837bbc: b"DISCONNECTED_FROM_SERVER\0"})
        data = b"synthetic audit input"
        # This fixture tests audit control flow, not the real client identity.
        # The exact local client is separately audited by the CLI.
        with patch("connection_client_audit.CLIENT_SHA256", hashlib.sha256(data).hexdigest()), \
             patch("connection_client_audit.PeImage", return_value=image):
            result = audit(data, source)
            self.assertEqual(result, {"sourceSignatures": "PASS", "glueResearchSignatures": "PASS",
                                     "liveGlueQualified": False, "runtimeQualified": False,
                                     "reconnectImplemented": False})
            image.memory[0x703ba0] = b"changed teardown"
            with self.assertRaises(ValueError):
                audit(data, source)
