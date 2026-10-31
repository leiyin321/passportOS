#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build/tests
cc -std=c11 -Wall -Wextra -Werror -Ios/core tests/passport/test_session.c os/core/session.c -o build/tests/session
build/tests/session
cc -std=c11 -Wall -Wextra -Werror -Ios/services/network tests/passport/test_policy.c os/services/network/policy.c -o build/tests/policy
build/tests/policy
cc -std=c11 -Wall -Wextra -Werror -Ios/services/network tests/passport/test_redirect.c os/services/network/redirect.c os/services/network/policy.c -o build/tests/redirect
build/tests/redirect
cc -std=c11 -Wall -Wextra -Werror -Ios/services/network tests/passport/test_headers.c os/services/network/headers.c -o build/tests/headers
build/tests/headers
cc -std=c11 -Wall -Wextra -Werror -Ios/services/network tests/passport/test_http_response.c -o build/tests/http-response
build/tests/http-response
cc -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations -fsanitize=address,undefined -Itests/os_network_stubs -Ios/services/network -Ios/package -Isimulator/vendor/cJSON tests/passport/test_network_worker.c os/services/network/headers.c os/services/network/policy.c os/services/network/redirect.c simulator/vendor/cJSON/cJSON.c -o build/tests/network-worker
ASAN_OPTIONS=detect_leaks=0 build/tests/network-worker
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/ws_transport_stubs tests/passport/test_ws_transport.c -o build/tests/ws-transport
build/tests/ws-transport
python3 tests/passport/test_network_worker_stack.py
python3 tests/passport/test_library_render_stack.py
cc -std=c11 -Wall -Wextra -Werror -Ios/services/audio tests/passport/test_audio.c os/services/audio/features.c -lm -o build/tests/audio
cc -std=c11 -Wall -Wextra -Werror -Itests/os_audio_stubs -Ios/services/audio tests/passport/test_audio_service.c os/services/audio/volume.c os/services/audio/features.c os/services/audio/opus_packet.c -lm -o build/tests/audio-service
python3 tests/passport/test_opus_packet.py
build/tests/audio-service
build/tests/audio
cc -std=c11 -Wall -Wextra -Werror -Ios/services/display -Isimulator/vendor/cJSON tests/assets/test_display_layout.c os/services/display/display_layout.c simulator/vendor/cJSON/cJSON.c -o build/tests/display-layout
build/tests/display-layout
cc -std=c11 -Wall -Wextra -Werror -Ios/services/language \
  tests/test_language.c os/services/language/language.c \
  -o build/tests/language
build/tests/language
python3 tests/passport/test_device_cli.py
python3 tests/passport/test_native.py
cc -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations -fsanitize=address,undefined -Ios/runtime -Ios/package -Isimulator/vendor/cJSON tests/passport/test_native_json.c os/runtime/native_json.c os/package/json.c simulator/vendor/cJSON/cJSON.c -o build/tests/native-json
build/tests/native-json
cc -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations -Wno-misleading-indentation -fsanitize=address,undefined -Isdk/native/include -Ios/runtime -Ios/package -Isimulator/vendor/cJSON tests/passport/test_xiaozhi_native.c examples/xiaozhi_native/main.c os/runtime/native_json.c os/package/json.c simulator/vendor/cJSON/cJSON.c -o build/tests/xiaozhi-native
build/tests/xiaozhi-native
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isdk/native/include tests/passport/test_city_radio.c examples/city_radio/catalog.c -o build/tests/city-radio
build/tests/city-radio
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icomponents/passport_mp3/include -Ios/services/audio tests/passport/test_mp3.c components/passport_mp3/decoder.c os/services/audio/mp3_frames.c -o build/tests/mp3
ASAN_OPTIONS=detect_leaks=0 build/tests/mp3
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -pthread -Itests/os_audio_stubs -Ios/services/audio -Ios/services/network -Ios/package -Isimulator/vendor/cJSON -Icomponents/passport_mp3/include tests/passport/test_media_service.c components/passport_mp3/decoder.c os/services/audio/mp3_frames.c os/services/audio/volume.c -o build/tests/media-service
ASAN_OPTIONS=detect_leaks=0 build/tests/media-service
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP_PLATFORM -Itests/mp3_vendor_stubs -Icomponents/passport_mp3/include tests/passport/test_mp3_vendor.c components/passport_mp3/decoder.c -o build/tests/mp3-vendor
build/tests/mp3-vendor
python3 tests/passport/test_packages.py
python3 tests/passport/test_declarative.py
python3 tests/passport/test_firmware_update.py
python3 tests/passport/test_distribution.py
./simulator/build.sh
python3 tests/passport/test_runtime.py
cc -std=c11 -Wall -Wextra -Werror -Ios/services/display tests/assets/test_registry.c os/services/display/icon_registry.c -o build/tests/icons
build/tests/icons
cc -std=c11 -Wall -Wextra -Werror -Ios/services/display tests/assets/test_format.c os/services/display/asset_format.c -o build/tests/asset-format
build/tests/asset-format
host_crypto=""
if [ "$(uname -s)" != Darwin ]; then host_crypto="-lcrypto"; fi
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wno-deprecated-declarations -fsanitize=address,undefined -Ios/services/display -Ios/package -Isimulator/vendor/cJSON tests/assets/test_loader.c os/services/display/asset_loader.c os/services/display/asset_format.c os/package/json.c simulator/vendor/cJSON/cJSON.c $host_crypto -o build/tests/asset-loader
ASAN_OPTIONS=detect_leaks=0 build/tests/asset-loader
asset_python="${PASSPORT_ASSET_PYTHON:-python3}"
if ! "$asset_python" -c 'import PIL,cairosvg' >/dev/null 2>&1 && [ -x venv/bin/python ]; then asset_python=venv/bin/python; fi
"$asset_python" tests/assets/test_compiler.py

cc -std=c11 -Wall -Wextra -Werror -Ios/services/bluetooth tests/passport/test_hid_protocol.c os/services/bluetooth/hid_protocol.c -o build/tests/hid-protocol
build/tests/hid-protocol
cc -std=c11 -Wall -Wextra -Werror -Isdk/native/include tests/passport/test_bluetooth_native.c examples/bluetooth_peripheral/gesture.c -o build/tests/bluetooth-native
build/tests/bluetooth-native
