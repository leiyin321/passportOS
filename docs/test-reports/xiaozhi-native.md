[简体中文](xiaozhi-native.zh_CN.md)

# Native XiaoZhi acceptance — 2026-10-29

Delivered Native XiaoZhi 1.0.4 (`org.passport.xiaozhi-native`) and Passport OS 1.0.45. The real board installed the OS through its normal Settings OTA flow; no full Flash image or data erase was used. Store index/package downloads, signature/hash and installation were exercised on the device. The Lua app retains its separate ID and data.

## Measurements

All table sizes are bytes; both firmware images remain below decimal 2.5 MB.

| Artifact | Bytes |
|---|---:|
| OTA | 2,331,440 |
| Merged | 2,462,512 |
| Native ELF | 12,420 |
| Native loaded span | 13,060 |
| Executable pool | 16,384 |
| Streaming task stack | 24,576 |
| Package | 13,404 |

Native has no private heap: code/data/BSS use the permanent pool; OS services separately own network/codec/scratch heap. The arena is cleared/reusable on exit and remains reserved. General heap counters include concurrent Wi-Fi and system caches, so they are not an isolated Native allocation count.

## Actual voice results

Official HTTPS configuration returned 200, certificate validation succeeded, WSS/hello reached Ready, press/release delivered microphone packets, server text/TTS replies were exercised, and the speaker driver consumed decoded packets. These counters prove software/driver flow, not acoustic intelligibility:

- frames=0, Opus TX=0, RX=88, unused stack=13804 bytes, max read=0 us
- frames=50, Opus TX=49, RX=0, unused stack=3304 bytes, max read=45187 us
- frames=51, Opus TX=50, RX=107, unused stack=3300 bytes, max read=45224 us

No panic, failed allocation or stalled receive queue occurred in the final runs. Native pool usage returned to zero on exit. See sanitized metrics; raw logs, transcripts, activation data and tokens remain under ignored `local/native_xiaozhi/`.

## Architecture and corrected failures

- The ELF contains the XiaoZhi configuration/authentication/hello/listen/STT/TTS state machine. OS exports generic ABI 2 network, JSON, identity, storage and consent-gated Opus services; no XiaoZhi provider is embedded in OS.
- A failed contiguous 24,524-byte encoder allocation was fixed by allocation ordering, Native half-duplex codec ownership and releasing I2S/DMA on exit. Shared I2C remains available.
- Matched-ELF stack faults in SILK and CELT established the real stack requirement. Streaming uses a 24 KiB stack and heap-owned PCM scratch. Generic low-delay 20 ms CBR encoding is joined into standard 60 ms Opus packets, preserving the wire contract. A real libopus encoder/decoder test verifies 960 samples per packet at 16 kHz.
- Espressif requires 160 bytes of output capacity for the selected internal frame despite its smaller encoded payload. The OS queries the requirement and supplies bounded 256-byte frame buffers.
- Two-message WebSocket receive queues could overflow during TTS bursts. Worker backpressure (bounded to 1 second), bounded batch delivery and deferred/coalesced drawing avoid waiting for LVGL between incoming messages. Native audio enqueue waits at most 80 ms; BSP button callbacks remain nonblocking.
- TLS still verifies the complete certificate chain. Releasing the unneeded peer-certificate copy, a 16 KiB LVGL pool, four-row DMA tile and measured HTTP/WS stack budgets recover RAM. No fonts, Lua, assertions or trust checks were removed.
- Store requests now remain exclusive until foreground verification/install finishes, preventing rapid OK from overwriting the shared temporary download. Submission failures clear pending state.

## Validation

Build: PASS — ESP-IDF 5.5.3 full gate and matching archive verification.

Host tests: PASS — Lua/declarative/package/network/audio regressions, Native parser mutation/ASan/UBSan checks, bounded non-NUL JSON, true Opus aggregation/decode, receive-backpressure/failure tests and Native bilingual protocol state-machine tests.

Device tests: PASS for the stated OTA, store, Native startup, official network/voice packet flow and exit metrics. Physical human speech recognition/acoustic quality and arbitrary remote Chinese glyph coverage remain user acceptance checks. Built-in UI label glyph coverage is tested.

Unverified: acoustic intelligibility, long-duration voice soak, power/current measurement and remote account activation on a fresh unbound device. User-controlled microphone consent remains mandatory.

## Artifact identity and resume

Archive: `build/firmware/529fc4652bdf03357206f07a9b0ebbd2e1c0188700a223b3ad02f55bd677c720`.

