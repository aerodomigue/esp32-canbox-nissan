"""
Integration tests for the OTA firmware update protocol (ESP32 CANBox v2).

Tests all OTA state-machine transitions via USB serial without actually
flashing firmware (a dummy payload is used and OTA ABORT is sent instead
of OTA END, so the device flash is never modified).

Prerequisites:
    pip install pyserial

Usage:
    python tools/test_ota_protocol.py /dev/ttyACM0
    python tools/test_ota_protocol.py COM3            # Windows

The test requires the ESP32 to be connected and running the v2 firmware.
"""

import argparse
import base64
import binascii
import hashlib
import sys
import time

import serial


# ---------------------------------------------------------------------------
# Configuration — must match firmware constants
# ---------------------------------------------------------------------------

CHUNK_SIZE = 180          # binary bytes per OTA DATA chunk
OTA_TIMEOUT_S = 60        # matches OTA_DATA_TIMEOUT_MS in SerialCommand.cpp
SERIAL_BAUD = 115200
SERIAL_READLINE_TIMEOUT = 0.1  # seconds per readline poll
CMD_TIMEOUT_S = 5.0       # default timeout for a command/response exchange


# ---------------------------------------------------------------------------
# Serial helpers
# ---------------------------------------------------------------------------

def _open_serial(port: str) -> serial.Serial:
    ser = serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_READLINE_TIMEOUT)
    time.sleep(2.0)  # wait for ESP32 to finish boot
    ser.reset_input_buffer()
    return ser


def send_cmd(ser: serial.Serial, cmd: str, timeout_s: float = CMD_TIMEOUT_S) -> list[str]:
    """Send a command and collect all lines until OK/ERROR or timeout.

    Returns every non-empty line received (including info lines before the
    terminal OK/ERROR line).
    """
    ser.write(f"{cmd}\n".encode())
    lines: list[str] = []
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        raw = ser.readline()
        if raw:
            line = raw.decode(errors="replace").strip()
            if line:
                lines.append(line)
                if line.startswith("OK") or line.startswith("ERROR"):
                    break
    return lines


def terminal_line(lines: list[str]) -> str | None:
    """Return the first line starting with OK or ERROR, or None."""
    return next((l for l in lines if l.startswith("OK") or l.startswith("ERROR")), None)


def drain(ser: serial.Serial, duration_s: float = 1.0) -> None:
    """Discard all bytes arriving over the next duration_s seconds."""
    deadline = time.time() + duration_s
    while time.time() < deadline:
        ser.readline()


# ---------------------------------------------------------------------------
# Protocol helpers
# ---------------------------------------------------------------------------

def crc32_hex(data: bytes) -> str:
    """CRC32 compatible with esp_rom_crc32_le / Java CRC32 / Python binascii."""
    return f"{binascii.crc32(data) & 0xFFFFFFFF:08x}"


def md5_hex(data: bytes) -> str:
    return hashlib.md5(data).hexdigest()


def preflight_abort(ser: serial.Serial) -> None:
    """Clear any stuck OTA session on the device."""
    send_cmd(ser, "OTA ABORT", timeout_s=1.5)
    drain(ser, 0.5)


def start_ota(ser: serial.Serial, payload: bytes) -> list[str]:
    md5 = md5_hex(payload)
    return send_cmd(ser, f"OTA START {len(payload)} {md5}")


def send_chunk(ser: serial.Serial, chunk: bytes, with_crc: bool = True) -> list[str]:
    b64 = base64.b64encode(chunk).decode()
    if with_crc:
        crc = crc32_hex(chunk)
        return send_cmd(ser, f"OTA DATA {b64} {crc}")
    return send_cmd(ser, f"OTA DATA {b64}")


def corrupt_crc(chunk: bytes) -> str:
    """Return a wrong CRC32 for the given chunk."""
    correct = binascii.crc32(chunk) & 0xFFFFFFFF
    return f"{correct ^ 0xFFFFFFFF:08x}"


# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"

_results: list[tuple[str, bool, str]] = []


def run_test(name: str, fn, ser: serial.Serial) -> bool:
    try:
        fn(ser)
        _results.append((name, True, ""))
        print(f"  {PASS}  {name}")
        return True
    except AssertionError as exc:
        _results.append((name, False, str(exc)))
        print(f"  {FAIL}  {name}")
        print(f"         {exc}")
        # Always attempt cleanup so subsequent tests start clean
        preflight_abort(ser)
        return False


def assert_ok(lines: list[str], test_name: str) -> str:
    tl = terminal_line(lines)
    assert tl is not None, f"No terminal line received (got: {lines})"
    assert tl.startswith("OK"), f"Expected OK, got: {tl!r}"
    return tl


def assert_error(lines: list[str], substring: str = "", test_name: str = "") -> str:
    tl = terminal_line(lines)
    assert tl is not None, f"No terminal line received (got: {lines})"
    assert tl.startswith("ERROR"), f"Expected ERROR, got: {tl!r}"
    if substring:
        assert substring.lower() in tl.lower(), \
            f"Expected '{substring}' in error, got: {tl!r}"
    return tl


