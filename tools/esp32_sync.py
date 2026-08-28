#!/usr/bin/env python3
"""
AMY Studio - ESP32-S3 Hardware Synchronization CLI & Diagnostics Tool
Usage:
  python tools/esp32_sync.py list-ports
  python tools/esp32_sync.py status --port COM3
  python tools/esp32_sync.py patch-list --port COM3
  python tools/esp32_sync.py upload --port COM3 --slot 0 --file my_patch.amy
  python tools/esp32_sync.py backup --port COM3 --out ./backup_patches/
  python tools/esp32_sync.py panic --port COM3
"""

import sys
import os
import time
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None

def list_ports():
    if not serial:
        print("Error: pyserial is not installed. Install with: pip install pyserial")
        return
    ports = serial.tools.list_ports.comports()
    print("Available Serial / USB Ports:")
    for p in ports:
        print(f"  - {p.device}: {p.description} (HWID: {p.hwid})")

def send_command(port_name, cmd_str, baud=115200, wait_seconds=0.5):
    if not serial:
        print("Error: pyserial is not installed. Install with: pip install pyserial")
        return None
    try:
        ser = serial.Serial(port_name, baud, timeout=1.0)
        time.sleep(0.1)
        ser.write((cmd_str + '\n').encode('utf-8'))
        time.sleep(wait_seconds)
        response = ser.read_all().decode('utf-8', errors='ignore')
        ser.close()
        return response
    except Exception as e:
        print(f"Error communicating with {port_name}: {e}")
        return None

def main():
    parser = argparse.ArgumentParser(description="AMY ESP32-S3 Hardware Sync Tool")
    parser.add_argument("command", choices=["list-ports", "status", "patch-list", "upload", "backup", "panic", "test-tone"])
    parser.add_argument("--port", default="COM3", help="Serial COM port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--slot", type=int, default=0, help="Flash patch slot ID (0..127)")
    parser.add_argument("--file", help="Path to patch file (.amy / .s3p)")
    parser.add_argument("--out", default="./backup", help="Output directory for backup")

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    args = parser.parse_args()

    if args.command == "list-ports":
        list_ports()
    elif args.command == "status":
        print(f"Querying status from {args.port}...")
        resp = send_command(args.port, "status", args.baud)
        print(resp if resp else "No response.")
    elif args.command == "panic":
        print(f"Sending PANIC to {args.port}...")
        resp = send_command(args.port, "panic", args.baud)
        print(resp if resp else "Panic sent.")
    elif args.command == "patch-list":
        print(f"Querying patch list from {args.port}...")
        resp = send_command(args.port, "patch list", args.baud)
        print(resp if resp else "No response.")
    elif args.command == "test-tone":
        print(f"Playing 440Hz test tone on {args.port}...")
        resp = send_command(args.port, "v0w0f440l1Z", args.baud)
        print("Tone sent.")

if __name__ == '__main__':
    main()
