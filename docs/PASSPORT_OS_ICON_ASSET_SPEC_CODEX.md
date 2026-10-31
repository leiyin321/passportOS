[简体中文 — complete original specification](PASSPORT_OS_ICON_ASSET_SPEC_CODEX.zh_CN.md)

# Passport OS icon and asset implementation handoff

The original user specification is preserved in the linked Chinese edition.
Implement I0–I6 against actual LVGL version and measured ESP32-C3/no-PSRAM limits.
Reuse BSP, LVGL symbols and native image decoding. Do not add on-device SVG/PNG engines or full-screen framebuffers.

| Gate | Required implementation and evidence |
|---|---|
| I0 | Pin LVGL major/minor, audit decoder/configuration, baseline image/Flash/RAM |
| I1 | Shared System Icon Registry, >=12 semantic IDs, theme recolor and unknown-ID fallback |
| I2 | Offline PNG/SVG compiler using mature rasterization/quantization; A1/A4/I4/I8/RGB565; exact sizes and reports |
| I3 | Signed PAP assets, indexed metadata/hashes/offsets, current-app URI isolation; no arbitrary filesystem paths |
| I4 | Generic sprite drawing, on-demand bounded asset loading, session-owned buffers, clean exit |
| I5 | Thirty-icon scroll fixture, bounded visible cache, 500 open/close cycles and memory measurements |
| I6 | Offload/data separation, interrupted-update recovery and OTA/reboot regression |

Required examples: icon_gallery (12 shared icons, theme/size/fallback), weather48x48 custom icon,
Tetris procedural graphics plus optional small sprite, transparent VoiceBird sprite plus audio,
and appstore_grid containing at least30 assets. Package resources are disposable code/cache;
App Data never belongs to image-cache eviction. Missing/offloaded icons show the system placeholder.

Prefer uncompressed A1/A4/I4/I8 and limited RGB565. A1 24x24 pixel payload72B;
I4 48x48 pixel payload1152B plus palette. Report actual wrapper/palette/stride costs and RGB565 comparisons.
Image formats must follow the pinned LVGL ABI or a documented validated wrapper translated by the OS.
Never pass a custom header, unchecked pointer or untrusted path directly to LVGL.
No arbitrary runtime indexed-image scaling/rotation assumptions; precompile required sizes/directions.

Validate dimensions with overflow-safe math, exact lengths, palette/index validity, duplicate IDs,
path traversal, source file/pixel/frame limits, malformed/compressed input, hashes and interrupted updates.
An image belongs to a foreground session. Cache budget starts8–16KiB, with measured peaks/latency,
free heap/largest block after close, CPU/frame statistics and Flash/component-size evidence.
Run host/simulator/hardware tests separately and never present simulation as device PASS.
Maintain SDK/protocol/developer documentation and reproducible committed checkpoints for every gate.