# ---------------------------------------------------------------------------
# Individual test cases
# ---------------------------------------------------------------------------

DUMMY_PAYLOAD = bytes(range(256)) * 4  # 1024 bytes, easily fits in OTA partition


def test_preflight_abort_when_idle(ser: serial.Serial) -> None:
    """OTA ABORT when no update is in progress must respond with 'OTA aborted'."""
    lines = send_cmd(ser, "OTA ABORT")
    # The response starts with "OTA" not "OK"/"ERROR"; just check it arrives
    assert any("aborted" in l.lower() for l in lines), \
        f"Expected 'aborted' in response, got: {lines}"


def test_start_valid(ser: serial.Serial) -> None:
    """OTA START with valid size and MD5 → OK READY must be the LAST OK line."""
    lines = start_ota(ser, DUMMY_PAYLOAD)
    tl = assert_ok(lines, "test_start_valid")
    assert "READY" in tl, f"Expected 'OK READY', got: {tl!r}"
    # v2 protocol: info lines come first, OK READY is last
    ok_indices = [i for i, l in enumerate(lines) if l.startswith("OK")]
    assert ok_indices[-1] == len(lines) - 1, \
        "OK READY must be the last line (info lines must precede it)"
    preflight_abort(ser)


def test_start_response_format_v2(ser: serial.Serial) -> None:
    """OTA START response must contain info lines BEFORE OK READY."""
    lines = start_ota(ser, DUMMY_PAYLOAD)
    ok_idx = next(i for i, l in enumerate(lines) if l.startswith("OK"))
    # There must be at least one info line before OK READY
    assert ok_idx > 0, f"No info lines before OK READY: {lines}"
    # No OK line should appear before the final one
    assert not any(l.startswith("OK") for l in lines[:ok_idx]), \
        f"Unexpected OK line before OK READY: {lines}"
    preflight_abort(ser)


def test_start_already_in_progress(ser: serial.Serial) -> None:
    """A second OTA START while one is active must return ERROR."""
    start_ota(ser, DUMMY_PAYLOAD)
    lines = start_ota(ser, DUMMY_PAYLOAD)
    assert_error(lines, "already", "test_start_already_in_progress")
    preflight_abort(ser)


def test_start_size_zero(ser: serial.Serial) -> None:
    """OTA START with size=0 must return ERROR."""
    preflight_abort(ser)
    lines = send_cmd(ser, "OTA START 0")
    assert_error(lines, "invalid", "test_start_size_zero")


def test_data_without_ota_start(ser: serial.Serial) -> None:
    """OTA DATA without a preceding OTA START must return ERROR."""
    preflight_abort(ser)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    lines = send_chunk(ser, chunk)
    assert_error(lines, "No OTA in progress", "test_data_without_ota_start")


def test_data_valid_crc32(ser: serial.Serial) -> None:
    """OTA DATA with correct CRC32 must be accepted (OK response)."""
    start_ota(ser, DUMMY_PAYLOAD)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    lines = send_chunk(ser, chunk, with_crc=True)
    assert_ok(lines, "test_data_valid_crc32")
    preflight_abort(ser)


def test_echo_disabled_during_ota(ser: serial.Serial) -> None:
    """While OTA is in progress, no echo characters should appear in the response."""
    start_ota(ser, DUMMY_PAYLOAD)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    b64 = base64.b64encode(chunk).decode()
    crc = crc32_hex(chunk)
    lines = send_cmd(ser, f"OTA DATA {b64} {crc}")
    # If echo were active, we'd see "OTA DATA AAAA..." among the received lines
    assert not any(l.startswith("OTA DATA") for l in lines), \
        f"Echo is still active during OTA: {lines}"
    preflight_abort(ser)


def test_data_invalid_crc32_does_not_advance_state(ser: serial.Serial) -> None:
    """A chunk with wrong CRC32 must return ERROR and leave otaReceivedSize unchanged.

    Proof: send bad CRC → ERROR, then same chunk with good CRC → OK 180/total
    (not 360/total), confirming the state pointer did not advance.
    """
    start_ota(ser, DUMMY_PAYLOAD)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    b64 = base64.b64encode(chunk).decode()
    bad_crc = corrupt_crc(chunk)
    good_crc = crc32_hex(chunk)

    # Bad CRC → ERROR
    lines = send_cmd(ser, f"OTA DATA {b64} {bad_crc}")
    assert_error(lines, "CRC", "bad_crc_returns_error")

    # Same chunk with good CRC → OK with 180 bytes received (not 360)
    lines = send_cmd(ser, f"OTA DATA {b64} {good_crc}")
    tl = assert_ok(lines, "retry_with_good_crc")
    assert f"OK {CHUNK_SIZE}/" in tl, \
        f"otaReceivedSize advanced despite CRC error; got: {tl!r}"

    preflight_abort(ser)


