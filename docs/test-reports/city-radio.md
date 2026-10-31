[简体中文](city-radio.zh_CN.md)

# Native City Radio acceptance checkpoint

App 0.1.0, OS 1.0.51, ESP32-C3/8 MB/no PSRAM. [Usage and API](../developer/city-radio.md). Logs and broadcast samples stay under ignored `local/city-radio/`; no broadcast recording or credential is included in the app.

## Tests and limits

- Native application host tests: PASS, including the old-host size guard, offline/retry limit, pause/resume, volume, search, menus, ten-item deduplicated history, corrupted storage, restart restoration, sleep expiry, styles, clock and English/Chinese UI.
- MP3 parser/decoder: PASS, 100 independent decoder cycles with synthetic 16 kHz sine audio and chunk lengths 1–100 bytes; bounded ID3, garbage and rejected sink checks. AddressSanitizer/UndefinedBehaviorSanitizer used. Additional nine live samples decoded on macOS: provider IDs 339, 270, 274, 4848, 1270, 1271, 1133, 1163, 4237 (16–21 frames each). This confirms format/decoding, not station identity or C3 throughput.
- OS media service host test: PASS, 100 start/play/stop/close cycles with actual decoder and mocked network/BSP; overlapping start and capture-owner rejection, PCM failure and EOF cleanup. macOS `leaks --atExit` reports **0 leaks / 0 leaked bytes** after the test. This is host memory recovery, not board heap recovery.
- Network worker tests: PASS, existing HTTP/OTA/WS regressions plus playback permission, unbounded EOF framing, successful relative redirect, unauthorized destination rejection, sink failure and error response. TLS/DNS policy is preserved. A streaming job copies domains/URL and retains only OS-owned callbacks/context.
- Font tests: PASS, all non-ASCII Native City Radio labels are covered; the shared font adds the missing Shenzhen glyph. Previous glyph inventories remain covered. Actual panel rendering is pending.
- Live-source availability varies: initial probes received HTTP 200/audio-mpeg; a later concurrent pass had timeouts on several sources. The UI bounds retries and permits manual retry/another station. Continuous availability and named program contents are unverified.

## Initial OS 1.0.51 resources

| Item | Result |
|---|---|
| Native ELF executable span / file / signed package | 10,024 / 12,116 / 13,029 bytes |
| C3 decoder allocation | 22,928 bytes, heap-owned state/scratch, no per-frame allocations |
| MP3 assembler / PCM output | 2,060 bytes / 4,608 bytes, contained in media session |
| C3 media session / session + decoder | 6,696 / 29,624 bytes (ELF DWARF layout, not peak runtime RAM) |
| macOS media-session allocation | 6,712 bytes; ABI-dependent, not a C3 measurement |
| HTTP worker stack | 8,192 bytes, increased by 2,048 bytes; no additional media RTOS task |
| Input/network limits | 2,048-byte frame buffer, 512-byte transport blocks; streamed, never accumulated as a complete recording |
| Decoder C3 compiler stack usage | `mp3dec_decode_frame` 384 bytes; wrapper 80 bytes, individual-function static usage, not a measured maximum call chain |
| Device RAM peak / heap recovery / stack margin | NOT RUN: no USB serial device present |

The ordinary ignored root `sdkconfig` uses Debug optimization and produced an oversized intermediate image. Delivery uses the isolated `sdkconfig.defaults` Size build; the user's root configuration and existing partition layout are preserved. No oversized intermediate image is published. The official codec prototype and subsequent intermediate builds are superseded by the final archived build.

## Remaining device acceptance

Update OS first, then install the app. Verify audible live playback and correct speed/pitch on C3, long-run decoder timing, physical navigation, glyphs, styles, system volume, clock wake, station switching, pause/exit resource recovery, sleep timer, Wi-Fi loss/reconnect and alarm interruption. Collect `metrics` before/while/after 10 sessions and the `passport_media` close logs (sampled free-heap minimum, post-free heap, HTTP task stack margin). The sampled minimum excludes unobserved TLS peaks; do not label it the complete RAM peak. Preserve phone pairing, Wi-Fi and existing app data.

Build: PASS (complete gate, including firmware and merged-image validation). Host tests: PASS. Device tests: NOT RUN. Unverified: the listed on-board checks. No USB flash, full-chip erase, Git commit or push was performed.

## Published artifacts

OS application: **2,443,168 bytes**, below the 2,500,000-byte target; merged first-install image: **2,574,240 bytes**, including offset padding/boot components. Existing OTA partitions remain 2,621,440 bytes each. Native pool occupancy is 10,024 of 16,384 bytes; that reserved arena is reused/cleared on unload, not returned to normal heap.

Signed OS 1.0.51 build `2026103001` is published at the local OTA service (port 8765). City Radio 0.1.0 is published at the local App Store (port 8040), preserving Bluetooth 0.1.4 and both XiaoZhi entries. Both servers were restarted after finding them stopped. HTTP download sizes/hashes and bilingual release notes were verified. Nothing was installed on the disconnected board.

- Firmware SHA-256: `408138817c358a3c941aba054c196ebca7ad6f471671feee70aed31e840a9c9d`.
- Package SHA-256: `61f513d8a9e5bff56cc5d59baf83fe53dc9a57e346e388f9576af5c514c515ab`.
- Debug archive ID: `a6d6a7ccdedbe31601d19b82bb2b19e0a019eb937a6f54839d827535ee209e69` under `build/firmware/`.

