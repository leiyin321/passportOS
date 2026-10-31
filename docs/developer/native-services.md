[简体中文](native-services.zh_CN.md)

# Native ABI 2 services

Requires Passport OS 1.0.35 or later and a trusted Native manifest with `abi:2`. ABI 1 and Lua remain supported. Include [`passport_native_v2.h`](../../sdk/native/include/passport_native_v2.h); machine metadata is [`abi-v2.json`](../../sdk/native/abi-v2.json). Use the existing SDK build, pack, validate, install and catalog tools described in [Native development](native-apps.md).

The entry still receives the ABI 1 prefix pointer. Check `host->abi==2` and `host->size>=sizeof(passport_native_host_v2)` before casting. Return `app.base.abi=2`, `app.base.size=sizeof(passport_native_app_v2)`, synchronous `event`/`stop`, and optional `data(kind,status,bytes,length)` callbacks. The ILP32 host/app tables are 80/20 bytes. The OS validates callback addresses before accepting the application; no direct LVGL, driver, heap or RTOS exports are added.

| Method | Contract |
|---|---|
| `network_ready()` | Actual OS Wi-Fi readiness; requires fetch or websocket permission |
| `http_request(method,url,headers,body,length)` | Async GET/POST over HTTPS; up to 2048 body bytes, 1024 header bytes/eight headers; requires `network.fetch` |
| `ws_open(url,headers)` | Async WSS handshake; up to four bounded headers; requires `network.websocket` |
| `ws_send(data,length,binary)` | Queue/copy at most 2048 bytes; same websocket permission |
| `ws_connected()` / `ws_close()` | Query or close the session-owned socket |
| `json_get(json,length,path,out,capacity)` | Dotted object path; copies a decoded string, number or boolean; nonnegative result is byte length excluding NUL, -1 invalid/missing, -2 insufficient capacity. No partial output. At most 16 KiB JSON, 127-byte path and 4096-byte output buffer |
| `identity(device,device_cap,client,client_cap)` | Requires `device.identity`; device buffer ≥18, per-app stable client buffer ≥37; same identity scheme as Lua |
| `storage_get(key,out,capacity)` / `storage_set(key,value)` | Existing per-app private storage and manifest quota; requires `storage.private`; at most four writes per callback |
| `audio_mode(1/0/-1)` | Request capture, playback-only, or close session; requires `audio.stream` and the OS microphone consent |
| `audio_play_opus(data,length)` | Copy a packet ≤1024 bytes into bounded playback queue; same permission and consent |

Service operations return 1 when accepted and 0 when rejected, except `json_get`, which uses the result above, and void close. Audio requests set managed view state; the OS foreground loop starts the worker asynchronously. Audio mode 0 does not start microphone capture. Mode -1 ends the managed audio session; worker reclamation happens in the OS loop. Application code must return promptly. Generic storage uses the existing synchronous bounded storage implementation.

HTTP bodies, headers and WS messages are copied into OS-owned jobs/queues before calls return. DNS/address/port/domain validation, TLS certificate checks, redirect allowlists and message quotas reuse the existing network service. No XiaoZhi URL, credential, authentication format or protocol is embedded in these Native host methods.

Async delivery is on the foreground application context:

- Event 7: microphone consent, value 1 granted / 0 denied. The OS sets consent before calling the application; denial and worker failure are distinct.
- Event 8: audio startup error; the value follows the runtime's audio error codes.
- Data kind 1: HTTP completion, including status 101 for successful WSS upgrade and negative status for failure. Late HTTP results with an old session generation are rejected.
- Data kinds 2/3: text/binary WebSocket messages; status is the opcode.
- Data kind 4: captured mono 16 kHz Opus packets, normally 60 ms each. Delivery stops when microphone capture stops.

Data pointers are borrowed and valid only for the callback duration. Copy retained strings; never store an incoming frame pointer or hand a Native callback to a worker. Stop runs while code is still loaded; the managed session then cancels workers, drains messages and releases audio/network resources before the package is released. No application callback is invoked after ELF unload.

The Native pool remains 16 KiB and the ELF file limit remains 32 KiB. This privileged API is cooperative, not a sandbox. Additional Lua publishers cannot install Native packages. Application-specific authentication and state machines belong in the application; see [Native XiaoZhi](xiaozhi-native.md).