def test_data_invalid_crc32_three_failures(ser: serial.Serial) -> None:
    """Three consecutive CRC failures must all return ERROR; OTA stays active."""
    start_ota(ser, DUMMY_PAYLOAD)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    b64 = base64.b64encode(chunk).decode()
    bad_crc = corrupt_crc(chunk)

    for attempt in range(3):
        lines = send_cmd(ser, f"OTA DATA {b64} {bad_crc}")
        assert_error(lines, "CRC", f"failure_{attempt}")

    # OTA must still be in progress (not auto-aborted)
    lines = send_cmd(ser, "OTA STATUS")
    assert any("YES" in l for l in lines), \
        f"OTA should still be in progress after 3 CRC failures: {lines}"

    preflight_abort(ser)


def test_abort_mid_transfer(ser: serial.Serial) -> None:
    """OTA ABORT mid-transfer must reset state (STATUS shows NO afterwards)."""
    start_ota(ser, DUMMY_PAYLOAD)
    for i in range(5):
        chunk = DUMMY_PAYLOAD[i * CHUNK_SIZE:(i + 1) * CHUNK_SIZE]
        send_chunk(ser, chunk)

    send_cmd(ser, "OTA ABORT")

    lines = send_cmd(ser, "OTA STATUS")
    assert any("NO" in l for l in lines), \
        f"OTA should be inactive after ABORT: {lines}"


def test_status_idle(ser: serial.Serial) -> None:
    """OTA STATUS when idle must report 'NO' for update in progress."""
    preflight_abort(ser)
    lines = send_cmd(ser, "OTA STATUS")
    tl = terminal_line(lines)
    assert tl is not None, f"No terminal line: {lines}"
    assert any("NO" in l for l in lines), f"Expected 'NO' in STATUS: {lines}"


def test_status_in_progress(ser: serial.Serial) -> None:
    """OTA STATUS during a transfer must report 'YES' and progress."""
    start_ota(ser, DUMMY_PAYLOAD)
    chunk = DUMMY_PAYLOAD[:CHUNK_SIZE]
    send_chunk(ser, chunk)

    lines = send_cmd(ser, "OTA STATUS")
    assert any("YES" in l for l in lines), f"Expected 'YES' in STATUS: {lines}"
    assert any(str(CHUNK_SIZE) in l for l in lines), \
        f"Expected {CHUNK_SIZE} bytes in STATUS: {lines}"

    preflight_abort(ser)


def test_timeout_auto_abort(ser: serial.Serial) -> None:
    """After OTA_TIMEOUT_S with no DATA, the device must auto-abort.

    This test takes ~65 seconds to run; skip with --skip-slow if needed.
    """
    start_ota(ser, DUMMY_PAYLOAD)
    print(f"\n    (waiting {OTA_TIMEOUT_S + 5}s for timeout auto-abort...)", end="", flush=True)
    time.sleep(OTA_TIMEOUT_S + 5)

    lines = send_cmd(ser, "OTA STATUS")
    assert any("NO" in l for l in lines), \
        f"Expected auto-abort after {OTA_TIMEOUT_S}s, STATUS still shows: {lines}"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

ALL_TESTS = [
    ("preflight_abort_when_idle",                test_preflight_abort_when_idle),
    ("start_valid",                              test_start_valid),
    ("start_response_format_v2",                 test_start_response_format_v2),
    ("start_already_in_progress",                test_start_already_in_progress),
    ("start_size_zero",                          test_start_size_zero),
    ("data_without_ota_start",                   test_data_without_ota_start),
    ("data_valid_crc32",                         test_data_valid_crc32),
    ("echo_disabled_during_ota",                 test_echo_disabled_during_ota),
    ("data_invalid_crc32_does_not_advance_state", test_data_invalid_crc32_does_not_advance_state),
    ("data_invalid_crc32_three_failures",        test_data_invalid_crc32_three_failures),
    ("abort_mid_transfer",                       test_abort_mid_transfer),
    ("status_idle",                              test_status_idle),
    ("status_in_progress",                       test_status_in_progress),
]

SLOW_TESTS = [
    ("timeout_auto_abort",                       test_timeout_auto_abort),
]


def main() -> None:
    parser = argparse.ArgumentParser(
        description="OTA protocol integration tests for ESP32 CANBox v2 firmware."
    )
    parser.add_argument("port", help="Serial port, e.g. /dev/ttyACM0 or COM3")
    parser.add_argument(
        "--skip-slow", action="store_true",
        help=f"Skip the {OTA_TIMEOUT_S}s auto-abort timeout test"
    )
    args = parser.parse_args()

    print(f"Connecting to {args.port}...")
    try:
        ser = _open_serial(args.port)
    except serial.SerialException as exc:
        print(f"ERROR: Cannot open {args.port}: {exc}")
        sys.exit(1)

    print(f"Running {len(ALL_TESTS)} integration tests (OTA protocol v2)\n")

    tests = ALL_TESTS if args.skip_slow else ALL_TESTS + SLOW_TESTS

    for name, fn in tests:
        run_test(name, fn, ser)

    ser.close()

    passed = sum(1 for _, ok, _ in _results if ok)
    failed = sum(1 for _, ok, _ in _results if not ok)

    print(f"\n{'─'*50}")
    print(f"  Results: {passed} passed, {failed} failed")
    if failed:
        print("\nFailed tests:")
        for name, ok, msg in _results:
            if not ok:
                print(f"  - {name}: {msg}")
    print()

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
