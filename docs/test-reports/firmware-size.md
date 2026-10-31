[简体中文](firmware-size.zh_CN.md)

# Firmware size optimization — OS 1.0.64

Baseline is the verified OS 1.0.63 bundle `bb663367fd8a47220cb12d60b5043faf5cbb22b59a8e92c860117452e77e9d53`. Both builds use ESP-IDF 5.5.3, ESP32-C3, the tracked Size defaults and the existing 8 MB OTA/data layout. The ignored developer `sdkconfig` is untouched.

| Measurement | 1.0.63 | 1.0.64 | Reduction |
|---|---:|---:|---:|
| OTA application bytes | 2,499,472 | 2,394,768 | 104,704 (4.189%) |
| Headroom below decimal 2.5 MB | 528 | 105,232 | — |
| Merged first-install bytes | 2,630,544 | 2,525,840 | 104,704 |
| Static D/IRAM bytes | 183,280 | 173,302 | 9,978 |
| Initialized RAM data | 15,884 | 12,840 | 3,044 |
| IRAM text | 109,372 | 102,438 | 6,934 |
| BSS | 58,024 | 58,024 | 0 |

Merged-image size includes bootloader/partition/OTA padding to the application offset; the 2.5 MB target applies to the application. Each existing 2,621,440-byte OTA slot now has 226,672 bytes of image capacity remaining. Static linker RAM reduction is not a measured runtime heap peak.

## Changes and compatibility

- `CONFIG_MBEDTLS_TLS_CLIENT_ONLY=y`: removes the unused TLS server role. HTTPS/WSS clients, chain/hostname verification and the full root bundle remain. Phone setup is the existing local HTTP service, with WPA2 AP and same-origin mutation checks.
- `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT=y`: keeps expression evaluation and abort-on-failure; replaces verbose assertion strings/file paths with address diagnostics. Decode with the exact archived ELF. Assertions, stack checks, watchdogs and memory protection are not disabled. Source confirmation: ESP-IDF `components/newlib/platform_include/assert.h` and `components/newlib/src/assert.c`.
- Removes LVGL software RGB888/XRGB8888/L8/AL88/I1 paths absent from the public asset contract. Keeps RGB565/swapped, RGB565A8, ARGB8888/premultiplied and A8. Supported A1/A4 masks, I4/I8 palette images and RGB565 sprites remain; indexed images decode to ARGB8888 and masks to A8. No font quality or glyph inventory is reduced.

ELF comparison verifies all four linked glyph bitmap arrays are byte-identical. The full root bundle is **68,983 bytes**, identical SHA-256 `d47b6376ca09b7c17d379e1e828aff8e28c4c0fb689e9e3128c7a3c063912592`. `__assert_func` remains; TLS server handshake symbol is absent. Per-archive LVGL footprint falls 29,368 bytes and mbedTLS 7,712 bytes. Archive contributions also include merged-string/assertion effects and are not independent causal attribution.

## Validation and artifacts

Complete gate PASS (repository, host tests, firmware, layout, merged-image and debug-bundle verification). Temporary signed Lua `org.passport.size-probe` renders all five supported image formats; each format's actual LVGL decoder row hash matches the 1.0.63 baseline exactly on 1.0.64. Icon-font readiness, visible-glyph probe and exit asset release PASS. Existing saved Wi-Fi list is unchanged and setup/cancel works. TLS/Native/audio acceptance is recorded below after completion. Probe package is temporary and is removed after testing, not published in the Store.

Published local OTA build **2026103014**, application SHA-256 `ee1178077f41c11acb4b9110f051db98ede71301a4f90b15d09a6dc5790373a9`. Verified bundle: `build/firmware/027dba4e1587d004b667fa11610ff9a228b20e19e9708c74fddd4dc9b2ba6bbd/`; full-image SHA-256 is the bundle ID, matching ELF SHA-256 `01e57d0a0222f5a852e7f3d30ecbdf74e46d5f4d17f0c629fd179a37b88fa472`. Installed through Settings OTA, preserving Wi-Fi profiles, phone bonds and applications/data. No USB firmware flash, erase, Git commit or push.

Private evidence: `local/size-opt/` (`gate64.log`, archive reports, `elf-comparison.json`, baseline/optimized asset hashes, OTA and device logs). Never publish raw provider/device logs or signing keys. Longer soak tests, physical appearance/button acceptance and intentionally triggered assertion decoding remain unverified.

## Final device checkpoint

Settings OTA installed **1.0.64**, slot `ota_1`, state 2 (valid). Wi-Fi obtained IP at 6,627 ms. All five asset decoder hashes match baseline, font probe 23/23 and later Chinese radio probe 28/28 have no missing glyphs. Native XiaoZhi reached Ready after real HTTPS configuration and WSS hello; no PTT speech was sent. The temporary Lua five-format probe validates Lua startup/render/exit; it was then removed with its own data, preserving the original installed Native apps. Wi-Fi saved-list/portal/cancel acceptance passed.

