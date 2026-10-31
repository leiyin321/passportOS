#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
: "${IDF_PATH:?Source ESP-IDF export.sh before building}"
mode="${1:-normal}"
config_name=pool
defaults='sdkconfig.defaults;sdkconfig.protected.defaults;sdkconfig.pool.defaults'
case "$mode" in
  normal) ;;
  heap|text) config_name="pool-$mode"; defaults="$defaults;sdkconfig.pool-$mode.defaults" ;;
  *) echo 'usage: build_pool.sh [normal|heap|text]' >&2; exit 2 ;;
esac
"$ROOT/build_module.sh"
IDF_TARGET=esp32c3 idf.py -C "$ROOT" -B "$ROOT/build-$config_name" \
  -D "SDKCONFIG=sdkconfig.$config_name" -D "SDKCONFIG_DEFAULTS=$defaults" build
