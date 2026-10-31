[简体中文](settings-scroll.zh_CN.md)

# Settings Scrolling and Chinese Glyph Report

Date: 2026-10-25 · Firmware: 1.0.15 / build 2026102518

## Changes

- Settings first-level menu now virtualizes eight rows in the 240 × 192 content
  area and displays a right-side scrollbar. UP/DOWN moves selection through the
  full list; the viewport follows the selected row. Adding settings no longer
  requires compressing every row into one screen.
- Added a host-tested scroll-base calculation that keeps selection visible and
  clamps the viewport at both ends.
- Added U+95F9 (alarm glyph) to the generated Source Han Sans subset. The
  generated subset supplies U+6EDA; LVGL's fallback subset supplies U+52A8.

## Validation

| Check | Result |
| --- | --- |
| Scroll viewport host tests | PASS |
| Generated glyph coverage: U+53CC/U+51FB/U+95F9 | PASS |
| LVGL fallback glyph source: U+52A8 | PASS |
| Full repository validation (`PASSPORT_OS_VERSION=1.0.15 ./tools/validate.sh`) | PASS |
| Signed release and served image integrity | PASS; downloaded image hash matches release |
| On-device scrolling and Chinese rendering | NOT RUN; user requested no hardware connection |

## Resource usage and release

- Application image: 2,067,776 / 2,621,440 bytes; 553,664 bytes free (21.12%).
- Merged initial-flash image: 2,198,848 bytes.
- Generated CJK subset: 133 code points, 113,325-byte generated C source; linked
  font data in this build: 16,932 bytes of Flash. Font bitmaps are read-only
  Flash data; no per-page heap font allocation was introduced.
- Signed update `1.0.15` / build `2026102518` is available at
  `http://192.168.1.10:8765/latest.json`. App image SHA-256:
  `14f762f8012ae56590b0e0df3cf65272914c866d89c0d9f89bf74168f583403f`.
- Build bundle: `build/firmware/21ea3d5371a0fa6c12ec703ece6c114bf5a7af580206c38b499d42354a043422/`.
