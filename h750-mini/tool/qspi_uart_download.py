#!/usr/bin/env python3
"""Send the stage-2 app raw binary to the qspi_map bootloader's UART download
mode and print the console output.

Protocol (see qspi_map/boot/src/main.c):
  1. bootloader prints "... UART download mode: send 4-byte LE length then raw binary ..."
  2. host sends 4-byte little-endian length, then the raw app binary
  3. bootloader programs it into the W25Q64 and jumps

Usage:
  python qspi_uart_download.py <app.bin> [port] [baud]
"""
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required:  pip install pyserial")

PORT = sys.argv[2] if len(sys.argv) > 2 else "COM56"
BAUD = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
HEX = None


def main() -> int:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    if len(data) == 0:
        sys.exit("empty app.bin")

    print(f"[qspi_uart_download] {sys.argv[1]}: {len(data)} bytes "
          f"-> {PORT} @ {BAUD}")

    with serial.Serial(PORT, BAUD, timeout=0.5) as ser:
        # The CH340 may reset the MCU on port open, so wait for the bootloader
        # to reach its UART download prompt (re-printed on each failed length).
        prompt = b"UART download mode"
        buf = b""
        deadline = time.monotonic() + 15
        while prompt not in buf and time.monotonic() < deadline:
            chunk = ser.read(512)
            if chunk:
                buf += chunk
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        if prompt not in buf:
            print("\n[qspi_uart_download] prompt not seen; sending anyway")
        else:
            print("\n[qspi_uart_download] prompt seen")

        # Send 4-byte LE length + raw binary. The bootloader buffers the whole
        # image in RAM (no flash writes during RX), so streaming it all at once
        # is safe.
        ser.write(len(data).to_bytes(4, "little"))
        ser.write(data)
        ser.flush()
        print("[qspi_uart_download] sent %d bytes" % (len(data) + 4))

        # Read the rest of the session (download progress + jump banner + app).
        end = time.monotonic() + 30
        while time.monotonic() < end:
            chunk = ser.read(512)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
