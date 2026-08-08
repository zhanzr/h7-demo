#!/usr/bin/env python3
"""Capture a serial port and print what arrives, cross-platform (pyserial).

Usage:
  python serial_capture.py <port> [baud] [seconds]
    port      e.g. COM3 (Windows) or /dev/ttyUSB0 (Linux/macOS)
    baud      default 115200
    seconds   capture duration in seconds (default 0 = until Ctrl+C)

Example:
  python serial_capture.py COM3 115200 10
"""
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required:  pip install pyserial")


def main() -> int:
    if len(sys.argv) < 2:
        sys.exit(__doc__)

    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0

    start = time.monotonic()
    with serial.Serial(port, baud, timeout=0.2) as ser:
        print(f"--- opened {port} @ {baud} baud ---", flush=True)
        while True:
            if seconds > 0 and time.monotonic() - start >= seconds:
                break
            chunk = ser.read(4096)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
    print(f"\n--- closed {port} after {time.monotonic() - start:.1f}s ---", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
