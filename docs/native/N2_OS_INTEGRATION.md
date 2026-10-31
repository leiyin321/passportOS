[简体中文](N2_OS_INTEGRATION.zh_CN.md)

# Native ELF integration — Passport OS 1.0.34

Implemented: shared C3 loader/protected pool, binary package validation and built-in publisher trust, AppManager lifecycle, callback ABI, SDK compiler/scaffold/inspection, and Native Hello. Lua and declarative applications remain supported. This is the initial integrated profile, not completion of every N2–N6 capability. Native networking, audio, storage, asynchronous workers, RTOS and XIP remain unimplemented.

## Flash and memory

| Measurement | Bytes |
|---|---:|
| OTA application | 2,120,064 |
| Merged first-install image | 2,251,136 |
| Each OTA slot (unchanged) | 2,621,440 |
| Permanent executable arena | 16,384 |
| Native Hello ELF file | 2,512 |
| Native Hello loaded span / measured pool peak | 1,472 |
| Pool used after application exit | 0 |
| Linked static RAM | 185,876 |

Both images meet a decimal 2.5 MB budget. Size optimization uses `-Os`, disables broad ELF symbol exports, and reuses existing rendering and sessions. TLS roots, CJK fonts, Lua, audio and assertions remain enabled. A same-feature `-Og` comparison is 2,315,088 bytes, 195,024 bytes larger (8.42%); this is a controlled comparison, unlike an old cached build. The executable arena contributes permanently reserved RAM and is not returned to the general heap on exit. Generic EXEC heap remains zero; ELF allocations use the dedicated arena. Memory protection, locks and monitors remain enabled. Metric `protection=0` means verification returned ESP_OK.

Native Hello leaves roughly 82 KB general heap while active and 84 KB after exit; concurrent networking makes this approximate. Linked RAM is not a runtime heap measurement. The global low watermark after Lua execution must not be attributed to Native alone.

## Validation and device results (2026-10-29)

- Repository static checks, host regression and firmware/merged-image gate passed. Native validation includes 2,500 Python/C mutation and truncation parity cases plus an ASan/UBSan harness with 20,000 mutations.
- Known ESP32-C3 USB device booted 1.0.34 and reported OTA VALID. Only the application slot at 0x20000 was written; partition table, NVS, packages and appdata were preserved. Prior 1.0.33 remains in the other slot.
- Signed Native Hello installed, launched, received a synthetic OK click, updated its counter, stopped and restarted. Standard viewport is 240×272. Lua XiaoZhi also launched and exited; this does not validate its remote service or audio.
- First 1,000 lifecycle cycles passed, with a 132-byte total heap decrease. A second 1,000 cycles passed with zero drift. On the final exact firmware, another 1,000 passed with zero drift in 75.214466 seconds. Pool usage returned to zero after each exit; peak was 1,472 bytes. The initial fluctuation is retained rather than described as universal zero allocation drift.
- Final firmware forced 100 render/lifecycle cycles: 100 passed in 21.589561 seconds, total heap drift -112 bytes. This includes session transitions and is not a frame-rate benchmark.
- A correctly signed temporary application returning ABI 2 was rejected by ABI 1 runtime: launch FAIL, error reported, pool returned to zero (628-byte temporary span). The test application was removed. Native Hello launched successfully afterward.
- Physical LCD appearance, physical button feel and native audio/network behavior are not validated by these automated checks. No such native audio/network API is exported yet.

## Reproducible artifacts

Verified archive: `build/firmware/28fa71885fd2e59e8d748c62273cbe81061ca0004741d34119a03e6bb7063a3b/`.

- OTA SHA-256: `965c257ad5b4677fa8decbf02aa04d82d889e06f252cb9f9048f2ad246092bb2`
- Merged SHA-256: `28fa71885fd2e59e8d748c62273cbe81061ca0004741d34119a03e6bb7063a3b`
- Matching ELF SHA-256: `b2dd2cf1f38f02cd5b87bcb632b191d68d7b8f5e4cec98aa83992d5f861fd992`

Raw build/device logs remain local under `local/native_os/`; selected sanitized evidence accompanies this report. Binaries, keys and unsanitized logs are not source-controlled. Use the merged image only at address 0 for first installation; it is not the OTA payload. Existing devices should use the application image through the update service.

## Trust and next steps

Native code is privileged machine code, not a Lua sandbox. Only the built-in OS publisher is accepted; newly registered Lua publishers do not gain Native authority. All ELF lengths, load segments, entry, relocations and undefined symbols are checked before calling the upstream loader. Host callback permissions and quotas are checked, but a malicious privileged module can bypass cooperative rules; no untrusted Native execution is claimed.

Next: extend the versioned host table for generic asynchronous networking/audio only after lifecycle/cancellation and resource limits have independent tests. Keep XiaoZhi protocol/authentication in the application. Add physical display/input acceptance and longer mixed Lua/Native soak tests before widening publication policy. See [Native developer specification](../developer/native-apps.md).

The first static run exposed an existing simulator WebSocket mock missing a trailing NUL after short text messages, causing stale hello detection and intermittent XiaoZhi audio-error failures (15/20 passed before correction). Both outbound echo and hello-response copies now terminate explicitly; the existing regression was repeated 100 times after correction. This host-only fix does not change the tested firmware.

The tested application image is staged in `build/system-updates/local-update/` as version 1.0.34, build 2026102901. The local HTTP service on port 8765 returned the manifest and image with matching length and SHA-256. Selected sanitized test results (`integration-results.txt`; local diagnostics, not published).
