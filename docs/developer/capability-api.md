[简体中文](capability-api.zh_CN.md)

# Capability API 1 and application lifecycle

The machine-readable API is [api-v1.json](../../sdk/passport/api-v1.json), also available through
`passport api`. Manifest input schema: [manifest-v1.schema.json](../../protocol/manifest-v1.schema.json).
This document specifies the implemented API, rather than promising every future API in the handoff.

Only one foreground app runs. Its signed publisher+ID determines private storage; the app receives
no LVGL pointer, file descriptor, OS task, raw GPIO/Flash/radio handle or socket. All calls use the
current system-owned session. On exit/failure, the system stops VM callbacks, cancels/drains HTTP,
closes WS, stops audio with an acknowledgement, frees code/UI state and invalidates scope tokens.
A new publisher cannot claim an ID whose installed/offloaded registry entry belongs to another key.

Lua lifecycle:

```lua
function on_start(_) passport.text("Hello Passport", 0xf7fbff) end
function on_button(key) -- short tap; 0 UP, 1 DOWN, 2 OK
  passport.text("Button " .. key)
end
function on_button_down(key) end -- physical press edge
function on_button_up(key) end   -- physical release edge
function on_button_short(key) end
function on_button_double(key) end
function on_button_long(key) end -- held through the 1,800 ms threshold
function on_tick(dt_ms) end
function on_stop(_) end
```

All three keys fire `on_button_down(key)` as soon as the press is detected and
`on_button_up(key)` immediately on physical release. Each UP/DOWN release also
fires one `on_button_short(key)` and its legacy `on_button(key)` alias immediately;
rapid direction taps are never classified as double taps. OK waits for the
180 ms double-tap window before a single event is emitted. Double OK returns
Home and suppresses both constituent short events. `on_button_double` is
reserved and is not delivered to MiniApps; apps needing direction-key double
actions can detect successive short callbacks themselves. If held through
1,800 ms, `on_button_long(key)` (and legacy `on_long(key)`) fires at the threshold
instead of a short event; release still fires `on_button_up(key)`.
Immediate press/release callbacks let a voice app start capture on OK down and
stop it on release:

```lua
local microphone_allowed = false
function on_permission(allowed) microphone_allowed = allowed == 1 end
function on_button_down(key)
  if key == 2 and microphone_allowed then passport.microphone(true) end
end
function on_button_up(key)
  if key == 2 then passport.microphone(false) end
end
function on_stop(_) passport.microphone(false) end
```

The app must request `audio.features` and wait for system consent before capture.

Double OK returns to Home. `on_stop` is best effort and budgeted; it is skipped after script failure.
Persist progress at meaningful events. Each KV commit is atomic; a group of several KV writes is not
one transaction. Tetris saves its whole game in one bounded value. App code/data are independent.
Offload removes active and previous code versions, retaining data and ownership. Complete deletion
is a separate trusted system action; automatic cache reclamation must never call it.

Lua 5.4.8 uses 32-bit integers/floats, a 32KiB default Lua heap with a 64KiB manifest maximum, 40,000 instructions and
a 12ms interpreted-code target per callback. Native service calls can extend elapsed time; measure
real callbacks. An event may issue at most4 storage writes. Rendering has up to192 rectangles,
clipped by strict 216x180 coordinate bounds; no app framebuffer. Tick frequency is1..20Hz from Manifest.
`text(value, color)` updates bounded MiniApp text and optionally chooses its RGB24 foreground color (`0x000000`–`0xFFFFFF`). If omitted, the OS uses readable light `#E9F0FA`. MiniApps should select a color with sufficient contrast against their own background surfaces. `clear`/`rect` compose the canvas behind the text.

Safe libraries: selected base, table, string, math. No io/os/debug/package/coroutine or module loading,
no load/dofile/loadfile, no pcall/xpcall (which could swallow budget failures), no metatable creation,
no script GC tuning. Pattern match/gmatch/gsub, string.dump/rep/format/pack/unpack are absent.
`string.find(haystack, needle, start, true)` only supports plain search. Errors/OOM/timeouts stop the
app and show a recoverable system error. The next launch constructs a fresh VM.