MP3 radio: **3,629 frames**, **94,796 ms audio / 94,806 ms elapsed**, **zero starvation in this run**, average/max decode **4,615/8,760 us**, max feed gap **9,250 us**, 59 compressed blocks. Codec warm allocation 28,472 bytes. Frame-sampled heap minimum 11,180 bytes; whole-run global minimum later reached 9,136 bytes (includes UI/teardown); do not equate those measurements. Playback/network stack margins 1,760/2,480 bytes. Five seconds after radio exit: free heap **110,420 bytes**, Native pool and assets zero. After temporary-package cleanup: **110,380 bytes**, pool zero, slot still valid. No allocation failure or reboot was observed during acceptance. This short run does not establish long-run radio continuity.

Comparable post-boot metric: **101,808 → 111,808 free bytes** for 1.0.63 vs 1.0.64, a measured 10,000-byte improvement; later workloads differ and cannot be compared as identical heap snapshots. No data partitions were written by a Flash programmer; application/data changes use normal OS APIs and Settings OTA.

Build: PASS. Host tests: PASS. Device tests: PASS within the above scope. Unverified: physical visual/button acceptance, long soaks and voice capture/conversation/intentional assertion faults. Resume from the archived **1.0.64** image and its matching ELF. Existing local update and Store services remain running.

## Size visibility and storage accounting — OS 1.0.69 / 1.0.70

OS 1.0.69 adds About's running-image byte count and Store/Library package sizes. Complete gate PASS; installed through Settings OTA. Device About reported **2,418,624 bytes**, matching the packaged application exactly. Library/Store navigation and current-slot validation passed; card appearance remains physical acceptance. Bundle `build/firmware/88b3485a404218a8841f1107234f445215d8ecce62b77448c05213615b1b38f3/`; ELF SHA-256 `473795370100df61156dd8d55b5369879bb074bb1e2401a2a7f5c9142862ea48`.

OS 1.0.70 adds a storage overview, segmented package usage bar, package/previous/cache breakdown, persistent NVS entry counts and per-app usage pages. UP/DOWN browses; OK refreshes. A background worker collects a bounded twelve-app snapshot without LVGL access. The worker owns its temporary state and exits after publishing; only the main task consumes completed snapshots and updates UI. Actual file lengths distinguish current PAP files from retained `.old` packages. SPIFFS used space minus those files is reported as cache/overhead, not as app data. Persistent per-app NVS allocation is explicitly approximate: live entries × 32 bytes, excluding shared metadata and page/GC overhead. Global NVS counts include registry/system records. App availability excludes OTA slots; this is not a fully allocatable 8 MiB heap or an exact per-app physical Flash audit. All operations are read-only.

Build: PASS (complete gate, ESP-IDF 5.5.3). Host tests: PASS, including new Chinese label coverage. A compiler truncation warning in the initial build was fixed with explicit 63-byte ID bounds; the successful gate was rerun. Application **2,422,048 bytes**, leaving **77,952 bytes** below 2.5 MB; merged **2,553,120 bytes**. Signed build **2026103020** published to the existing local OTA service. Bundle `build/firmware/e771b19f01d329d6d44ec2981503cf49ddf68055bf1ea10bc743075a61f969bb/`; application SHA-256 `dcc0f053edb0abc10d9ee3fdc5ea273b817c77a282978a9459e4fa7bc55d6cd3`; ELF SHA-256 `f0fdd99b0aa64f68e90f4ed68187a65219133fce194bb1644a33cf236e4a0bdb`. Partitions, publisher keys and persistent records are unchanged. Raw diagnostics remain ignored under `local/wifi-captive/`.

Device tests: PASS on installed 1.0.70 for storage overview, current/previous/cache breakdown, NVS statistics, app-detail navigation, ten asynchronous refreshes and current OTA slot state 2. Sampled package store: 70 KiB used / 2,228 KiB total, 2,158 KiB available; current packages 32 KiB, previous packages 34 KiB, cache/overhead 2 KiB (values independently rounded down). NVS snapshot: 93 used / 16,128 total entries, 15,909 available; these figures need not sum because unavailable reserved entries are separate. Before/after ten refreshes free heap was exactly **108,372 bytes**, whole-run minimum **92,728 bytes**, largest free block **90,112 bytes**. Worker absent from completed task lists; main stack margin **2,772 bytes**, Lua usage/peak zero. This is a bounded recovery observation, not an unlimited leak test or isolated peak-RAM measurement. No USB Flash write/erase; serial closed after navigation returned home.

Unverified: physical appearance of the usage bar/card two-line layout; other language/theme visual combinations, full/failed filesystem/NVS and low-memory task-creation fault injection. Per-app usage is a point-in-time estimate and may change during concurrent writes. Historical full phone/network/audio acceptance is not repeated by this UI test.

Follow-up device checks: all three registered app usage pages PASS, and About on 1.0.70 reported exactly 2,422,048 bytes, matching the delivered image.

## Whole-Flash overview correction — OS 1.0.71

