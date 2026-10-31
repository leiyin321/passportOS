[简体中文](PASSPORT_OS_DISPLAY_LAYOUT_SPEC_CODEX.zh_CN.md)

# Passport OS display layout and API specification

This is the D0–D6 implementation brief for the FoloToy AI Passport 240×320 portrait display.
The original Chinese handoff is retained in the linked paired file.

The default standard layout reserves the first 24 pixels for system status and the final 24 for
OS navigation. The application viewport is 240×272 at screen origin (0,24). Fullscreen apps use
240×320 at (0,0); both modes expose zero safe insets and a separate default 8-pixel UI padding.
Apps select their immutable launch mode in Manifest v1. Only portrait is supported. System bars,
focus/navigation and the long-OK OS exit remain controlled by the OS. Apps receive viewport
geometry through the Display API; all draw commands and child widgets are clipped by an OS-owned
LVGL host. Keep partial refresh, existing BSP locking and no full-screen RGB565 allocation.

D0 verifies panel/buttons/memory. D1 implements the display manager and standard bars. D2 adds
fullscreen and forced exit. D3 extends manifest, SDK, Lua and declarative UI. D4 standardizes
three-button press/release/long handling. D5 sets Weather and WebSocket Chat to standard mode,
and Tetris and VoiceBird to fullscreen. D6 reports build/test evidence, heap, frame/draw timing,
app switching and resource recovery.

Acceptance includes exact 240×272/240×320 API values, clipping, standards/fullscreen rendering,
working three-key navigation, long-OK exit, app error recovery and at least 200 launch/exit cycles.
See the [implementation guide](developer/display-layout.md) and the [D0–D6 report](test-reports/display-layout.md).