Networking:

```lua
-- Manifest: network.fetch + domains:["api.example.com"]
function on_start(_)
  passport.http.request("POST", "https://api.example.com/session",
    { ["Content-Type"]="application/json" }, '{"hello":true}')
end
function on_http(status, body) end

-- Manifest: network.websocket + an exact allowlisted domain
local queued = passport.ws_connect("wss://api.example.com/socket", {
  ["Authorization"]="Bearer app-owned-token",
  ["Protocol-Version"]="1"
})
function on_http(status, body) -- WSS start reports 101; HTTP requests report their server status
end
function on_tick(_)
  if passport.ws_connected() then passport.ws_send("hello", false) end
end
function on_message(bytes, opcode) -- 1 text, 2 binary; NUL bytes are preserved
end
```

`http.request` queues HTTPS GET/POST with at most 8 custom headers, a 2 KiB request body, and an 8 KiB response. `on_http(status, body)` receives the server status and bounded body. TLS remains verified; exact Manifest domains and port 443 are required. GET redirects are reauthorized and remain same-origin when custom headers are supplied; POST redirects are not followed. Reserved transport headers and CR/LF are rejected. `passport.json.decode/encode` are bounded to 8 KiB, depth 8 and 256 values.

`ws_connect` accepts WSS only and at most 4 custom headers. Its queued completion is reported as `on_http(101, "")`; use `ws_connected()` to wait for the actual handshake before sending. Incoming messages are reassembled up to 2048 bytes (text and binary opcodes remain distinct), with bounded queues. Sending is limited to 50 messages/s and 16 KiB/s. TLS, DNS/IP checks, and exact domain allowlists still apply; automatic reconnect is disabled. `system.public_device_identity()` returns the Wi-Fi MAC and stable app-scoped UUID only with `device.identity`; it never exposes hardware secrets.

Microphone and generic Opus stream:

```lua
function on_permission(allowed)
  if allowed == 1 then passport.audio.stream_start() end
end
function on_audio_frame(opus_bytes)
  -- This app decides whether/where to send the Opus frame.
  passport.ws_send(opus_bytes, true)
end
function on_message(bytes, opcode)
  if opcode == 2 then passport.audio.play_opus(bytes) end
end
function on_stop(_) passport.audio.stream_stop() end
```

Declare `audio.stream` and (for RMS-only local features) `audio.features`. The foreground consent screen explains that streaming apps may send audio to their approved endpoint. The current stream API captures and encodes 16 kHz mono Opus in 60 ms frames; it never exposes PCM. Each playback frame is a raw Opus v1 packet and is queued for 16 kHz mono decode/playback. Capture/playback queues are bounded, and the OS stops the worker and closes the codec on app exit or fault. `audio.stream_stop()` requests stop without blocking the Lua callback. `passport.microphone(true/false)` remains the local RMS-only API and does not upload audio.

System language:

```lua
local language = passport.system.language() -- "en" or "zh-CN"
```

This read-only value follows Settings → Language and is available without a permission. MiniApps
should select their own translated names, content, and prompts from it; Passport OS does not rewrite
strings supplied by an app.

Display layout is selected from the signed Manifest `display.mode` at launch. It defaults to `standard`;
`fullscreen` is available to game apps. Only portrait orientation is accepted. The app cannot switch mode
at runtime. The Lua API `passport.display.get_viewport()` (also `getViewport`) returns the clipped app
width, height, zero safe insets and mode. Standard mode exposes 240×272 below the 24px status bar;
fullscreen exposes 240×320 with system bars hidden. The 8px theme padding is independent of that size.
The AppViewportHost clips text, canvas rectangles, symbols and package images. Declarative apps read
`display.width`, `display.height`, `display.mode` and `display.safeInsets.*`; this runtime object is not
written to app storage. `navigation` is an optional constrained footer hint (`buttons`, `scroll`, `select`,
`game`). Double OK is reserved by the OS to return Home. Lua apps can use long OK and all press/release
and short/long callbacks.

See the [Display layout API](display-layout.md) for examples and coordinates.

