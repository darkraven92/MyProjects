import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from water_client_audit import PeImage, audit, verify_signatures


def fixture():
    data = bytearray(0x240)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", data, 0x84, 0x14c, 1)
    struct.pack_into("<H", data, 0x94, 0xe0)
    struct.pack_into("<H", data, 0x98, 0x10b)
    struct.pack_into("<I", data, 0xb4, 0x400000)
    struct.pack_into("<IIII", data, 0x178 + 8, 0x80, 0x1000, 0x40, 0x200)
    data[0x200:0x204] = b"TEST"
    return data


class WaterClientAuditTest(unittest.TestCase):
    def test_file_backed_va_and_signature(self):
        image = PeImage(fixture())
        self.assertEqual(image.read(0x401000, 4), b"TEST")
        verify_signatures(image, {0x401000: "54455354"})

    def test_changed_instruction_rejected(self):
        with self.assertRaisesRegex(ValueError, "Signature mismatch"):
            verify_signatures(PeImage(fixture()), {0x401000: "54455355"})

    def test_unbacked_virtual_tail_and_cross_boundary_rejected(self):
        image = PeImage(fixture())
        for va, size in ((0x401040, 1), (0x40103f, 2), (0x400fff, 1), (0x401000, 0)):
            with self.subTest(va=va, size=size), self.assertRaises(ValueError):
                image.read(va, size)

    def test_truncated_invalid_or_foreign_pe_rejected(self):
        for data in (b"", b"MZ", fixture()[:-1]):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                PeImage(data)
        for offset, fmt, value in ((0x84, "<H", 0x8664), (0x98, "<H", 0x20b),
                                   (0xb4, "<I", 0x500000), (0x86, "<H", 0)):
            data = fixture()
            struct.pack_into(fmt, data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                PeImage(data)

    def test_unreviewed_client_hash_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unsupported client SHA256"):
            audit(fixture())


if __name__ == "__main__":
    unittest.main()
