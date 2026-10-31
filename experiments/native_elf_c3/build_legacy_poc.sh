#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
: "${IDF_PATH:?Source ESP-IDF export.sh before building}"
"$ROOT/build_module.sh"
IDF_TARGET=esp32c3 idf.py -C "$ROOT" -B "$ROOT/build" \
  -D SDKCONFIG=sdkconfig.poc build
