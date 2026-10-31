#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/simulator
lua_sources=$(find components/passport_lua/src -name '*.c' ! -name lua.c ! -name luac.c)
host_crypto=""
if [ "$(uname -s)" != Darwin ]; then host_crypto="-lcrypto"; fi
cc -std=c11 -O2 -Wall -Wextra -DLUA_32BITS -Ios/runtime -Ios/package -Ios/services/storage -Ios/services/network -Ios/services/audio -Ios/services/display -Ios/services/language -Icomponents/passport_lua/src -I"simulator/vendor/cJSON" simulator/host.c os/runtime/runtime.c os/runtime/declarative.c os/package/json.c os/services/display/display_layout.c os/services/display/asset_loader.c os/services/display/asset_format.c os/services/display/icon_registry.c os/services/language/language.c "simulator/vendor/cJSON/cJSON.c" $lua_sources -lm $host_crypto -o build/simulator/runtime
