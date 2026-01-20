#!/usr/bin/env python3
"""
CAN Frame Injection Tool for ESP32 Nissan CAN Bridge

Reads CAN frames from a capture file and injects them via serial port
for testing without a real CAN bus connection.

Usage:
    python can_inject.py <serial_port> <capture_file> [options]

Examples:
    python can_inject.py /dev/ttyUSB0 "nissan trame export.txt"
    python can_inject.py /dev/ttyUSB0 "nissan trame export.txt" --delay 10
    python can_inject.py /dev/ttyUSB0 "nissan trame export.txt" --filter 180,5C5,60D
    python can_inject.py --list-ports
"""

import argparse
import re
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)


def list_serial_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


def parse_can_line(line):
    """
    Parse a CAN frame line from the capture file.

    Format: RX ID: 0x182 | DLC: 8 | Data: 00 00 00 00 00 35 00 CE

    Returns: (can_id, data_bytes) or None if parsing fails
    """
    # Regex to match the format
    match = re.match(
        r'RX ID:\s*0x([0-9A-Fa-f]+)\s*\|\s*DLC:\s*(\d+)\s*\|\s*Data:\s*(.*)',
        line.strip()
    )

    if not match:
        return None

    can_id = match.group(1)
    dlc = int(match.group(2))
    data_str = match.group(3).strip()

    # Remove trailing spaces and split
    data_bytes = data_str.split()

    # Validate DLC matches data length
    if len(data_bytes) != dlc:
        # Truncate or use what we have
        data_bytes = data_bytes[:dlc]

    return (can_id, data_bytes)


def format_injection_command(can_id, data_bytes):
    """
    Format a CAN frame as an injection command.

    Returns: ">ID DATA..." string
    """
    data_str = ' '.join(data_bytes)
    return f">{can_id} {data_str}"


def inject_frames(serial_port, capture_file, delay_ms=10, filter_ids=None, loop=False, verbose=True):
    """
    Read frames from capture file and inject via serial.

    Args:
        serial_port: Serial port path (e.g., /dev/ttyUSB0)
        capture_file: Path to the capture file
        delay_ms: Delay between frames in milliseconds
        filter_ids: List of CAN IDs to inject (hex strings), None = all
        loop: Loop continuously through the file
        verbose: Print each injected frame
    """
    # Open serial port
    try:
        ser = serial.Serial(serial_port, 115200, timeout=1)
        print(f"Connected to {serial_port}")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    # Give ESP32 time to initialize
    time.sleep(0.5)

    # Convert filter IDs to uppercase set
    if filter_ids:
        filter_set = {id.upper() for id in filter_ids}
        print(f"Filtering: {', '.join(filter_set)}")
    else:
        filter_set = None

    try:
        while True:
            # Read and parse capture file
            with open(capture_file, 'r') as f:
                lines = f.readlines()

            frame_count = 0
            injected_count = 0

            for line in lines:
                result = parse_can_line(line)
                if result is None:
                    continue

                can_id, data_bytes = result
                frame_count += 1

                # Apply filter if specified
                if filter_set and can_id.upper() not in filter_set:
                    continue

                # Format and send command
                cmd = format_injection_command(can_id, data_bytes)
                ser.write((cmd + '\n').encode())
                injected_count += 1

                if verbose:
                    print(f"[{injected_count:04d}] {cmd}")

                # Read response (non-blocking)
                if ser.in_waiting:
                    response = ser.readline().decode('utf-8', errors='ignore').strip()
                    if response and verbose:
                        print(f"       <- {response}")

                # Delay between frames
                if delay_ms > 0:
                    time.sleep(delay_ms / 1000.0)

            print(f"\nProcessed {frame_count} frames, injected {injected_count}")

            if not loop:
                break

            print("Looping... (Ctrl+C to stop)")

    except KeyboardInterrupt:
        print("\nStopped by user")
    except FileNotFoundError:
        print(f"Error: File not found: {capture_file}")
        sys.exit(1)
    finally:
        ser.close()
        print("Serial port closed")


def main():
    parser = argparse.ArgumentParser(
        description='Inject CAN frames via serial for ESP32 testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s /dev/ttyUSB0 "nissan trame export.txt"
  %(prog)s /dev/ttyUSB0 capture.txt --delay 50
  %(prog)s /dev/ttyUSB0 capture.txt --filter 180,5C5,60D
  %(prog)s /dev/ttyUSB0 capture.txt --loop
  %(prog)s --list-ports
        """
    )

    parser.add_argument('port', nargs='?', help='Serial port (e.g., /dev/ttyUSB0, COM3)')
    parser.add_argument('file', nargs='?', help='CAN capture file')
    parser.add_argument('--delay', '-d', type=int, default=10,
                        help='Delay between frames in ms (default: 10)')
    parser.add_argument('--filter', '-f', type=str,
                        help='Comma-separated CAN IDs to inject (hex, e.g., 180,5C5,60D)')
    parser.add_argument('--loop', '-l', action='store_true',
                        help='Loop continuously through the file')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Suppress frame-by-frame output')
    parser.add_argument('--list-ports', action='store_true',
                        help='List available serial ports')

    args = parser.parse_args()

    if args.list_ports:
        list_serial_ports()
        return

    if not args.port or not args.file:
        parser.print_help()
        sys.exit(1)

    filter_ids = None
    if args.filter:
        filter_ids = [id.strip() for id in args.filter.split(',')]

    inject_frames(
        serial_port=args.port,
        capture_file=args.file,
        delay_ms=args.delay,
        filter_ids=filter_ids,
        loop=args.loop,
        verbose=not args.quiet
    )


if __name__ == '__main__':
    main()
