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
