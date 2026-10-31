#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
: "${IDF_PATH:?Source ESP-IDF export.sh before building}"
riscv32-esp-elf-gcc \
  -march=rv32imc_zicsr_zifencei -mabi=ilp32 -Os \
  -fPIC -fvisibility=hidden -nostartfiles -nostdlib -shared \
  -Wl,-e,app_main -Wl,--no-relax \
  -o "$ROOT/module/native.app.elf" "$ROOT/module/native.c" "$ROOT/module/helper.c"
riscv32-esp-elf-readelf -h -S -r -s "$ROOT/module/native.app.elf" \
  > "$ROOT/logs/module-readelf.txt"
riscv32-esp-elf-readelf -l "$ROOT/module/native.app.elf" \
  > "$ROOT/logs/module-program-headers.txt"
python3 "$ROOT/lint_module.py" "$ROOT/module/native.app.elf"
riscv32-esp-elf-objdump -dr "$ROOT/module/native.app.elf" > "$ROOT/logs/module-objdump.txt"
