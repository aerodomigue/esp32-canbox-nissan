"""Host tests for the analysis importer, independent from firmware mocks."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from analyze_can_log import counter_candidates, inspect_capture, parse_line, read_capture, write_outputs


class CaptureAnalysisTests(unittest.TestCase):
    def test_historical_and_current_formats_have_identical_bytes(self):
        old = parse_line("RX ID: 0x180 | DLC: 2 | Data: 04 FB", 1)
        current = parse_line("RX 0x180 [2]: 04 FB", 1)
        self.assertEqual(old, current)
        self.assertEqual(old.data, bytes([4, 251]))

    def test_csv_preserves_millisecond_timestamp(self):
        frame = parse_line("1706277052156,RX,0x180,2,04 FB", 7)
        self.assertEqual(frame.timestamp_ms, 1706277052156)
        self.assertEqual(frame.line, 7)

    def test_timestamped_can_and_diagnostic_raw_preserve_flags_and_bytes(self):
        regular = parse_line("RX 0x180 [2]: 04 FB | t_ms=4294967295 ext=0 rtr=0", 1)
        diagnostic = parse_line("ODB RAW_RX 0x7E8 [8]: 04 41 0C 14 18 00 00 00 | t_ms=0 ext=0 rtr=0", 2)
        self.assertEqual(regular.timestamp_ms, 0xFFFFFFFF)
        self.assertEqual(regular.data, b"\x04\xfb")
        self.assertEqual(diagnostic.data, bytes.fromhex("04 41 0C 14 18 00 00 00"))
        self.assertEqual(diagnostic.origin, "ODB RAW_RX")
        self.assertEqual(diagnostic.dlc, 8)
        self.assertFalse(diagnostic.extended)
        self.assertFalse(diagnostic.rtr)

    def test_diagnostic_metadata_does_not_count_as_malformed_can(self):
        for kind in ["RX", "TX", "SUPPORTED", "TIMEOUT", "NEGATIVE", "STATE", "DRIVER"]:
            self.assertIsNone(parse_line(f"ODB {kind} t_ms=100 seq=2", 1))

    def test_remote_frame_preserves_dlc_without_fabricating_bytes(self):
        remote = parse_line("ODB RAW_RX 0x180 [8]: | t_ms=1234 ext=0 rtr=1", 1)
        data = parse_line("RX 0x180 [2]: 04 FB | t_ms=1235 ext=0 rtr=0", 2)
        extended = parse_line("RX 0x180 [2]: FF FF | t_ms=1236 ext=1 rtr=0", 3)
        report = inspect_capture([remote, data, extended], [])
        self.assertEqual(remote.data, b"")
        self.assertEqual(remote.dlc, 8)
        self.assertEqual(report["ids"]["0x180/rtr"]["dlcs"], {8: 1})
        self.assertEqual(report["ids"]["0x180/rtr"]["bytes"], [])
        self.assertEqual(report["ids"]["0x180/ext"]["count"], 1)
        self.assertEqual(report["hypotheses"]["0x180_bytes_0_1_BE_raw"]["samples"], 1)
        self.assertEqual(report["hypotheses"]["0x180_bytes_0_1_BE_raw"]["first"], 1275)

    def test_extended_identifiers_require_explicit_valid_flags(self):
        frame = parse_line("RX 0x1FFFFFFF [0]: | t_ms=1 ext=1 rtr=0", 1)
        self.assertTrue(frame.extended)
        self.assertEqual(frame.can_id, 0x1FFFFFFF)
        for line in ["RX 0x180 [1]: FF | t_ms=1 ext=0 rtr=1",
                     "RX 0x1FFFFFFF [0]:", "RX 0x20000000 [0]: | t_ms=1 ext=1 rtr=0",
                     "RX 0x180 [0]: | t_ms=4294967296 ext=0 rtr=0",
                     "RX 0x180 [0]: | t_ms=-1 ext=0 rtr=0",
                     "RX 0x180 [0]: | t_ms=1 ext=2 rtr=0"]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_line(line, 1)

    def test_timestamped_flags_round_trip_in_canonical_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "capture.log"
            source.write_text("ODB RAW_RX 0x180 [8]: | t_ms=99 ext=0 rtr=1\n"
                              "RX 0x1FFFFFFF [2]: 01 02 | t_ms=100 ext=1 rtr=0\n"
                              "ODB TIMEOUT t_ms=150 seq=2\n")
            expected, rejected = read_capture(source)
            self.assertFalse(rejected)
            out = Path(directory) / "output"
            report = write_outputs(source, out)
            self.assertEqual(report["diagnostic_metadata"],
                             [{"line": 3, "kind": "TIMEOUT", "text": "ODB TIMEOUT t_ms=150 seq=2"}])
            frames, rejected = read_capture(out / "frames.csv")
            self.assertFalse(rejected)
            self.assertEqual([(f.can_id, f.data, f.timestamp_ms, f.dlc, f.extended, f.rtr, f.origin)
                              for f in frames],
                             [(f.can_id, f.data, f.timestamp_ms, f.dlc, f.extended, f.rtr, f.origin)
                              for f in expected])

    def test_comments_and_csv_header_are_skipped(self):
        for line in ["", " # source", "timestamp_ms,direction,id,dlc,data"]:
            self.assertIsNone(parse_line(line, 1))

    def test_malformed_and_extended_records_are_rejected(self):
        cases = [
            "RX ID: 0x180 | DLC: 8 | Data: 04 FB",
            "RX ID: 0x180 | DLC: 1 | Data: 04 FB",
            "RX ID: 0x000 | DLC: 20 | Data: 00",
            "RX ID: 0x3FC91000 | DLC: 0 | Data:",
            "RX 0x180 [2]: 04 GG", "RX 0x180 [2]: 04 100",
            "NaN,RX,0x180,2,04 FB", "-1,RX,0x180,2,04 FB",
            "100,TX,0x180,2,04 FB", "incomplete export",
            '100,RX,0x180,2,"04 FB',
        ]
        for line in cases:
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_line(line, 1)

    def test_zero_dlc_is_valid_but_cannot_fabricate_a_signal(self):
        frames = [parse_line("RX 0x180 [0]:", 1)]
        report = inspect_capture(frames, [])
        self.assertEqual(report["accepted"], 1)
        self.assertEqual(report["hypotheses"]["0x180_bytes_0_1_BE_raw"], {"samples": 0})

    def test_bad_csv_field_is_rejected_without_losing_following_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "capture.csv"
            source.write_text("100,RX,0x180,2," + "A" * 140000 + "\n"
                              "101,RX,0x180,2,04 FB\n")
            frames, rejected = read_capture(source)
            self.assertEqual(len(rejected), 1)
            self.assertIn("malformed CSV", rejected[0]["reason"])
            self.assertEqual([f.line for f in frames], [2])

    def test_counter_pattern_is_distinguished_from_constant_bits(self):
        candidates = counter_candidates([0x80 | n for n in [12, 13, 14, 15, 0, 1, 2, 3]])
        low_nibble = next(c for c in candidates if c["shift"] == 0 and c["width"] == 4)
        self.assertEqual(low_nibble["wraps"], 1)
        self.assertEqual(low_nibble["unit_step_fraction"], 1)
        self.assertFalse(counter_candidates([3] * 20))

    def test_duplicate_12bit_fields_match_in_capture_order(self):
        lines = ["RX 0x160 [3]: 34 C3 62", "RX 0x180 [4]: 04 FB 36 23"]
        report = inspect_capture([parse_line(line, n) for n, line in enumerate(lines, 1)], [])
        self.assertEqual(report["preceding_160_vs_180_12bit_copy"],
                         {"compared": 1, "equal": 1})
        self.assertEqual(report["hypotheses"]["0x180_bytes_2_3_BE_shift4"]["first"], 866)

    def test_authentic_excerpt_has_expected_rejections_and_candidates(self):
        fixture = Path(__file__).resolve().parents[1] / "test/fixtures/historical_can_excerpt.txt"
        frames, rejected = read_capture(fixture)
        report = inspect_capture(frames, rejected)
        self.assertEqual(report["accepted"], 14)
        self.assertEqual(len(rejected), 1)
        self.assertEqual(report["hypotheses"]["0x180_bytes_0_1_BE_raw"]["first"], 1275)
        self.assertEqual(report["hypotheses"]["0x180_bytes_0_1_BE_div8"]["first"], 159.375)
        self.assertEqual(report["hypotheses"]["0x2DE_bytes_6_7_BE_div10"]["first"], 375.6)

    def test_outputs_can_be_read_back_without_losing_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "capture.txt"
            source.write_text("RX 0x180 [2]: 04 FB\nRX 0x580 [2]: 00 14\n")
            out = Path(directory) / "output"
            report = write_outputs(source, out)
            decoded = json.loads((out / "analysis.json").read_text())
            canonical, rejected = read_capture(out / "frames.csv")
            self.assertEqual(report["source"]["sha256"], decoded["source"]["sha256"])
            self.assertFalse(rejected)
            self.assertEqual([f.data for f in canonical], [b"\x04\xfb", b"\x00\x14"])

    def test_output_cannot_overwrite_input(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "frames.csv"
            raw = "100,RX,0x180,2,04 FB\n"
            source.write_text(raw)
            with self.assertRaises(ValueError):
                write_outputs(source, Path(directory))
            self.assertEqual(source.read_text(), raw)

    def test_hard_link_output_cannot_overwrite_input(self):
        for name in ["analysis.json", "frames.csv"]:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                source = Path(directory) / "capture.log"
                raw = "RX 0x180 [2]: 04 FB\n"
                source.write_text(raw)
                out = Path(directory) / "output"
                out.mkdir()
                os.link(source, out / name)
                with self.assertRaises(ValueError):
                    write_outputs(source, out)
                self.assertEqual(source.read_text(), raw)

    def test_outputs_cannot_alias_each_other(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "capture.log"
            source.write_text("RX 0x180 [2]: 04 FB\n")
            out = Path(directory) / "output"
            out.mkdir()
            (out / "analysis.json").write_text("previous report\n")
            os.link(out / "analysis.json", out / "frames.csv")
            with self.assertRaises(ValueError):
                write_outputs(source, out)
            self.assertEqual((out / "analysis.json").read_text(), "previous report\n")


if __name__ == "__main__":
    unittest.main()
