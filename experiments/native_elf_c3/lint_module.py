#!/usr/bin/env python3
"""Validate the generated module's ELF identity and loader relocation subset."""
import re
import subprocess
import sys
import struct
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit(f"usage: {sys.argv[0]} MODULE.ELF")
path = Path(sys.argv[1])
data = path.read_bytes()
if len(data) < 52:
    raise SystemExit("FAIL truncated ELF header")
header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
if header[0][:6] != b"\x7fELF\x01\x01" or header[1] != 3 or header[2] != 243:
    raise SystemExit("FAIL expected little-endian ELF32 RISC-V ET_DYN")
entry, phoff, phentsize, phnum = header[4], header[5], header[9], header[10]
if phentsize != 32 or phoff + phnum * phentsize > len(data):
    raise SystemExit("FAIL invalid program-header table")
loads = []
for i in range(phnum):
    ph = struct.unpack_from("<IIIIIIII", data, phoff + i * phentsize)
    if ph[0] == 1:
        if ph[4] > ph[5] or ph[1] + ph[4] > len(data):
            raise SystemExit("FAIL invalid PT_LOAD bounds")
        loads.append(ph)
if not loads or min(ph[2] for ph in loads) != 0:
    raise SystemExit("FAIL POC supports zero-based PT_LOAD layout only")
if not any(ph[6] & 1 and ph[2] <= entry < ph[2] + ph[5] for ph in loads):
    raise SystemExit("FAIL entry outside executable PT_LOAD")
out = subprocess.check_output(
    ["riscv32-esp-elf-readelf", "-h", "-r", str(path)], text=True
)
for required in ("ELF32", "DYN (Shared object file)", "RISC-V"):
    if required not in out:
        raise SystemExit(f"FAIL missing ELF property: {required}")
allowed = {"R_RISCV_NONE", "R_RISCV_32", "R_RISCV_RELATIVE", "R_RISCV_JUMP_SLOT"}
relocs = re.findall(r"\bR_RISCV_[A-Z0-9_]+\b", out)
unsupported = sorted(set(relocs) - allowed)
if unsupported:
    raise SystemExit(f"FAIL unsupported relocations: {unsupported}")
entry = re.search(r"Entry point address:\s+0x([0-9a-fA-F]+)", out)
if not entry or int(entry.group(1), 16) == 0:
    raise SystemExit("FAIL no nonzero entry point")
print(f"PASS ELF32 ET_DYN EM_RISCV entry=0x{entry.group(1)} relocations={relocs or 'none'}")
