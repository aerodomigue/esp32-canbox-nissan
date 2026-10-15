"""Host tests for reference grouping, millis wrap and numeric comparison."""

import json
import os
from pathlib import Path
import tempfile
import unittest

from analyze_can_log import parse_line, read_capture
from compare_obd_can import (Candidate, Reference, active_candidates, alternatives, compare,
                             millis_delta, parse_reference, read_references,
                             read_session_boundaries, write_comparison)


def reference(line, stamp, value=643.0, pid=0x0C, ecu=0x7E8, unit="rpm"):
    return Reference(line, stamp, 2, ecu - 8, ecu, pid, "test", value, unit, 1)


class ObdCanComparisonTests(unittest.TestCase):
    def test_reference_format_is_separate_from_can_raw_records(self):
        line = "ODB RX t_ms=1201 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=1"
        ref = parse_reference(line, 3)
        self.assertEqual(ref, Reference(3, 1201, 2, 0x7E0, 0x7E8, 0x0C,
                                        "rpm", 643.0, "rpm", 1))
        self.assertIsNone(parse_line(line, 3))
        self.assertIsNone(parse_reference("ODB RAW_RX 0x7E8 [0]: | t_ms=100 ext=0 rtr=0", 1))

    def test_invalid_reference_values_are_reported_as_metadata_errors(self):
        base = "ODB RX t_ms=1201 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=1"
        for line in [base.replace("643.000", "NaN"), base.replace("1201", "4294967296"),
                     base + " seq=3", base.replace("pid=0x0C", "pid=0x100"),
                     base.replace("ecu=0x7E8", "ecu=0xFFF"), base.replace("unit=rpm ", "")]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_reference(line, 1)

    def test_reference_accepts_functional_and_matching_physical_addresses(self):
        base = "ODB RX t_ms=1201 seq=2 request_id=0x7DF ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=1"
        for ecu in range(0x7E8, 0x7F0):
            functional = base.replace("ecu=0x7E8", f"ecu=0x{ecu:03X}")
            physical = functional.replace("request_id=0x7DF", f"request_id=0x{ecu - 8:03X}")
            for line in [functional, physical]:
                with self.subTest(line=line):
                    self.assertEqual(parse_reference(line, 1).ecu, ecu)

    def test_reference_rejects_impossible_transport_addresses(self):
        base = "ODB RX t_ms=1201 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=1"
        for request_id, ecu in [(0, 0x123), (0x7E0, 0x7EF), (0x7DF, 0x7E7),
                                (0x7DF, 0x7F0), (0x7D0, 0x7E8), (0x7E8, 0x7E8)]:
            line = base.replace("request_id=0x7E0", f"request_id=0x{request_id:03X}")
            line = line.replace("ecu=0x7E8", f"ecu=0x{ecu:03X}")
            with self.subTest(line=line), self.assertRaises(ValueError):
                parse_reference(line, 1)

    def test_nearest_frame_obeys_delay_limit_and_separate_pid_ecu_streams(self):
        frames = [parse_line("RX 0x180 [2]: 14 18 | t_ms=100 ext=0 rtr=0", 1),
                  parse_line("RX 0x180 [2]: 1F 40 | t_ms=300 ext=0 rtr=0", 5)]
        refs = [reference(2, 101), reference(3, 101, 700, ecu=0x7E9),
                reference(4, 101, 15, pid=0x0D, unit="km/h"), reference(6, 1000)]
        candidate = Candidate("rpm", "test", 0x0C, "rpm", 0x180, 0, 2, 8)
        report, matches = compare(frames, refs, [candidate], max_delay_ms=200)
        self.assertEqual(len(report["reference_streams"]), 3)
        rows = {r["ecu"]: r for r in report["comparisons"]}
        self.assertEqual(rows["0x7E8"]["matched"], 1)
        self.assertEqual(rows["0x7E8"]["unmatched"], 1)
        self.assertEqual(rows["0x7E8"]["mean_absolute_error"], 0)
        self.assertEqual(rows["0x7E9"]["mean_absolute_error"], 57)
        self.assertEqual({(p["pid"], p["ecu"]) for p in matches}, {("0x0C", "0x7E8"), ("0x0C", "0x7E9")})

    def test_wrap_safe_matching_and_out_of_window_after_wrap(self):
        self.assertEqual(millis_delta(2, 0xFFFFFFFE), 4)
        self.assertEqual(millis_delta(0xFFFFFFFE, 2), -4)
        frames = [parse_line("RX 0x180 [2]: 14 18 | t_ms=4294967294 ext=0 rtr=0", 1)]
        candidate = Candidate("rpm", "test", 0x0C, "rpm", 0x180, 0, 2, 8)
        report, matches = compare(frames, [reference(2, 2), reference(3, 1000)], [candidate])
        self.assertEqual(report["comparisons"][0]["matched"], 1)
        self.assertEqual(matches[0]["delta_ms"], -4)
        self.assertEqual(matches[0]["can_value"], 643)

    def test_delay_boundary_is_inclusive_and_fuel_rate_is_not_converted(self):
        frames = [parse_line("RX 0x180 [2]: 14 18 | t_ms=100 ext=0 rtr=0", 1)]
        refs = [reference(2, 300), reference(3, 301),
                reference(4, 302, 0.6, pid=0x5E, unit="L/h")]
        candidate = Candidate("rpm", "test", 0x0C, "rpm", 0x180, 0, 2, 8)
        report, matches = compare(frames, refs, [candidate], max_delay_ms=200)
        self.assertEqual(report["comparisons"][0]["matched"], 1)
        self.assertEqual(matches[0]["delta_ms"], -200)
        self.assertEqual(report["reference_streams_without_candidate"],
                         [{"session": 0, "pid": "0x5E", "ecu": "0x7E8", "unit": "L/h", "samples": 1}])

    def test_remote_extended_and_untimestamped_frames_cannot_match(self):
        frames = [parse_line("RX 0x180 [8]: | t_ms=100 ext=0 rtr=1", 1),
                  parse_line("RX 0x180 [2]: 14 18 | t_ms=100 ext=1 rtr=0", 2),
                  parse_line("RX 0x180 [2]: 14 18", 3),
                  parse_line("1706277052156,RX,0x180,2,14 18", 4)]
        report, matches = compare(frames, [reference(5, 100)], alternatives())
        self.assertFalse(matches)
        self.assertEqual(report["excluded_frames"], 4)
        self.assertTrue(all(r["matched"] == 0 for r in report["comparisons"]))

    def test_mismatched_units_remain_visible_without_numeric_error(self):
        frame = parse_line("RX 0x5C5 [1]: 40 | t_ms=100 ext=0 rtr=0", 1)
        candidate = Candidate("fuel liters", "test", 0x2F, "L", 0x5C5, 0, 1)
        report, matches = compare([frame], [reference(2, 101, 50, pid=0x2F, unit="percent")], [candidate])
        self.assertFalse(report["comparisons"][0]["unit_match"])
        self.assertIsNone(report["comparisons"][0]["mean_absolute_error"])
        self.assertEqual(matches[0]["can_value"], 64)
        self.assertIsNone(matches[0]["error"])

    def test_active_preset_uses_integer_rounding_and_globaldata_voltage_unit(self):
        preset = {"frames": [
            {"canId": "0x180", "fields": [{"target": "ENGINE_RPM", "startByte": 0,
                "byteCount": 2, "formula": "SCALE", "params": [1, 8, 0]}]},
            {"canId": "0x6F6", "fields": [{"target": "VOLTAGE", "startByte": 0,
                "byteCount": 1, "formula": "NONE"}]},
            {"canId": "0x5C5", "fields": [{"target": "FUEL_LEVEL", "startByte": 0,
                "byteCount": 1, "formula": "MAP_RANGE", "params": [255, -70, 0, 40]}]},
        ]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "preset.json"
            path.write_text(json.dumps(preset))
            candidates = active_candidates(path)
        self.assertEqual(candidates[0].value(parse_line("RX 0x180 [2]: 04 FB", 1)), 159)
        self.assertAlmostEqual(candidates[1].value(parse_line("RX 0x6F6 [1]: 8D", 1)), 14.1)
        self.assertEqual(candidates[2].value(parse_line("RX 0x5C5 [1]: 40", 1)), 23)

    def test_active_fuel_candidate_requires_odometer_bytes_for_atomic_frame(self):
        preset = {"frames": [{"canId": "0x5C5", "fields": [
            {"target": "FUEL_LEVEL", "startByte": 0, "byteCount": 1, "formula": "NONE"},
            {"target": "ODOMETER", "startByte": 1, "byteCount": 3, "formula": "NONE"},
        ]}]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "preset.json"
            path.write_text(json.dumps(preset))
            candidates = active_candidates(path)
        frame = parse_line("RX 0x5C5 [1]: 40 | t_ms=100 ext=0 rtr=0", 1)
        self.assertEqual(candidates[0].required_dlc, 4)
        with self.assertRaises(ValueError):
            candidates[0].value(frame)
        report, pairs = compare([frame], [reference(2, 101, 50, pid=0x2F, unit="percent")], candidates)
        self.assertFalse(pairs)
        self.assertEqual(report["comparisons"][0]["candidate_frames"], 0)

    def test_unsigned_type_does_not_mask_more_than_production_extraction(self):
        # Production casts INT8/INT16 explicitly; UINT8 falls through int32_t.
        # Its byteCount may be two, and the formula uses the full raw value.
        candidate = Candidate("preset unsigned", "active_preset", 0x0C, "rpm", 0x180, 0, 2,
            field={"dataType": "UINT8", "formula": "SCALE", "params": [1, 8, 0]}, target="ENGINE_RPM")
        self.assertEqual(candidate.value(parse_line("RX 0x180 [2]: 01 28", 1)), 37)

    def test_state_boundaries_prevent_cross_session_matches_and_merged_errors(self):
        frames = [parse_line("RX 0x180 [2]: 14 18 | t_ms=100 ext=0 rtr=0", 1),
                  parse_line("RX 0x180 [2]: 1F 40 | t_ms=100 ext=0 rtr=0", 6)]
        refs = [reference(2, 101), reference(5, 101, 1000)]
        candidate = Candidate("rpm", "test", 0x0C, "rpm", 0x180, 0, 2, 8)
        report, pairs = compare(frames, refs, [candidate], session_boundaries=[3, 4])
        self.assertEqual([(p["reference_line"], p["can_line"], p["session"]) for p in pairs],
                         [(2, 1, 0), (5, 6, 2)])
        self.assertEqual(len(report["comparisons"]), 2)
        self.assertTrue(all(r["mean_absolute_error"] == 0 for r in report["comparisons"]))
        report, pairs = compare(frames[:1], refs[1:], [candidate], session_boundaries=[3, 4])
        self.assertFalse(pairs)
        self.assertEqual(report["comparisons"][0]["matched"], 0)

    def test_session_boundary_reader_distinguishes_log_changes_from_retries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.log"
            path.write_text("ODB STATE t_ms=10 enabled=1 status=discovery\n"
                            "ODB STATE t_ms=20 status=no_supported_response retry_ms=5000\n"
                            "ODB STATE t_ms=30 enabled=0 status=stopped\n"
                            "ODB STATE t_ms=40 enabled=1 status=discovery\n"
                            "ODB STATE t_ms=0 status=reboot\n"
                            "--- ESP32 BOOT (F15 Gateway) ---\n")
            self.assertEqual(read_session_boundaries(path), [1, 3, 4, 5, 6])

    def test_mixed_capture_generates_outputs_and_preserves_ecu_filter(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture, preset = root / "capture.txt", root / "preset.json"
            capture.write_text("ODB STATE t_ms=99 enabled=1 status=discovery\n"
                               "RX 0x180 [2]: 14 18 | t_ms=100 ext=0 rtr=0\n"
                               "ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=101 ext=0 rtr=0\n"
                               "ODB RX t_ms=101 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643 unit=rpm latency_ms=1\n"
                               "ODB RX t_ms=102 seq=2 request_id=0x7E1 ecu=0x7E9 pid=0x0C name=rpm value=700 unit=rpm latency_ms=2\n"
                               "ODB TIMEOUT t_ms=200 seq=3\n"
                               "ODB RX t_ms=201 incomplete\n")
            preset.write_text('{"frames": []}')
            report = write_comparison(capture, preset, root / "output", ecu=0x7E8)
            frames, rejected = read_capture(capture)
            refs, rejected_refs, categories = read_references(capture)
            self.assertEqual(len(frames), 2)
            self.assertFalse(rejected)
            self.assertEqual(len(refs), 2)
            self.assertEqual(len(rejected_refs), 1)
            self.assertEqual(categories, {"RAW_RX": 1, "RX": 3, "STATE": 1, "TIMEOUT": 1})
            self.assertEqual(report["session_boundary_lines"], [1])
            self.assertEqual(report["reference_streams"][0]["session"], 1)
            self.assertEqual(report["reference_streams"][0]["ecu"], "0x7E8")
            self.assertTrue((root / "output/matches.csv").exists())
            stored = json.loads((root / "output/comparison.json").read_text())
            self.assertEqual(len(stored["rejected_references"]), 1)

    def test_comparison_outputs_cannot_overwrite_capture_via_hard_link(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture, preset, out = root / "capture.txt", root / "preset.json", root / "output"
            raw = "RX 0x180 [2]: 14 18 | t_ms=100 ext=0 rtr=0\n"
            capture.write_text(raw)
            preset.write_text('{"frames": []}')
            out.mkdir()
            os.link(capture, out / "comparison.json")
            with self.assertRaises(ValueError):
                write_comparison(capture, preset, out)
            self.assertEqual(capture.read_text(), raw)


if __name__ == "__main__":
    unittest.main()
