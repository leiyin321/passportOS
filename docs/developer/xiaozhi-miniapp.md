[简体中文](xiaozhi-miniapp.zh_CN.md)

# XiaoZhi AI MiniApp

## Implementation

`examples/xiaozhi/` now implements a bilingual XiaoZhi WebSocket MiniApp. It obtains a read-only public device identity, posts device/app information to the XiaoZhi-compatible OTA/config endpoint, reads activation instructions and WebSocket configuration, and stores the app's URL/token in its own private storage. It ignores firmware-update fields. When configuration is available, it opens WSS with app-supplied Authorization, Protocol-Version, Device-Id, and Client-Id headers, sends protocol v1 `hello`, and validates the server transport/audio format.

Holding OK sends `listen/start`, requests the OS's generic 16 kHz mono Opus stream, and sends each encoded Opus v1 frame as a binary WebSocket message. Releasing OK sends `listen/stop` and requests capture stop. Incoming `stt`, `llm`, `tts`, `alert`, and `goodbye` JSON messages update the Voice and Conversation pages; incoming Opus binary frames are queued to generic OS decode/playback. Unsupported `mcp`, legacy `iot`, and `system` commands are ignored. Double OK is consumed by the OS to leave the MiniApp.

Version 0.2.1 introduced the full-screen 240×320 Display API viewport. Version 0.2.2 reads `passport.system.display_mode()` and chooses its own matching palette: light surfaces with dark text in Light mode, or dark surfaces with light text in Dark mode. The app sets text color through `passport.text(value, rgb24)`; it owns these colors and can be restyled independently of the system theme. Version 0.2.3 retries service configuration when the OS network worker temporarily rejects a request or returns a transient HTTP error. A rejected request submission no longer appears as a Wi-Fi setup error; the app shows a network-wait message and retries automatically. The Manifest declares `display.mode: fullscreen`, so the OS hides its status/navigation bars while this app is active.

XiaoZhi-specific config fields, authentication semantics, protocol messages, and conversation presentation remain in Lua. OS services expose provider-neutral HTTPS/WSS, JSON, identity, and Opus APIs. The app does not receive PCM, socket handles, hardware secrets, or codec handles.

## Configuration and compatibility

The app currently targets `https://api.tenclass.net/xiaozhi/ota/` and accepts a `websocket` object with string `url` and `token` fields. It requires a `wss://` URL whose exact host is declared in the Manifest. The manifest currently allows `api.tenclass.net`; if the service returns a different host, the manifest must be updated to that exact host before it can connect. Activation code/message are displayed so the user can activate the device in their account; firmware URLs and OS update fields are never acted on.

The generic OS audio API currently supports 16 kHz mono Opus v1 frames of 60 ms. The app advertises those parameters and rejects incompatible server hello formats. This package needs Passport OS 1.0.17 or later because it declares a 64 KiB Lua heap. MCP, AEC, MQTT, UDP, wake-word, and automatic background listening are not supported. The user must hold OK for microphone upload; no recording continues after release or app exit.

## Validation and remaining device work

The OS transport preserves TLS verification, exact Manifest-domain enforcement, bounded request/response/message sizes, reserved-header rejection, WSS reconnect policy, foreground microphone consent, bounded audio queues, and app-exit teardown. Audio worker diagnostics report codec heap before/after, free heap after codec close, largest free block, and task stack high-water mark. Firmware and host resource measurements are in the [XiaoZhi test report](../test-reports/xiaozhi-miniapp.md); physical-device heap and stack figures remain pending device acceptance.

The MiniApp has been signed and SDK-validated. Static/host validation and the isolated ESP32-C3 firmware build pass. It is now the sole entry in the local Mac development App Store catalog at `http://192.168.1.10:8040/index.json`; the index and package were fetched from the live server and verified. Activation against the official service, English/Chinese rendering on hardware, real audio round-trip, live heap peak, and post-exit heap recovery still require device acceptance before public catalog distribution.

## Protocol references

