from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from . import diskproto as dp
from .extract_log_mocks import extract_mocks_from_log, extract_log_mocks
import argparse


_FIXTURE = Path(__file__).resolve().parents[1] / "unittest_data" / "cat01.txt"


class TestExtractLogMocks(unittest.TestCase):
    def test_cat01_extracts_two_read_sector_mocks(self) -> None:
        if not _FIXTURE.is_file():
            self.skipTest(f"fixture missing: {_FIXTURE}")

        mocks = extract_mocks_from_log(_FIXTURE)
        self.assertEqual(len(mocks), 2)

        self.assertEqual(mocks[0].filename, "001_read_sector_0.bin")
        self.assertEqual(mocks[1].filename, "002_read_sector_1.bin")
        self.assertEqual(len(mocks[0].response_payload), 267)
        self.assertEqual(len(mocks[1].response_payload), 267)

        r0 = dp.parse_read_sector_resp(mocks[0].response_payload)
        r1 = dp.parse_read_sector_resp(mocks[1].response_payload)
        self.assertEqual(r0.lba, 0)
        self.assertEqual(r1.lba, 1)
        self.assertEqual(len(r0.data), 256)
        self.assertEqual(len(r1.data), 256)
        self.assertIn(b"lcww1", r0.data)

    def test_writes_binary_files(self) -> None:
        if not _FIXTURE.is_file():
            self.skipTest(f"fixture missing: {_FIXTURE}")

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            rc = extract_log_mocks(
                argparse.Namespace(
                    log=str(_FIXTURE),
                    output=str(out),
                    device=None,
                    command=None,
                    dry_run=False,
                )
            )
            self.assertEqual(rc, 0)
            p0 = out / "001_read_sector_0.bin"
            p1 = out / "002_read_sector_1.bin"
            self.assertTrue(p0.is_file())
            self.assertTrue(p1.is_file())
            self.assertEqual(p0.stat().st_size, 267)
            self.assertEqual(p1.stat().st_size, 267)


if __name__ == "__main__":
    unittest.main()
