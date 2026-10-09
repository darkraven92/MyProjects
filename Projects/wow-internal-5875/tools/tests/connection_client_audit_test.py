from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from connection_client_audit import audit, reader_signatures


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
