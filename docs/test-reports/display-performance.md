[简体中文](display-performance.zh_CN.md)

# Display latency acceptance — 2026-10-29

Firmware: Passport OS 1.0.45, ESP32-C3, 240×320 RGB565, no PSRAM. The before measurement used OS 1.0.41. Raw serial captures stay under ignored `local/native_xiaozhi/`; this report includes only timings.

## Method and measurements

At Home, issue an injected DOWN click through the normal foreground input queue, then immediately request the serial screen snapshot. Read each response through `PASSPORT DONE`. Before: five trials; after: ten trials. This measures software command/lock latency and does not measure LCD photon response, mechanical switch debounce or the OK double-click classification window.

| Measurement (ms) | Range | Mean |
|---|---:|---:|
| Old screen query after click | 923.88–928.56 | 925.35 |
| New screen query after click | 86.30–116.07 | 103.45 |
| New click command ACK | 1.20–1.92 | 1.48 |

The display event probe recorded input to the next refresh-ready event at 85.11–114.18 ms; the corresponding render durations were 83.01–112.02 ms. A refresh already in progress can complete first, so this probe alone does not prove that changed pixels reached the panel. Full-screen transitions and application text redraws can be slower than Home focus changes.

## Changes

- Cache the applied theme and unchanged label strings, eliminating unnecessary root/style invalidation and repeated text layout.
- Home focus invalidates the old and new card bounds. Custom card/settings drawing skips items outside the active LVGL clip rectangle.
- Limit LVGL idle sleep to 50 ms. Use one four-row DMA tile (1,920 bytes), a 5 KiB LVGL task and the existing 16 KiB LVGL pool; retain single buffering on the no-PSRAM C3.
- Batch bounded network deliveries and defer UI drawing to the normal FPS/view-change path. WebSocket TX runs in a separate bounded 3 KiB task.
- UP/DOWN remain immediate clicks on release. OK retains its 180 ms single/double classification window so a double click does not trigger confirmation first. Raw press/release remain available for push-to-talk.

## Validation

Build: PASS — full `./tools/validate.sh` under ESP-IDF 5.5.3.

Host tests: PASS — complete repository/host gate, including button and font coverage regressions.

Device tests: PASS for the stated serial timing measurements and clean Home navigation; no claim of instrumented optical latency or perceived animation smoothness.

Unverified: human confirmation of physical button feel and visual smoothness, optical latency and power measurement. Native voice/font/memory details: [Native XiaoZhi report](xiaozhi-native.md).
