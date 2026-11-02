"""Capture real P4 USB/JTAG diagnostics without flashing or sending commands.

Reset sequence follows the user's P4SDM tests/capture_guition.py.
"""
import argparse
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()
    if not 0 < args.seconds <= 3600:
        parser.error("--seconds must be in (0, 3600]")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.2
    connection.dtr = False
    connection.rts = False
    connection.open()
    with connection as port, args.output.open("wb") as output:
        if args.reset:
            port.rts = True
            time.sleep(0.2)
            port.rts = False
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            chunk = port.read(max(1, port.in_waiting))
            if chunk:
                output.write(chunk)
                output.flush()
    size = args.output.stat().st_size
    print(f"Captured {size} real serial bytes to {args.output}")
    if not size:
        raise SystemExit("FAIL: no serial data; boot and peripherals are not verified")


if __name__ == "__main__":
    main()