- OTA SHA-256: `fd5c16c32c4cce3e3bade457a038d3cba1151dc3c6cb9d6ce76717c9b4b9fb41`
- Merged SHA-256: `529fc4652bdf03357206f07a9b0ebbd2e1c0188700a223b3ad02f55bd677c720`
- Matching ELF SHA-256: `c304c627ecc229d90390aa4fed9dc3916cf0898027599e58aad7c42b9d69f1d0`
- App package SHA-256: `5820c20afbddc406e50cd20ea4c41faeff08e05f2c53bdae5426dbc8dca9b686`

The local update/App Store services use ports 8765/8040. Public app source and SDK workflow: [Native XiaoZhi](../developer/xiaozhi-native.md), [ABI 2](../developer/native-services.md). Check those versions, the archive and sanitized counters before resuming. Existing Lua/ABI 1 applications and persistent data must be preserved.

## Fonts, latency and memory

The actual app-body font probe reported checked/missing counts: voice: 59/0, greeting: 105/0, repeat: 78/0. All were zero-missing while the system UI was Chinese; host coverage checks additionally verify language-independent widget assignment and the complete GB2312 level-1 inventory. The 2,919-glyph supplement adds 202,625 Flash bytes, with no heap-owned bitmap. Rare CJK outside the supported inventory and emoji remain unsupported unless an app supplies suitable assets. A descriptor probe does not replace visual confirmation of clipping/baselines.

Post-boot free 8-bit heap was 109,640 bytes; the measured session low-water mark was 8,604 bytes, a 101,036-byte decrease relative to idle. This includes OS network/audio allocations and Wi-Fi activity, rather than an isolated app heap. Native executable-pool peak was 13,060 bytes and usage returned to zero. Exit (free/largest block, bytes): [('voice', 103840, 77824), ('greeting', 103312, 77824), ('repeat', 104268, 65536)].

The mixed run plays a greeting, interrupts playback with press/release capture, then exits. Historical OS 1.0.43 failed this case with Wi-Fi RX allocation warnings and audio enqueue failure. The final build reduces bounded buffers and enables the pinned SDK's official separate WebSocket TX lock. See [display latency measurements](display-performance.md) for the Home comparison, input probe limits and physical acceptance checks. User feedback confirmed audible replies on the earlier build; final physical Chinese rendering, button feel and actual spoken recognition remain human acceptance checks.

## Compatibility and lifecycle regression

On OS 1.0.45, ABI 1 Native Hello launched, handled an injected OK click and exited; Lua XiaoZhi launched and exited with its interpreter reclaimed (`lua=0` after exit). This verifies startup/lifecycle, not the Lua remote speech service. Native XiaoZhi then passed 100/100 launch/exit cycles in 8,930,072 microseconds, with a 36-byte reported heap drift and zero executable-pool usage on exit. This bounded test is not a long-duration leak proof.

## Native 1.0.5 activation guidance — 2026-11-01

Complete repository gate: PASS (host tests and ESP-IDF 5.5.3 firmware build). Address/undefined sanitizers cover activation priority, retry, no premature WebSocket/capture, four language/theme combinations, bounds and the 192-command limit. All new Chinese labels pass font coverage. The rendered 240×320 host QR was decoded with macOS Vision to exactly https://xiaozhi.me/console/. This is not a physical camera test.

The static QR uses 483 bytes of run data and 163 total draw commands. Native executable span: 14,084 bytes (previous 13,060); ELF file: 13,480 bytes; signed package: 14,464 bytes. Package SHA-256: `15e36dc60bfae24e1422620bf0d775760d056839ed1a1aa439fd4d03830e98d6`. No additional runtime QR heap is allocated. Actual board RAM peak, app-exit reclamation and physical rendering for this version are NOT RUN: USB device was absent; the temporary local preview package was not installed. Previous-version device measurements above must not be substituted.

The user confirmed real unbind/rebind with 1.0.4 before this change. The new real activation page and phone scan remain user acceptance checks. No cloud binding was changed by this implementation. OS stays 1.0.75; publish the app through Pages using the existing signed OS payload, without rebuilding or replacing its public binary.

Online publication PASS: release `apps-20261101-xiaozhi-1.0.5`, source commit `4e53ccb`, Pages run `41276029714` and GitHub static checks `41276016831`. Fresh public downloads verify XiaoZhi 1.0.5 (14,464 bytes), signature and hash. OS 1.0.75 signed manifest fields and image hash remain unchanged. Device installation is pending while USB is absent.
