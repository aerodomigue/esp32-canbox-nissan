#!/usr/bin/env python3
"""Compare timestamped CAN candidates with separate ODB Mode 01 references.

Pair each reference with its nearest eligible CAN frame within --max-delay-ms.
References remain separate by PID, ECU and unit. Firmware t_ms is dequeue time,
not a hardware wire timestamp. Timing and agreement do not prove calibration;
the tool never changes the vehicle preset. Explicit LOG state/boot markers
separate sessions; use separate files for captures with unmarked restarts.
"""

from __future__ import annotations

import argparse
from bisect import bisect_left, bisect_right
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import re
import statistics

from analyze_can_log import Frame, read_capture, series_stats, word


UINT32 = 1 << 32


@dataclass(frozen=True)
class Reference:
    line: int
    timestamp_ms: int
    seq: int
    request_id: int
    ecu: int
    pid: int
    name: str
    value: float
    unit: str
    latency_ms: int


@dataclass(frozen=True)
class Candidate:
    name: str
    origin: str
    pid: int
    unit: str
    can_id: int
    start: int
    length: int
    divisor: float = 1
    offset: float = 0
    shift: int = 0
    field: dict | None = None
    target: str | None = None
    required_dlc: int = 0

    def value(self, frame: Frame) -> float:
        if len(frame.data) < max(self.start + self.length, self.required_dlc):
            raise ValueError("frame lacks bytes required by its full preset definition")
        if self.field is None:
            return (word(frame.data, self.start, self.length) >> self.shift) / self.divisor + self.offset
        field = self.field
        raw = int.from_bytes(frame.data[self.start:self.start + self.length],
                             "little" if field.get("byteOrder") == "LE" else "big")
        bits = {"INT8": 8, "INT16": 16}.get(field.get("dataType"), 32)
        raw &= (1 << bits) - 1
        if raw & (1 << (bits - 1)):
            raw -= 1 << bits
        params = field.get("params", [])
        formula = field.get("formula", "NONE")
        if formula == "SCALE":
            value = trunc_div(raw * (params[0] or 1), params[1] or 1) + params[2]
        elif formula == "MAP_RANGE":
            value = trunc_div((raw - params[0]) * (params[3] - params[2]),
                              params[1] - params[0]) + params[2]
        elif formula == "BITMASK_EXTRACT":
            value = (raw & params[0]) >> params[1]
        else:
            value = raw
        # Match production GlobalData assignment for the comparable numeric fields.
        if self.target == "ENGINE_RPM":
            value &= 0xFFFF
        elif self.target in {"VEHICLE_SPEED", "FUEL_LEVEL"}:
            value &= 0xFF
        elif self.target in {"COOLANT_TEMP", "TEMPERATURE"}:
            value = (value + 128) % 256 - 128
        elif self.target == "VOLTAGE":
            return value * 0.1
        return float(value)


def trunc_div(numerator: int, denominator: int) -> int:
    """C++ signed integer division, without float precision loss."""
    if not denominator:
        raise ValueError("preset formula has a zero denominator")
    quotient = abs(numerator) // abs(denominator)
    return -quotient if (numerator < 0) != (denominator < 0) else quotient