For on-device automation, `passport device button INDEX EVENT --port PORT` supports indexes 0/1/2 and press/release/click/long; double is accepted only for OK. It injects the same queue events as the BSP and does not synthesize click on press. Physical button timing and microphone acoustics still require board acceptance.

OS 1.0.36 adds the Native half-duplex policy for the no-PSRAM C3: capture/playback hold the encoder and decoder alternately. Select `audio_mode(0)` before playback; playback during capture returns 0. Native selects this policy explicitly; legacy Lua duplex behavior remains compatible. Playback-only startup does not create an encoder.

The validated C3 voice baseline is OS 1.0.45: PCM scratch is heap-owned, the worker has a 24 KiB streaming stack allocation, and the generic encoder uses low-complexity CELT 20 ms frames aggregated into RFC 6716 60 ms CBR packets. This is internal codec scheduling; ABI/network packet duration remains 60 ms for both Lua and Native. Native defers I2S until after encoder allocation and releases DMA/channels on exit. Shared I2C remains available.

Encoder output capacity is checked against `esp_opus_enc_get_frame_info_by_cfg`, not inferred solely from compressed bitrate: the current profile reports 640 PCM input bytes and at least 160 output bytes per internal frame. The OS owns scratch, codec and I2S/DMA teardown.

Native playback submission can wait up to 80 ms for the bounded audio queue. WS worker delivery waits at most 1000 ms for foreground consumption before reporting a stalled queue. This provides bounded TCP backpressure for speech bursts without growing buffers; the BSP button callbacks remain queue-only. HTTP/WS task stacks are 6/4 KiB after measured stack-margin checks. The LVGL pool is 16 KiB and the display DMA tile is four rows. Full TLS chain validation remains enabled; only the unused post-handshake peer-certificate copy is released.

WebSocket delivery drains at most four messages per foreground iteration and does not wait for LVGL between frames. The existing FPS/view-change path coalesces drawing. The SDK connection-state query is lock-free; receive backpressure addresses burst scheduling rather than replacing that query.

OS 1.0.45 adds a dedicated 3 KiB WebSocket TX worker. The foreground enqueues copied messages (eight slots, at most 4096 pending payload bytes) rather than contending with the SDK receive lock. Exit joins the worker and releases pending messages. App labels always use the shared multilingual font chain even with English system UI; a compact supplement covers GB2312 level 1 and common chat punctuation. `font-probe` checks the actual widget font; `display-metrics` reports queued-input/render-ready timings.

The pinned Espressif WebSocket client uses its supported separate TX lock (`CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK=y`), so an incoming speech stream does not hold the outbound audio/control path. Audio input/output queues each hold two packets (120 ms for 60 ms packets); pressure remains bounded and teardown drains them.

## Optional BLE HID extension

OS 1.0.47 adds a separate optional 96-byte HID host table whose first 80 bytes are the unchanged core ABI 2 table. Existing binaries retain compatibility. Trusted Native apps declare `bluetooth.hid` and check the larger host size before using [`passport_native_hid.h`](../../sdk/native/include/passport_native_hid.h). The extension supports bonded BLE pointer/Consumer Control reports; it provides no A2DP/HFP audio or telephone support. The API/lifecycle and iPhone setup are documented in [Bluetooth peripherals](bluetooth-peripheral.md). Lua cannot request this first extension.

## Optional network media extension

OS 1.0.51 adds the 120-byte optional Native media table: HTTPS MP3 playback, stop/state, system media volume, read-only wall time and battery. It preserves the 80/96-byte prefixes and needs `audio.playback` plus `network.fetch`; no microphone consent is required. See [City Radio and generic media API](city-radio.md) for ownership, limits and migration.

OS 1.0.56 additionally supports explicit `audio.http` for public broadcasts, requiring trusted Native ABI 2, both media/fetch permissions, exact manifest domains and public IPv4 port 80. Credentials/query strings are forbidden; ordinary fetch/WS retain their secure policy. Recommend 1.0.59 or later for compressed-byte packing and adaptive TX-only playback.