- [XiaoZhi official platform](https://xiaozhi.me/)
- [XiaoZhi WebSocket protocol](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)
- [FoloToy Passport XiaoZhi reference](https://github.com/FoloToy/folo-ai-passport-xiaozhi)
- [FoloToy reference OTA/config implementation](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/main/main/ota.cc)
- [Passport Capability API](capability-api.md)

## Connection recovery in 0.2.4

Use OS 1.0.24 or later for dynamic TLS buffers and Lua collection before network submission. A generic HTTPS failure is not a Wi-Fi-disconnected signal. The app makes at most three automatic configuration attempts, retains the failure status, and lets OK on Connection start a new attempt sequence. An activation code opens Connection; complete activation in the official account and press OK to refresh. Empty tokens are permitted; Authorization is omitted when empty. The service's firmware payload never updates Passport OS. See the [measured failure and release report](../test-reports/xiaozhi-miniapp.md#tls-allocation-failure-os-1024--miniapp-024-2026-10-26).

## Audio startup recovery in 0.2.7

Microphone consent is independent of Opus/I2S worker startup. OS clears the active capture state and delivers generic `on_audio_error(1)` when the worker fails; it keeps the consent decision. XiaoZhi shows an audio unavailable retry hint for both immediate and asynchronous startup failures. It sends the server `listen/start` only after local capture starts, and preserves “permission denied” for explicit consent denial.

## OS 1.0.29 audio worker allocation

After 1.0.28/0.2.7, consent stayed granted but audio still could not start. The service allocated two stream queues before its 10 KiB task stack. After WSS connected, the observed largest free block was about 9.7 KiB before the queues and 7.7 KiB afterward, so the contiguous task-stack allocation could not succeed. The service now allocates the worker first with a 9 KiB stack, blocks it on a startup semaphore, then allocates the queues and releases it. Startup logs identify semaphore, task-stack, or queue failure with free heap and largest-block values. The host lifecycle test injects failure at every allocation and checks cleanup.

OS 1.0.29 / build 2026102610 is on the Mac update service. Firmware and host tests pass. The 9 KiB stack leaves headroom beyond the previously measured 4,752-byte capture frame; codec-call high-water and successful physical push-to-talk remain to be measured after OTA installation.

## OS 1.0.30 and MiniApp 0.2.8: reserve before network, stop retrying while held

On the installed 1.0.29 device, serial logs confirmed every start failed at
`stage=task-stack bytes=9216` with 22.4 KiB free but only a 7,680-byte largest
block. Moving queue allocation alone did not make that stack available after
TLS/WSS fragmentation. The OS now prepares the idle audio task when opening an
app that declares `audio.stream`, before its network callbacks run. The task
waits on a gate and never initializes or reads the microphone until the app has
permission and starts a stream. A prepared worker is retained between PTT
sessions and reclaimed on app exit or permission denial. This preserves the
9 KiB stack required by the earlier stack-fault evidence.

MiniApp 0.2.8 clears its held-to-talk state after immediate or asynchronous
audio failure. Previously `on_tick` retried while OK remained logically held,
producing repeated allocation attempts and replacing the error feedback. The
user must release OK before retrying. Host tests verify one stream-start call
across subsequent ticks after both failure paths.

OS 1.0.31 supersedes 1.0.30 on the update service and keeps the same pre-network
reservation while preserving ordinary microphone capture for apps that also
declare stream permission. XiaoZhi 0.2.8 is available in the local App Store.

The installed 1.0.31 device then showed WSS TLS certificate-signature
verification failing with mbedTLS `0x4290`, followed by handshake `-0x3000`.
The reserved worker stack was competing with RSA verification for ordinary
heap. OS 1.0.32 places the same 9 KiB stack in ESP32-C3 Retention RAM through
the FreeRTOS task-with-capabilities API; the task control block remains in
internal RAM. This keeps the large regular heap blocks available to TLS. The
release is published, but WSS and push-to-talk must be retested after OTA.

The user reproduced the same WSS failure on 1.0.32. The Retention stack still
reduces RAM available to allocations requiring `MALLOC_CAP_INTERNAL`; Retention
and normal DRAM are not independent pools for these requests. OS 1.0.33 therefore
defers audio worker creation until `audio.stream_start()` after TLS and hello.
Microphone permission grants no longer allocate the stack. Heap metrics now
distinguish Internal and Retention RAM free totals and largest blocks. Device
TLS, audio, peak-memory, and reclamation checks remain pending OTA installation.