def millis_delta(a: int, b: int) -> int:
    """Signed a-b across uint32 millis wrap, valid within half a wrap."""
    return (a - b + UINT32 // 2) % UINT32 - UINT32 // 2


def normalized_unit(unit: str) -> str:
    return {"degC": "C", "°C": "C", "kmh": "km/h", "%": "percent"}.get(unit, unit)


def parse_reference(line: str, number: int) -> Reference | None:
    if not line.startswith("ODB RX "):
        return None
    parts = line.strip().split()[2:]
    fields = {}
    for part in parts:
        if "=" not in part:
            raise ValueError("diagnostic reference requires key=value fields")
        key, value = part.split("=", 1)
        if key in fields:
            raise ValueError("duplicate diagnostic reference key")
        fields[key] = value
    required = {"t_ms", "seq", "request_id", "ecu", "pid", "name", "value", "unit", "latency_ms"}
    if not required <= fields.keys():
        raise ValueError("incomplete diagnostic reference")
    integers = {}
    for key in ["t_ms", "seq", "latency_ms"]:
        if not re.fullmatch(r"\d+", fields[key]):
            raise ValueError(f"diagnostic {key} must be uint32")
        integers[key] = int(fields[key])
        if integers[key] >= UINT32:
            raise ValueError(f"diagnostic {key} outside uint32 range")
    for key, maximum in [("request_id", 0x7FF), ("ecu", 0x7FF), ("pid", 0xFF)]:
        if not re.fullmatch(r"0x[0-9a-fA-F]+", fields[key]):
            raise ValueError(f"diagnostic {key} must use 0x hexadecimal notation")
        integers[key] = int(fields[key], 16)
        if integers[key] > maximum:
            raise ValueError(f"diagnostic {key} outside supported range")
    request_id, ecu = integers["request_id"], integers["ecu"]
    if not 0x7E8 <= ecu <= 0x7EF:
        raise ValueError("diagnostic ECU must be in 0x7E8..0x7EF")
    if request_id != 0x7DF and not (0x7E0 <= request_id <= 0x7E7 and ecu == request_id + 8):
        raise ValueError("diagnostic request/ECU addresses do not match")
    value = float(fields["value"])
    if not math.isfinite(value):
        raise ValueError("diagnostic value must be finite")
    return Reference(number, integers["t_ms"], integers["seq"], integers["request_id"],
                     integers["ecu"], integers["pid"], fields["name"], value,
                     normalized_unit(fields["unit"]), integers["latency_ms"])


def read_references(path: Path) -> tuple[list[Reference], list[dict], dict]:
    references, rejected, categories = [], [], Counter()
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.startswith("ODB "):
            continue
        category = line.split()[1] if len(line.split()) > 1 else "UNKNOWN"
        categories[category] += 1
        try:
            reference = parse_reference(line, number)
            if reference is not None:
                references.append(reference)
        except ValueError as exc:
            rejected.append({"line": number, "reason": str(exc)})
    return references, rejected, dict(sorted(categories.items()))


def read_session_boundaries(path: Path) -> list[int]:
    """Split explicit LOG state changes and firmware boot markers, not retries."""
    result = []
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        state_change = line.startswith("ODB STATE ") and (
            re.search(r"(?:^|\s)enabled=[01](?:\s|$)", line) or
            re.search(r"(?:^|\s)status=(?:boot|reboot|reset)(?:\s|$)", line))
        if state_change or line.startswith("--- ESP32 BOOT"):
            result.append(number)
    return result


def alternatives() -> list[Candidate]:
    candidates = []
    for divisor in [1, 7, 8]:
        candidates.append(Candidate(f"0x180 bytes0-1 BE /{divisor}", "alternative", 0x0C,
                                    "rpm", 0x180, 0, 2, divisor))
    candidates += [
        Candidate("0x180 bytes2-3 BE >>4", "alternative", 0x0C, "rpm", 0x180, 2, 2, shift=4),
        Candidate("0x1F9 bytes2-3 BE /8", "alternative", 0x0C, "rpm", 0x1F9, 2, 2, 8),
        Candidate("0x284 bytes0-1 BE /100", "alternative", 0x0D, "km/h", 0x284, 0, 2, 100),
        Candidate("0x284 bytes4-5 BE /100", "alternative", 0x0D, "km/h", 0x284, 4, 2, 100),
        Candidate("0x280 bytes4-5 BE /100", "alternative", 0x0D, "km/h", 0x280, 4, 2, 100),
        Candidate("0x551 byte0 -40", "alternative", 0x05, "C", 0x551, 0, 1, offset=-40),
        Candidate("0x6F6 byte0 /10", "alternative", 0x42, "V", 0x6F6, 0, 1, 10),
        Candidate("0x540 byte0 -40", "alternative", 0x46, "C", 0x540, 0, 1, offset=-40),
    ]
    return candidates


TARGETS = {
    "ENGINE_RPM": (0x0C, "rpm"), "VEHICLE_SPEED": (0x0D, "km/h"),
    "COOLANT_TEMP": (0x05, "C"), "VOLTAGE": (0x42, "V"),
    "TEMPERATURE": (0x46, "C"), "FUEL_LEVEL": (0x2F, "L"),
}


def active_candidates(path: Path) -> list[Candidate]:
    preset = json.loads(path.read_text())
    result = []
    for frame in preset.get("frames", []):
        can_id = int(frame["canId"], 0) if isinstance(frame["canId"], str) else frame["canId"]
        required_dlc = 0
        # The firmware rejects the whole frame before any write if any defined
        # field is incomplete, including fields not comparable to an ODB PID.
        for field in frame.get("fields", []):
            start, length = field.get("startByte", 0), field.get("byteCount", 1)
            if not isinstance(start, int) or not isinstance(length, int) or not 0 <= start < 8 or not 1 <= length <= 4 or start + length > 8:
                raise ValueError("preset field outside classic CAN payload")
            required_dlc = max(required_dlc, start + length)
        for field in frame.get("fields", []):
            target = field.get("target")
            if target not in TARGETS:
                continue
            start, length = field.get("startByte", 0), field.get("byteCount", 1)
            if not isinstance(start, int) or not isinstance(length, int) or not 0 <= start < 8 or not 1 <= length <= 4 or start + length > 8:
                raise ValueError("preset comparable field outside classic CAN payload")
            if not isinstance(can_id, int) or not 0 <= can_id <= 0x7FF:
                raise ValueError("preset CAN ID outside standard range")
            if field.get("byteOrder", "BE") not in {"BE", "LE"}:
                raise ValueError("unsupported preset byte order")
            formula = field.get("formula", "NONE")
            expected = {"NONE": 0, "SCALE": 3, "MAP_RANGE": 4, "BITMASK_EXTRACT": 2}.get(formula)
            params = field.get("params", [])
            if expected is None or len(params) < expected or any(not isinstance(v, int) for v in params):
                raise ValueError("unsupported or incomplete preset formula")
            if formula == "MAP_RANGE" and params[0] == params[1]:
                raise ValueError("preset MAP_RANGE has a zero denominator")
            if formula == "BITMASK_EXTRACT" and not 0 <= params[1] < 32:
                raise ValueError("preset bit shift outside supported range")
            if field.get("dataType", "UINT8") not in {"UINT8", "UINT16", "UINT24", "UINT32", "INT8", "INT16", "INT32"}:
                raise ValueError("unsupported preset numeric data type")
            pid, unit = TARGETS[target]
            result.append(Candidate(f"preset {target} 0x{can_id:03X} bytes{start}-{start + length - 1}",
                                    "active_preset", pid, unit, can_id, start, length,
                                    field=field, target=target, required_dlc=required_dlc))
    return result


def compare(frames: list[Frame], references: list[Reference], candidates: list[Candidate],
            max_delay_ms: int = 200, session_boundaries: list[int] | None = None) -> tuple[dict, list[dict]]:
    if not 0 <= max_delay_ms < UINT32 // 2:
        raise ValueError("max delay must be nonnegative and less than half a millis wrap")
    # Unwrap a single log's uint32 timestamps in record order. Small backwards
    # steps retain ordering information without pretending to be another wrap.
    eligible = [f for f in frames if not f.extended and not f.rtr and f.timestamp_ms is not None
                and 0 <= f.timestamp_ms < UINT32 and f.timestamp_ms == int(f.timestamp_ms)]
    boundaries = sorted(set(session_boundaries or []))
    sessions = {entry.line: bisect_right(boundaries, entry.line) for entry in [*eligible, *references]}
    events = sorted([(f.line, int(f.timestamp_ms)) for f in eligible] +
                    [(r.line, r.timestamp_ms) for r in references])
    times, previous, previous_session, timeline = {}, None, None, 0
    for line, stamp in events:
        session = sessions[line]
        timeline = stamp if previous is None or session != previous_session else timeline + millis_delta(stamp, previous)
        times[line] = timeline
        previous, previous_session = stamp, session
    streams = defaultdict(list)
    for ref in references:
        streams[(sessions[ref.line], ref.pid, ref.ecu, ref.unit)].append(ref)
    summaries, matches = [], []
    for (session, pid, ecu, unit), refs in sorted(streams.items()):
        for candidate in candidates:
            if candidate.pid != pid:
                continue
            samples = sorted((times[f.line], f.line, f) for f in eligible
                             if sessions[f.line] == session and f.can_id == candidate.can_id
                             and len(f.data) >= max(candidate.start + candidate.length, candidate.required_dlc))
            sample_times = [t for t, _, _ in samples]
            paired, errors, values, delays = [], [], [], []
            unit_match = normalized_unit(candidate.unit) == unit
            for ref in refs:
                ref_time = times[ref.line]
                i = bisect_left(sample_times, ref_time)
                nearby = samples[max(0, i - 1):min(len(samples), i + 1)]
                if not nearby:
                    continue
                time, _, frame = min(nearby, key=lambda item: (abs(item[0] - ref_time), item[1]))
                delay = time - ref_time
                if abs(delay) > max_delay_ms:
                    continue
                value = candidate.value(frame)
                error = value - ref.value if unit_match else None
                pair = {"candidate": candidate.name, "origin": candidate.origin,
                        "session": session,
                        "pid": f"0x{pid:02X}", "ecu": f"0x{ecu:03X}",
                        "reference_line": ref.line, "can_line": frame.line,
                        "reference_t_ms": ref.timestamp_ms, "can_t_ms": int(frame.timestamp_ms),
                        "seq": ref.seq, "request_id": f"0x{ref.request_id:03X}",
                        "delta_ms": delay, "can_value": value, "can_unit": candidate.unit,
                        "reference_value": ref.value, "reference_unit": unit, "error": error}
                paired.append(pair)
                values.append(value)
                delays.append(abs(delay))
                if error is not None:
                    errors.append(error)
            matches.extend(paired)
            summaries.append({"candidate": candidate.name, "origin": candidate.origin,
                              "session": session,
                              "pid": f"0x{pid:02X}", "ecu": f"0x{ecu:03X}",
                              "can_unit": candidate.unit, "reference_unit": unit,
                              "unit_match": unit_match, "reference_samples": len(refs),
                              "candidate_frames": len(samples), "matched": len(paired),
                              "unmatched": len(refs) - len(paired),
                              "distinct_matched_frames": len({p["can_line"] for p in paired}),
                              "max_observed_delay_ms": max(delays) if delays else None,
                              "can_values": series_stats(values),
                              "mean_error": statistics.mean(errors) if errors else None,
                              "mean_absolute_error": statistics.mean(map(abs, errors)) if errors else None,
                              "max_absolute_error": max(map(abs, errors)) if errors else None,
                              "rmse": math.sqrt(statistics.mean(e * e for e in errors)) if errors else None})
    return {"reference_streams": [{"session": s, "pid": f"0x{p:02X}", "ecu": f"0x{e:03X}", "unit": u,
                                   "samples": len(rs)} for (s, p, e, u), rs in sorted(streams.items())],
            "reference_streams_without_candidate": [
                {"session": s, "pid": f"0x{p:02X}", "ecu": f"0x{e:03X}", "unit": u, "samples": len(rs)}
                for (s, p, e, u), rs in sorted(streams.items())
                if not any(candidate.pid == p for candidate in candidates)
            ],
            "max_delay_ms": max_delay_ms, "comparisons": summaries,
            "session_boundary_lines": boundaries,
            "excluded_frames": len(frames) - len(eligible),
            "timing_note": "t_ms is firmware dequeue time. Nearest samples use uint32 millis wrap; log gaps must be less than half a wrap.",
            "session_note": "Explicit ODB STATE enabled changes and ESP32 boot markers split sessions; references never match across them. Unmarked reboots or concatenated captures need separate input files.",
            "calibration_note": "Numeric agreement is evidence for review, not automatic calibration. Constant captures cannot establish scale. Units that differ are never compared numerically."}, matches


def write_comparison(capture: Path, preset: Path, destination: Path,
                     max_delay_ms: int = 200, ecu: int | None = None) -> dict:
    outputs = [destination / "comparison.json", destination / "matches.csv"]
    def aliases(a, b):
        return a.resolve() == b.resolve() or (a.exists() and b.exists() and a.samefile(b))
    if any(aliases(source, output) for source in [capture, preset] for output in outputs) or aliases(*outputs):
        raise ValueError("comparison outputs must not alias inputs or each other")
    frames, rejected = read_capture(capture)
    references, rejected_refs, categories = read_references(capture)
    if ecu is not None:
        references = [r for r in references if r.ecu == ecu]
    if not references:
        raise ValueError("capture contains no valid ODB RX references for the selected ECU")
    report, matches = compare(frames, references, active_candidates(preset) + alternatives(),
                              max_delay_ms, read_session_boundaries(capture))
    report.update({"source": {"path": str(capture), "sha256": hashlib.sha256(capture.read_bytes()).hexdigest()},
                   "preset": {"path": str(preset), "sha256": hashlib.sha256(preset.read_bytes()).hexdigest()},
                   "rejected_can_records": rejected, "rejected_references": rejected_refs,
                   "diagnostic_record_categories": categories})
    destination.mkdir(parents=True, exist_ok=True)
    outputs[0].write_text(json.dumps(report, indent=2) + "\n")
    with outputs[1].open("w", newline="") as stream:
        columns = ["candidate", "origin", "session", "pid", "ecu", "reference_line", "can_line",
                   "reference_t_ms", "can_t_ms", "seq", "request_id", "delta_ms",
                   "can_value", "can_unit", "reference_value", "reference_unit", "error"]
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(matches)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--preset", type=Path, default=Path(__file__).resolve().parents[1] / "data/NissanJukeF15.json")
    parser.add_argument("--output-dir", type=Path, default=Path(".local-validation/obd-comparison"))
    parser.add_argument("--max-delay-ms", type=int, default=200)
    parser.add_argument("--ecu", type=lambda value: int(value, 0), help="optional responder ECU filter, e.g. 0x7E8")
    args = parser.parse_args()
    try:
        report = write_comparison(args.capture, args.preset, args.output_dir, args.max_delay_ms, args.ecu)
    except (OSError, ValueError, TypeError, KeyError) as exc:
        parser.error(str(exc))
    for row in report["comparisons"]:
        error = f"MAE={row['mean_absolute_error']:.3f} {row['can_unit']}" if row["mean_absolute_error"] is not None else "no comparable values"
        print(f"ODB COMPARE session={row['session']} {row['ecu']} {row['pid']} {row['candidate']}: {row['matched']}/{row['reference_samples']} matched; {error}")
    for stream in report["reference_streams_without_candidate"]:
        print(f"ODB NOTE session={stream['session']} {stream['ecu']} {stream['pid']}: {stream['samples']} references retained; no CAN candidate configured for {stream['unit']}.")
    print(f"ODB OUTPUT comparison={args.output_dir / 'comparison.json'}")
    print(f"ODB OUTPUT pairs={args.output_dir / 'matches.csv'}")
    print(f"ODB NOTE {report['session_note']}")
    print(f"ODB NOTE {report['calibration_note']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