Declarative apps use a separate bounded `ui.json` document, not Lua callbacks; see the [Declarative UI/1 contract](declarative-ui.md) for supported nodes, actions and limits.

System icons and package sprites are implemented as `passport.ui.icon` / `passport.graphics.sprite`; see the [asset API](assets.md).

Deferred APIs: arbitrary UI widgets with independent focus trees, line/text canvas commands, raw PCM access, device/cache/channel/BLE services, public OTA transport and multi-app background execution. These require API-versioned additions; app manifests cannot enable them today.

The XiaoZhi example MiniApp uses these generic capabilities for its own provisioning request, authentication headers, JSON hello/session state, Opus framing and conversation UI. XiaoZhi-specific endpoint rules, account tokens, and protocol interpretation remain app code. See [XiaoZhi MiniApp](xiaozhi-miniapp.md).

`passport.system.display_mode()` reads the system appearance selected in Settings and returns `dark` or `light`. It is read-only. Apps can follow this value as a palette hint or define independent backgrounds and foreground colors; the OS does not force its theme onto the app canvas. `passport.text(value, color)` accepts an optional RGB24 foreground color, while `passport.rect` lets the app draw its own background surfaces.

## Audit increment: OS 1.0.22

HTTP response bodies now support bounded chunked/EOF framing as well as Content-Length.
A failed header read is a transport error; a partial/oversized response is never delivered
as success. OTA still requires the signed exact size. Header names/values reject NUL
as well as CR/LF and reserved fields. The response cap is a maximum, not a preallocated
8 KiB buffer: storage grows after TLS handshake as payload arrives.

JSON depth and value limits are checked before recursive C parsing. Temporary JSON
allocations are freed even when Lua conversion runs out of memory. Embedded NUL
keys/strings are rejected instead of being silently truncated.

The runtime consumes the loaded package source when compiling/parsing it; C callers
must reload the package before starting it again. MiniApp APIs are unchanged. Lua
quota and available system heap are separate: request/codec allocation can still fail.
See the [system review](../development/passport-os-review.md) for resource evidence
and remaining lifecycle/whole-system memory work.

### TLS memory behavior (OS 1.0.24)

HTTP and WSS retain certificate/hostname verification and domain policy. The OS collects unreachable Lua objects before custom HTTP/WSS submission and uses ESP-IDF dynamic TLS record buffers to lower peak heap pressure. This is shared infrastructure, not a service-specific provider. A false submission result can mean contention or resource/policy failure; asynchronous HTTP -1 can mean transport/TLS failure. Neither is proof of disconnected Wi-Fi. Apps should bound retries and preserve error feedback.

For gesture-based apps, keep the connection-in-progress guard after status 101; clear it only after `ws_connected()` succeeds or an explicit failure/timeout. A press followed by a click must not restart a handshake.

OS 1.0.26 preserves the approved hostname in WebSocket Host/SNI/verification while pinning the TCP dial address to the IP already checked by network policy. No second hostname DNS lookup occurs during socket connect. The ESP-IDF 5.5.3-specific connect adapter is isolated in `os/services/network/ws_transport.c`; revalidate its private transport ABI when changing IDF versions.

Opus playback output is 16 kHz mono PCM. Do not reject an otherwise compatible Opus stream merely because the producer uses another supported Opus sample rate; the decoder selects its output rate. The current playback buffer accommodates up to 60 ms per packet at 16 kHz. OS 1.0.27 budgets 10 KiB stack for the audio task; hardware stack/heap headroom remains subject to acceptance testing.

`audio.stream_start()` may return false if the worker cannot be created. A later worker startup failure clears the active capture flags but does not revoke user consent, then calls `on_audio_error(code)`; code 1 means stream startup failed. Stop/clean up and allow a later retry. Reserve `on_permission(0)` for an explicit user denial.

Native OS 1.0.51 adds `audio.playback`, restricted to trusted Native ABI 2 apps. With `network.fetch` it enables the optional HTTPS MP3 media host extension without microphone permission. Declare each stream/redirect domain explicitly. See [media ownership, API and limits](city-radio.md).
