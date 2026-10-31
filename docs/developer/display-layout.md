[简体中文](display-layout.zh_CN.md)

# Display layout and Display API v1

The panel is 240×320 portrait. Manifest `display` is optional and defaults to standard mode:

```json
"display": { "mode": "standard", "orientation": "portrait", "navigation": "select" }
```

Only `standard` and `fullscreen` are accepted. Orientation is `portrait`; other values fail package
validation. `navigation` is a constrained hint: `buttons`, `scroll`, `select`, or `game`. The OS
chooses its own translated footer. `design_width`/`design_height` are hints only. Apps cannot switch
modes dynamically.

| Mode | Status | App viewport | Navigation |
| --- | --- | --- | --- |
| standard | y=0..23 | (0,24), 240×272 | y=296..319 |
| fullscreen | hidden | (0,0), 240×320 | hidden |

Safe insets are all zero; the UI theme's recommended 8px padding is separate. In Lua:

```lua
local vp = passport.display.get_viewport() -- getViewport is an alias
-- vp.width, vp.height, vp.mode, vp.safeInsets.left/top/right/bottom
```

The viewport is read-only. `passport.rect`, system icons, package sprites and declarative image
nodes use viewport-relative coordinates. Draw rectangles are clipped to its bounds; invalid origins,
negative sizes, unsupported images and excessive coordinates fail safely. The OS-owned clipped LVGL
AppViewportHost is the only parent for app canvas and text. Do not reach into system objects, drivers,
raw display handles or the framebuffer.

Declarative `ui.json` bindings can read `display.width`, `display.height`, `display.mode`, and
`display.safeInsets.*`. The reserved runtime `display` object is omitted from persisted app state.
Use the `navigation` hint for standard-mode footer text; button presses remain the existing stable
`on_button(0 UP / 1 DOWN / 2 OK)` event. Lua also receives `on_button_up(key)` on release and
`on_long(key)` for legacy long events. The API also provides immediate down/up edges and
classified single/long callbacks. UP/DOWN report each short tap immediately; fast repeated taps
never become doubles. OK waits 180ms to distinguish a single confirmation from double OK, which
returns Home and suppresses both short callbacks. Long OK is delivered to the app. BSP long timing
is 1,800ms.

The status bar owns app title, network and battery. The bottom strip owns navigation hints. Standard
apps get 240×272 clipped content; fullscreen hides both bars and receives 240×320. A microphone
consent prompt temporarily restores system bars. App exit/fault restores standard system UI and closes
image, audio, network and VM resources while retaining package and private save data.

## Phone Wi-Fi setup

Open **Settings** from Home and press OK. The Passport starts a temporary WPA2
access point with a per-session random key, shown on the device. Join that
network on a phone and open `http://192.168.4.1`; enter the target 2.4 GHz SSID
and password. The setup page is served locally by the device. Credentials are
stored by ESP-IDF Wi-Fi configuration in NVS and are never printed to the serial
log. After the station gets an IP address, the setup server and AP stop. Double OK or `home` cancels setup. If the network rejects the credentials, the AP stays up
so the user can retry. A manual URL is required; this release does not provide a
DNS-based captive portal.

The setup form accepts one SSID (1–32 bytes) and one password (empty for open
networks or 8–63 bytes), caps the request at 512 bytes, decodes form escapes,
and rejects duplicate/unknown fields, control characters, invalid escapes and
truncation.

Examples: Weather/Chat use standard; Tetris/VoiceBird use fullscreen. The [hardware report](../test-reports/display-layout.md)
separates host, device and visual checks. No full-framebuffer allocation is part of this API.