On 2026-10-31 the user accepted the other storage functions but reported the 8 MB usage distribution was inaccurate. The 1.0.70 bar used SPIFFS logical capacity as its denominator while the heading showed whole-device Flash; its earlier navigation/refresh checks did not establish correct accounting. That bar is superseded by 1.0.71.

The background snapshot now reads physical Flash size, actual app/data/package partition sizes and the running image metadata. Six nonoverlapping categories cover the whole device: running OS image; all app-slot capacity outside that image (OTA reservation, including current-slot padding); SPIFFS used files/cache; SPIFFS available file capacity; complete app-data partition reservation; remaining system/overhead, including boot/control/settings, alignment and filesystem capacity withheld from files. Data reservation is not measured live payload. Only app-available capacity can accept app files. Each KiB label rounds down independently; byte categories sum exactly to physical Flash and cumulative pixel boundaries end at 216 pixels.

Build: PASS (complete gate). Host tests: PASS, including ASan/UBSan whole-Flash sum, real OS proportion, monotonic/full-width pixel boundaries, empty/full filesystems, invalid and overflow inputs. Actual firmware application **2,422,480 bytes**, **77,520 bytes** below 2.5 MB; merged **2,553,552 bytes**. Signed version **1.0.71**, build **2026103101** published to local OTA. Bundle `build/firmware/883d29ed74e4f40d25557e8a72d6963ad316b9c134807e012350702e6fb2f49d/`; application SHA-256 `0f2b059643c28671e34588adf0dd28fc78360e3feb7a00edc551e8d6b4ae1132`; matching ELF SHA-256 `07ae9a851664204588d926960fe769659b4b7caa9eb4365fd29ed66348ddd3bb`. The first build exposed a missing explicit SPI Flash dependency; it was declared and the complete gate rerun. No partition changes, erase or USB Flash writes.

Device tests: PASS on 1.0.71 installed through Settings OTA (running slot `ota_0`, state 2). The device reports 8,192 KiB total; rounded categories: OS 2,365 KiB, OTA reservation 2,754 KiB, apps/cache 70 KiB, app-available 2,158 KiB, data reservation 512 KiB, system/overhead 331 KiB. Labels round independently, so their displayed integers need not sum to 8,192; the underlying byte model sums exactly. Package/NVS/app-detail regression and ten snapshot refreshes PASS. Before/after free heap **108,512 bytes**, global minimum **92,824 bytes**, largest block **90,112 bytes**, main stack margin **2,772 bytes**, no remaining storage worker in completed task listings. Serial closed after returning home. Logs remain private in `local/wifi-captive/` (`gate71`, `ota71`, `ui71-private`, `ui71-result`).

Unverified: physical color/proportion appearance of the corrected whole-Flash bar, cross-theme/language visuals and fault injection. User confirmation of the other 1.0.70 features is retained; it does not constitute physical acceptance of the replacement bar.

## Available-last bar and matching legend — OS 1.0.72

The six categories now share one order for accounting indices, bar segments and overview rows: OS, OTA reservation, apps/cache, data reservation, system/overhead, app-available. The available segment is last at the far right. The bar and six row swatches read the same color array. The legend is a single persistent LVGL object; row positions follow the current system font line height and label line spacing, and overview text is indented to make room. Other pages restore the original text width and hide the legend. Flash accounting and partition semantics are unchanged.

Build: PASS (complete gate). Host tests: PASS, including whole-Flash conservation, pixel boundaries and an explicit available-last/right-tail assertion. Signed **1.0.72**, build **2026103102**, published to local OTA. Application **2,422,816 bytes**, **77,184 bytes** below 2.5 MB; merged **2,553,888 bytes**. Bundle `build/firmware/71e965855ede16da06326f22677643b02eb7db23b50e657dd38a3a82f1750bd3/`; application SHA-256 `d9d15040deb835a8f384793d757e85b94755efd8586a377776c197d59fc9bda2`; ELF SHA-256 `aea0c206713ac0be5b21a11ef6fbb92c5e1d37eaa8876afcf96a5a153658e4db`. No partition, saved-profile or application-data changes. Private logs remain in `local/wifi-captive/`.

Device tests: PASS after Settings OTA installation of 1.0.72 (`ota_1`, state 2), checking 8,192 KiB overview and text order (data reservation → system/overhead → app-available), package/NVS/app-detail navigation and ten refreshes. Free heap before/after **108,512 bytes**, global minimum **92,824 bytes**, largest block **90,112 bytes**, main stack margin **2,724 bytes**, no completed storage worker remaining. Device left on the overview for physical acceptance, serial closed. No USB firmware writes. Raw logs: `local/wifi-captive/ui72-private.log` and `ui72-result.log`.

User acceptance — 2026-10-31: the user confirmed the OS 1.0.72 storage overview displays normally, including the available-last bar and matching category color swatches. This records user-reported physical acceptance, separate from automated text/navigation checks. Other language/theme combinations remain unverified.
