idf_component_register(
    SRCS "passport_main.c" "../assets/fonts/passport_cjk_extra_16.c" "../assets/fonts/passport_cjk_chat_16.c" "../os/core/session.c" "../os/services/network/wifi.c" "../os/services/network/wifi_setup_form.c" "../os/services/network/wifi_setup_qr.c" "../os/services/network/portal_flow.c" "../os/services/network/portal_dns.c" "../os/services/network/portal_dns_server.c" "../os/services/network/wifi_profiles.c" "../os/services/network/policy.c" "../os/services/network/redirect.c" "../os/services/network/network.c" "../os/services/network/ws_transport.c" "../os/services/network/headers.c" "../os/package/package.c" "../os/package/json.c" "../os/package/trust.c" "../os/services/display/display_layout.c" "../os/services/display/asset_format.c" "../os/services/display/asset_loader.c" "../os/services/display/asset_probe.c" "../os/services/display/icon_registry.c" "../os/services/storage/store.c" "../os/services/storage/storage_usage.c" "../os/services/storage/storage_layout.c" "../os/services/storage/app_version.c" "../os/runtime/runtime.c" "../os/runtime/native.c" "../os/runtime/native_json.c" "../os/runtime/native_elf.c" "../os/runtime/declarative.c" "../os/services/bluetooth/hid.c" "../os/services/bluetooth/hid_protocol.c" "../os/services/audio/audio.c" "../os/services/audio/media.c" "../os/services/audio/mp3_frames.c" "../os/services/audio/volume.c" "../os/services/audio/opus_packet.c" "../os/services/audio/notification.c" "../os/services/audio/features.c" "../os/services/ota/update.c" "../os/services/language/language.c" "../os/services/clock/alarm.c" "../os/services/settings/list.c"
    INCLUDE_DIRS "." "../os/services/bluetooth" "../sdk/native/include" "../os/core" "../os/services/network" "../os/services/ota" "../os/services/language" "../os/package" "../os/services/storage" "../os/runtime" "../os/services/audio" "../os/services/display" "../os/services/clock" "../os/services/settings"
    REQUIRES spi_flash passport_mp3 bt elf_loader native_exec_pool tcp_transport bsp esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash json esp_driver_usb_serial_jtag passport_lua mbedtls spiffs app_update esp_http_client esp_websocket_client esp_http_server esp_audio_codec
)

# Isolated dial-address adapter for the pinned ESP-IDF 5.5.3 transport ABI.
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/tcp_transport/private_include")

# Public fork defaults are generated outside the source tree. Only a public key
# is accepted; keep signing keys outside builds and pull-request jobs.
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../tools/passport/configure_distribution.py"
    --output "${CMAKE_CURRENT_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
    RESULT_VARIABLE passport_distribution_result)
if(NOT passport_distribution_result EQUAL 0)
    message(FATAL_ERROR "Invalid Passport distribution URL or public key")
endif()
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