Final logs: `local/city-radio/gate.log`, `static-final.log`, `leaks.log`, `live-decode.log`, `http-verify.json`. A final audio ownership guard blocks Opus/capture until the media worker releases output; stop during a PCM write is tested without overwriting Stopping with Playing. Alarm notification is deferred if a cancelled network worker is still cleaning up, then started after release. Device interruption timing remains unverified.

## Stutter investigation and device follow-up

The user connected the board over USB-C and authorized direct operation. Tests use `/dev/cu.usbmodem101`, Settings OTA, and the existing City Radio 0.1.0 package. No USB Flash write, erase, pairing reset or app-data removal is used. The app API/package does not change.

- Before the fix: Shanghai News produced 90 MP3 frames (about 2.35 seconds at 44.1 kHz) across approximately 16 seconds, with TLS `-0x7100` read failures. Network, scalar floating-point decode and PCM writes shared one worker, so delayed decode/read operations exhausted the short DMA window. The configured 8 KiB TLS record limit also cannot accept arbitrary standard 16 KiB records.
- OS 1.0.52: integer decode plus a separate playback worker, 4 KiB compressed queue and 16 KiB TLS input. User reported substantially improved, mostly continuous audio with occasional small pauses. Device measurement: 1,703 frames, 44,485 ms audio / 45,177 ms elapsed, average decode 5,562 us, max 10,248 us, four starvation events, max feed gap 316,003 us.
- OS 1.0.53 experimental 12 KiB queue: rejected as final because dynamic TLS RX allocation failed (15,772 bytes, then a subsequent failed read). Logs remain private. Buffer growth alone did not solve heap fragmentation.
- OS 1.0.54: 8 KiB queue with per-media `HTTP_TLS_DYN_BUF_RX_STATIC`, retaining a full receive record after handshake before lazy codec allocations. A 92-second run decoded 3,523 frames: 92,027 ms audio / 92,047 ms elapsed, average 5,465 us, max 10,557 us, **zero starvation**, max feed gap 11,314 us, no TLS/allocator failure in that run. Device free-heap minimum was 6,392 bytes; playback/network stack margins 3,304/1,784 bytes. Exit recovered 107,860 bytes with Native pool usage zero. Subsequent rapid reconnect testing had transient connection timeouts and some starvation; it did not complete ten successful cycles.

OS 1.0.55 additionally requests `WIFI_PS_NONE` only for the media network job and restores the previous mode on every completion/error/cancel path. The board log advertises `bi=102400 us`, `li=3` (307.2 ms); actual wake behavior also depends on DTIM and power-save policy; this is a supported contributor to jitter, not proof that every network timeout is caused by power saving. Full prebuffering is now 16 chunks / 8 KiB (about 1.024 seconds at 64 kbit/s). Stop during an empty-queue wait no longer counts as starvation. Host tests verify RX retention only for media, ordinary HTTP unchanged, and balanced Wi-Fi mode acquisition/restoration.

## 2026-10-30 follow-up: OS 1.0.58 and short-read correction

Earlier entries are historical checkpoints, not current acceptance. OS 1.0.56's fixed 32 KiB experiment hit Wi-Fi receive allocation failures; 1.0.57 reduced buffering but still starved. OS 1.0.58 adds output-only BSP initialization, a 4 KiB playback stack, 1,536-byte frame assembler and adaptive growth after codec/DAC setup.

- Standard stream, 1.0.58: 3,441 frames, 89,885 ms audio / 89,913 ms elapsed, zero starvation, max feed gap 11,898 us, average decode 4,932 us. Actual pool 59 blocks, sampled heap minimum 9,744 bytes; global free-heap minimum 7,676 bytes. Playback/network stack margins 1,740/2,440 bytes.
- Smooth stream, app 0.1.2 on 1.0.58: 3,117 frames, 81,422 ms audio / 88,351 ms elapsed, one starvation and 6,918,716 us gap. Pool 63 blocks; sampled minimum 7,460 bytes, global minimum 3,332 bytes; exit heap 107,584 bytes and Native pool zero. This run does **not** establish continuous playback.
- Independent Mac receive probe: same Beijing Music `/24k.mp3` source, 258,048 bytes over 48.671 seconds. Repeated receive gaps 5.23–5.64 seconds; source measured 32 kbit/s. This establishes burst delivery outside the device, not the cause of every local stall.

OS 1.0.59 packs short reads into full 512-byte blocks before queuing, flushing the partial tail before EOF. Previously every short read consumed an entire pool block, so block count did not guarantee byte capacity. A one-byte-fragment regression checks packing and EOF delivery. Full firmware gate and on-board long-run results follow below when completed. Raw private logs: `local/city-radio/stutter/{play58,play012,mac-probe-result,gate59}.log`. No chip erase, pairing reset or USB firmware flashing.

### OS 1.0.59 device result

Settings OTA installed 1.0.59 and validated the slot (state 2). A 180-second playback observation with app 0.1.2 decoded 6,686 frames: 174,651 ms audio / 179,904 ms elapsed, one starvation and a 5,235,086 us gap. Average/max decode 4,578/9,764 us; 60 packed blocks; codec allocation 28,480 bytes; sampled heap minimum 7,376 bytes, global minimum 7,044 bytes. Playback/network stack margins 1,760/2,568 bytes. After exit and delayed network cleanup, free heap recovered to 107,608 bytes and the Native pool was zero. No allocator failure or reboot was observed. This is an improvement checkpoint, **not** zero-stall acceptance; longer source/network gaps can still exceed buffering. User audible acceptance remains pending. Firmware is 2,486,032 bytes, complete gate PASS; archive `build/firmware/5990c195411dfebe1384422bd3114367f7423bc2bbb57a13a1a03f23beb8a6da/`.
