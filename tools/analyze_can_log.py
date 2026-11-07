#!/usr/bin/env python3
"""Inspect standard CAN exports without asserting physical signal calibration.

Supports historical ``RX ID: ... | DLC: ... | Data: ...`` records, current
``RX 0x180 [8]: ...`` records, and CANBox CSV (timestamp_ms,RX,id,dlc,data).
Timestamped RX and ``ODB RAW_RX`` records may carry explicit ext/rtr flags.
Extended and remote frames are retained separately; hypotheses use standard
data frames only. Sample order is not time; t_ms is firmware dequeue time.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import re


HISTORICAL = re.compile(
    r"RX ID:\s*(0x[0-9a-f]+)\s*\|\s*DLC:\s*(\d+)\s*\|\s*Data:\s*(.*)", re.I
)
CURRENT = re.compile(r"(RX|ODB RAW_RX)\s+(0x[0-9a-f]+)\s*\[(\d+)\]:\s*(.*)", re.I)
FRAME_METADATA = re.compile(r"t_ms=(\d+)\s+ext=([01])\s+rtr=([01])")
BYTE = re.compile(r"[0-9a-f]{2}", re.I)


@dataclass(frozen=True)
class Frame:
    line: int
    can_id: int
    data: bytes
    timestamp_ms: float | None = None
    dlc: int | None = None
    extended: bool = False
    rtr: bool = False
    origin: str = "RX"

    def __post_init__(self):
        if self.dlc is None:
            object.__setattr__(self, "dlc", len(self.data))


def parse_line(line: str, number: int) -> Frame | None:
    """Return a frame, skip a comment/header, or raise on a malformed record."""
    line = line.strip()
    if not line or line.startswith("#"):
        return None
    # Diagnostics are a distinct record family. Only RAW_RX contains a CAN frame.
    if line.startswith("ODB ") and not line.startswith("ODB RAW_RX "):
        return None
    timestamp = None
    extended, rtr, origin = False, False, "RX"
    historical, current = HISTORICAL.fullmatch(line), CURRENT.fullmatch(line)
    if historical:
        identifier, dlc_text, payload = historical.groups()
    elif current:
        origin, identifier, dlc_text, payload = current.groups()
        origin = origin.upper()
        if "|" in payload:
            payload, metadata = payload.split("|", 1)
            flags = FRAME_METADATA.fullmatch(metadata.strip())
            if not flags:
                raise ValueError("expected t_ms=<uint32> ext=0|1 rtr=0|1")
            timestamp = int(flags[1])
            if timestamp > 0xFFFFFFFF:
                raise ValueError("t_ms outside uint32 millis range")
            extended, rtr = bool(int(flags[2])), bool(int(flags[3]))
    elif "," in line:
        try:
            row = next(csv.reader([line], strict=True))
        except csv.Error as exc:
            raise ValueError(f"malformed CSV: {exc}") from exc
        if row and row[0].strip().lower() in {"timestamp", "timestamp_ms"}:
            return None
        if len(row) not in {5, 6, 8, 9} or row[1].strip().upper() != "RX":
            raise ValueError("expected timestamp_ms,RX,can_id,dlc,data")
        if row[0].strip():
            timestamp = float(row[0])
            if not math.isfinite(timestamp) or timestamp < 0:
                raise ValueError("timestamp must be a finite nonnegative millisecond value")
        identifier, dlc_text, payload = (part.strip() for part in row[2:5])
        if len(row) >= 8:
            if row[6].strip() not in {"0", "1"} or row[7].strip() not in {"0", "1"}:
                raise ValueError("CSV ext/rtr flags must be 0 or 1")
            extended, rtr = row[6].strip() == "1", row[7].strip() == "1"
        if len(row) == 9:
            origin = row[8].strip()
            if origin not in {"RX", "ODB RAW_RX"}:
                raise ValueError("unsupported CSV frame origin")
    else:
        raise ValueError("unsupported or incomplete CAN record")
    if not re.fullmatch(r"0x[0-9a-f]+", identifier, re.I):
        raise ValueError("CAN ID must use hexadecimal 0x notation")
    can_id = int(identifier, 16)
    dlc = int(dlc_text)
    if not 0 <= can_id <= (0x1FFFFFFF if extended else 0x7FF):
        raise ValueError("CAN ID outside explicit extended 29-bit range" if extended
                         else "CAN ID outside standard 11-bit range")
    if not 0 <= dlc <= 8:
        raise ValueError("DLC outside classic CAN range 0..8")
    tokens = payload.split()
    if rtr and tokens:
        raise ValueError("RTR record must not contain a data payload")
    if (not rtr and len(tokens) != dlc) or any(not BYTE.fullmatch(token) for token in tokens):
        raise ValueError("payload bytes do not match DLC or contain invalid hex")
    return Frame(number, can_id, bytes(int(token, 16) for token in tokens), timestamp,
                 dlc, extended, rtr, origin)


def read_capture(path: Path) -> tuple[list[Frame], list[dict]]:
    frames, rejected = [], []
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        try:
            frame = parse_line(line, number)
            if frame is not None:
                frames.append(frame)
        except ValueError as exc:
            rejected.append({"line": number, "reason": str(exc)})
    return frames, rejected


def series_stats(values: list[int | float]) -> dict:
    if not values:
        return {"samples": 0}
    unique = sorted(set(values))
    return {
        "samples": len(values), "min": min(values), "max": max(values),
        "first": values[0], "last": values[-1], "unique_count": len(unique),
        "changes": sum(a != b for a, b in zip(values, values[1:])),
        "unique_values": unique if len(unique) <= 32 else None,
    }


def counter_candidates(values: list[int]) -> list[dict]:
    """Report modulo patterns, never infer a physical counter automatically."""
    result = []
    if len(values) < 6:
        return result
    for shift, width in [(0, 2), (0, 4), (1, 2), (4, 4), (0, 8)]:
        modulus = 1 << width
        samples = [(value >> shift) & (modulus - 1) for value in values]
        steps = [(b - a) % modulus for a, b in zip(samples, samples[1:])]
        wraps = sum(b < a for a, b in zip(samples, samples[1:]))
        unit_fraction = steps.count(1) / len(steps)
        if len(set(samples)) >= 3 and wraps and unit_fraction >= 0.75:
            result.append({
                "shift": shift, "width": width, "wraps": wraps,
                "unit_step_fraction": round(unit_fraction, 6),
                "step_counts": dict(sorted(Counter(steps).items())),
                "interpretation": "counter-like pattern; semantics unverified",
            })
    return result


def word(data: bytes, start: int, length: int = 2, order: str = "big") -> int:
    if start + length > len(data):
        raise ValueError("incomplete field")
    return int.from_bytes(data[start:start + length], order)


def inspect_capture(frames: list[Frame], rejected: list[dict]) -> dict:
    groups = defaultdict(list)
    physical_groups = defaultdict(list)
    for frame in frames:
        groups[(frame.can_id, frame.extended, frame.rtr)].append(frame)
        if not frame.extended and not frame.rtr:
            physical_groups[frame.can_id].append(frame)
    per_id = {}
    for (identifier, extended, rtr), records in sorted(groups.items()):
        byte_stats, windows = [], []
        for offset in range(max(map(lambda f: len(f.data), records))):
            values = [f.data[offset] for f in records if len(f.data) > offset]
            variation = 0
            for value in values:
                variation |= value ^ values[0]
            byte_stats.append({"offset": offset, **series_stats(values),
                               "varying_bits": f"0x{variation:02X}",
                               "counter_candidates": counter_candidates(values)})
            for order in ("big", "little"):
                values = [word(f.data, offset, order=order) for f in records
                          if len(f.data) >= offset + 2]
                if values:
                    windows.append({"offset": offset, "length": 2,
                                    "order": order, **series_stats(values)})
        key = f"0x{identifier:03X}" + ("/ext" if extended else "") + ("/rtr" if rtr else "")
        per_id[key] = {
            "count": len(records), "dlcs": dict(Counter(f.dlc for f in records)),
            "extended": extended, "rtr": rtr,
            "origins": dict(Counter(f.origin for f in records)),
            "first_line": records[0].line, "last_line": records[-1].line,
            "bytes": byte_stats, "unsigned_16bit_windows": windows,
        }

    def signal(identifier, start, length=2, transform=lambda v: v):
        return series_stats([transform(word(f.data, start, length))
                             for f in physical_groups[identifier]
                             if len(f.data) >= start + length])

    # Explicit numeric hypotheses. Names do not assert vehicle signal semantics.
    hypotheses = {
        "0x180_bytes_0_1_BE_raw": signal(0x180, 0),
        "0x180_bytes_0_1_BE_div8": signal(0x180, 0, transform=lambda v: v / 8),
        "0x1F9_bytes_2_3_BE_raw": signal(0x1F9, 2),
        "0x180_bytes_2_3_BE_shift4": signal(0x180, 2, transform=lambda v: v >> 4),
        "0x160_bytes_1_2_BE_maskFFF": signal(0x160, 1, transform=lambda v: v & 0xFFF),
        "0x280_bytes_4_5_BE_div100": signal(0x280, 4, transform=lambda v: v / 100),
        "0x284_bytes_4_5_BE_div100": signal(0x284, 4, transform=lambda v: v / 100),
        "0x2DE_bytes_6_7_BE_div10": signal(0x2DE, 6, transform=lambda v: v / 10),
        "0x2DE_byte4_bit1": signal(0x2DE, 4, 1, lambda v: (v >> 1) & 1),
        "0x5C5_byte0_flags_candidate": signal(0x5C5, 0, 1),
        "0x5C5_bytes_1_3_odometer_candidate": signal(0x5C5, 1, 3),
        "0x580_byte1_raw_unit_unknown": signal(0x580, 1, 1),
        "0x580_byte3_shift1_mod4": signal(0x580, 3, 1, lambda v: (v >> 1) & 3),
        "0x551_byte0_minus40": signal(0x551, 0, 1, lambda v: v - 40),
    }
    # Match preceding frames in capture order, without inventing timestamps.
    matches, compared, last160 = 0, 0, None
    for frame in frames:
        if frame.extended or frame.rtr:
            continue
        if frame.can_id == 0x160 and len(frame.data) >= 3:
            last160 = word(frame.data, 1) & 0xFFF
        elif frame.can_id == 0x180 and len(frame.data) >= 4 and last160 is not None:
            compared += 1
            matches += (word(frame.data, 2) >> 4) == last160
    return {
        "accepted": len(frames), "rejected": rejected, "ids": per_id,
        "timestamped_frames": sum(f.timestamp_ms is not None for f in frames),
        "time_note": "No sample frequency or elapsed time is inferred from record counts.",
        "hypotheses": hypotheses,
        "preceding_160_vs_180_12bit_copy": {"compared": compared, "equal": matches},
        "calibration_note": "All hypotheses require synchronized physical measurements.",
    }


def write_outputs(path: Path, destination: Path) -> dict:
    outputs = [destination / name for name in ["analysis.json", "frames.csv"]]

    def aliases(a: Path, b: Path) -> bool:
        return a.resolve() == b.resolve() or (a.exists() and b.exists() and a.samefile(b))

    if any(aliases(path, output) for output in outputs):
        raise ValueError("output paths must differ from the input capture")
    if aliases(*outputs):
        raise ValueError("JSON and CSV outputs must be different files")
    frames, rejected = read_capture(path)
    if not frames:
        raise ValueError("capture contains no valid standard CAN frames")
    report = inspect_capture(frames, rejected)
    report["diagnostic_metadata"] = [
        {"line": number, "kind": line.split()[1], "text": line}
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1)
        if line.startswith("ODB ") and not line.startswith("ODB RAW_RX ")
        and len(line.split()) >= 2
    ]
    report["source"] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    with (destination / "frames.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["timestamp_ms", "direction", "can_id", "dlc", "data", "source_line",
                         "ext", "rtr", "origin"])
        for frame in frames:
            writer.writerow([frame.timestamp_ms, "RX", f"0x{frame.can_id:03X}",
                             frame.dlc, frame.data.hex(" ").upper(), frame.line,
                             int(frame.extended), int(frame.rtr), frame.origin])
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path(".local-validation/deep-analysis"))
    args = parser.parse_args()
    try:
        report = write_outputs(args.capture, args.output_dir)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Accepted {report['accepted']} frames; rejected {len(report['rejected'])}; "
          f"observed {len(report['ids'])} ID/ext/rtr groups.")
    print(f"Numeric analysis: {args.output_dir / 'analysis.json'}")
    print(f"Canonical frames: {args.output_dir / 'frames.csv'}")
    print(report["calibration_note"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
