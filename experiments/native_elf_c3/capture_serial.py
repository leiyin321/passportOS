#!/usr/bin/env python3
"""Bounded USB serial capture; raw logs must stay local until sanitized."""
import argparse
import time
from pathlib import Path
import serial
from esptool.reset import HardReset

parser = argparse.ArgumentParser()
parser.add_argument('port')
parser.add_argument('output', type=Path)
parser.add_argument('--seconds', type=float, default=12)
parser.add_argument('--command', action='append', default=[])
args = parser.parse_args()
if not 1 <= args.seconds <= 60:
    parser.error('seconds must be in [1, 60]')
args.output.parent.mkdir(parents=True, exist_ok=True)
with serial.Serial(args.port, 115200, timeout=0.1) as port, args.output.open('wb') as out:
    port.dtr = False
    HardReset(port, uses_usb=True).reset()
    start = time.monotonic()
    next_command = 0
    while time.monotonic() - start < args.seconds:
        if next_command < len(args.command) and time.monotonic() - start >= 8 + next_command * 2:
            command = args.command[next_command]
            out.write(f'\nHOST COMMAND {command}\n'.encode())
            port.write((command + '\n').encode())
            next_command += 1
        data = port.read(port.in_waiting or 1)
        if data:
            out.write(data)
            out.flush()
print(f'Captured {args.output.stat().st_size} bytes to {args.output}')
