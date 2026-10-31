[简体中文](passport-os-review.zh_CN.md)

# Passport OS design and implementation review

Date: 2026-10-26. Branch: `feature/passport-os`. Baseline device: OS 1.0.21,
XiaoZhi 0.2.3. This is a source review and a tested optimization increment,
not a claim that every subsystem has completed hardware acceptance.
Existing uncommitted project work is preserved. Changes ship through the existing
system update service; the Flash partition layout is unchanged.

## Architecture assessment and scope

The single foreground AppSession, signed PAP packages, bounded Lua/declarative
runtimes, system-owned hardware services, separate package/private-data storage,
and dual-slot OTA remain appropriate for an ESP32-C3 without PSRAM. Preserve
those boundaries. Application service semantics and authentication belong to the
MiniApp; the OS provides generic HTTP/WSS, JSON, audio and display primitives.

Reviewed paths: `main/passport_main.c`, `os/core`, `os/package`, `os/runtime`,
network/audio/display/storage/clock/settings/OTA services, BSP integration,
`protocol`, `sdk/passport`, simulator and relevant regression tests. Read the
pinned ESP-IDF 5.5.3 HTTP implementation to check its framing contract. Vendored
Lua/LVGL/codec internals were not independently audited in full.

## Implemented plan

| ID | Priority | Finding and change | Evidence/check |
| --- | --- | --- | --- |
| R01 | P0 | Every network operation allocated a 9,308-byte job, including unused HTTP body/header and WS header arrays. Use one compact header block, offset indexes and an exactly sized POST body. Add a target compile-time 4 KiB job ceiling and rejection logs. | Target ELF: job 3,176 B; headers 1,064 B. Header tests cover roundtrip, bounds, duplicates, reserved fields, CR/LF and NUL injection; actual worker tests run with an 8 KiB maximum allocation block. |
| R02 | P0 | Retained package source consumed memory after Lua compilation. Release it before top-level/on_start execution; release declarative source after parsing. Document consumed ownership and reject restarting a consumed package without reloading. | XiaoZhi source is 13,744 B (+ terminator). Simulator asserts source reclamation on successful startup; existing app scenarios run against the real runtime. |
| R03 | P1 | Response storage was allocated before TLS handshake; chunked fetch length 0 was incorrectly compared with received payload. Grow response storage only as data arrives; distinguish unknown length, parser completion, transport errors and quota overflow. Complete short POST writes; never follow POST redirects or consume/transfer a GET redirect body before the next hop. | HTTP framing tests cover chunked/EOF, exact quota, empty body, timeout, truncation and strict OTA length. Read timeout/size caps remain enforced. |
| R04 | P0 | RMS-only audio initialized to NO_MEM because its intentionally absent streaming buffer was NULL. Correct the mode-specific check. | New host harness executes actual `audio.c`: RMS capture, stream encoding, reply playback, allocation/task/init failures and teardown. |
| R05 | P0 | Main loop tried starting RMS mode while an Opus stream already existed; mode mismatch revoked microphone permission. Dispatch according to the active runtime audio mode. | Firmware compilation plus actual audio-service mode/stop tests. Physical PTT/TTS is still a device gate. |
| R06 | P1 | Main loop retained a 2 KiB WS receive scratch on every callback path. Isolate receive/dispatch in a non-inlined helper. | Target disassembly: fixed main frame 2,752 → 1,168 B. This is not a measurement of worst-case nested callback stack. |
| R07 | P1 | Alarm check required exactly second 0, although networking/storage may delay the main loop. Check throughout the configured minute, keeping daily deduplication. | Alarm test triggers at second 17 and rejects a duplicate on that day. A delay beyond the entire minute remains a scheduler limitation. |
| R08 | P0 | JSON limits were checked after recursive C parsing; Lua allocation failure could jump over cJSON cleanup. Preflight depth/value limits before parsing; protect Lua conversion/string creation and free temporary C allocations before propagating OOM. Reject NUL keys/values. | Real runtime tests: deep input, duplicate/NUL keys, 256-value boundary, normal provisioning; 26 runtime tests pass. |
| R09 | P1 | Catalog schema omitted audio.stream/device.identity; API metadata retained obsolete viewport/audio descriptions. Align contracts with the implementation. | Catalog/Manifest permission parity test; existing package/schema tests. |
| R10 | P1 | CLI silently succeeded without completion and hid worker logs after ACK. Add bounded `--observe`/`--verbose`, timeout/failure exit status and newline/NUL argument rejection. Implement documented pin on/off. | Fake-serial tests for timeout, failure, normal ACK and post-ACK logs; firmware compile for pin parsing. |
| R11 | P1 | Publishing wrote directly over served image/catalog/manifest paths. Atomically replace files; commit OTA manifest last; enforce increasing builds and immutable versioned firmware images. | Interrupted-publication test retains previous manifest/image; rollback/reused-version rejection; signed release verification. |

## Resource evidence

| Metric | Before | After | Measurement type |
| --- | ---: | ---: | --- |
| Network job, ESP32-C3 layout | 9,308 B | 3,176 B | Matching ELF DWARF; 6,132 B / 65.9% reduction, excluding separately allocated actual POST body |
| Header storage within job | 4,112 B pairs + 1,025 B WS copy | 1,064 B shared storage | Compiled type layout; no secrets logged |
| Retained XiaoZhi source after compilation | 13,745 B | 0 B | Source size plus NUL; allocation released before on_start |
| Preallocated custom HTTP response during handshake | 8,193 B | 0 B | Allocation lifetime in source; response grows up to limit+1 later |
| Fixed app_main stack frame | 2,752 B | 1,168 B | Target disassembly; task stack remains 8 KiB |
| Baseline live device free/largest heap | 25,140 / 8,192 B | Pending OTA acceptance | 1.0.21 serial observation while XiaoZhi waits |
| Baseline main-task minimum remaining stack | 796 B | Pending OTA acceptance | Device stack high-water measurement |

The old 9,308-byte request allocation exceeds the observed 8,192-byte largest
free block. This is a concrete allocation failure risk consistent with the
waiting state, even when total free heap is sufficient. It is not proof that
DNS, TLS, server activation, WSS and audio will all work after the memory fix.
Do not add the above savings together as a measured live RAM peak.

## Follow-up architecture work and acceptance gates

These remain explicit follow-up work, not implemented claims:

1. **P1: service cancellation and teardown acknowledgement.** `ps_net_cancel`
   waits 16 seconds while a firmware read may block for 60 seconds; audio stop
   can time out. Introduce a two-phase close state that stops dispatch, requests
   cancellation, and releases session-owned resources only after acknowledgement.
   Test cancellation during DNS/TLS/read/playback and forbid stale generation delivery.
2. **P1: whole-system memory admission.** Lua, assets and transport currently
   have separate budgets. Add capability-based admission/reservation for TLS +
   Opus + VM, and deterministic no-memory results. Measure real simultaneous
   streaming peaks; retain TLS verification. The 64 KiB Lua maximum is a quota,
   not a guarantee that every combination fits.
3. **P1: boot/update fault matrix.** Retain candidate validation and rollback;
   add power interruption tests at download, slot selection and confirmation,
   and verify long offline reads cannot race cancellation. Before public release,
   separate firmware and community publisher trust/rotation policies.
4. **P2: extract the shell.** Split the roughly 2.4k-line main module into app
   manager, settings/controllers and developer console, keeping a single event
   owner and existing LVGL locking. First freeze behavior with event-sequence
   tests. This increment isolates only WS dispatch, avoiding a simultaneous
   full shell rewrite during the network repair.
5. **P2: performance and catalog scale.** Measure callback P50/P95 and GC cost
   before replacing the per-callback full collection; cache verified package
   metadata to avoid repeated signature/source reads when listing versions.
   Add paginated catalogs before exceeding eight Store entries; keep the current
   eight-entry and twelve-installed-app limits explicit.
6. **P2: broader real-service harnesses.** The simulator mocks networking/audio.
   New audio and HTTP harnesses execute actual service code. The HTTP harness
   uses AddressSanitizer/UBSan, exercises queues, partial POST writes, chunk
   trailers, redirects, quota/transport errors, allocation failures and OTA
   sinks, and completes 500 requests with zero outstanding tracked allocations.
   RTOS/socket/TLS/BSP behavior is stubbed; real concurrency, TLS handshakes
   and whole-device resource drift still require target tests.

## Resume and device acceptance

The final build, artifact identity and OTA publication are recorded below.
After installing through Settings, check About and capture one serial connection:

```sh
PYTHONPATH=sdk python -m passport device --port /dev/cu.usbmodem1101 \
  --verbose --observe 20 launch org.passport.xiaozhi
```

Accept the existing microphone prompt on the device when testing audio. Record
HTTP queued/completion or sanitized rejection reason, activation result, WSS
handshake and hold/release/TTS behavior. Check `device metrics` before launch,
after connection, during audio and after `device home`; repeat start/exit cycles
and compare free/largest heap, task stack watermarks and asset bytes. Keep raw
identity/authentication/device logs out of committed documentation. Verify an
alarm with a delayed loop check and both system languages. These are pending
hardware checks; a successful build is not device acceptance.

## Final validation and publication

- Build: PASS. `PASSPORT_OS_VERSION=1.0.23 ./tools/validate.sh` with activated ESP-IDF 5.5.3; archive verification passed.
- Host tests: PASS. Complete gate, including 26 real-runtime scenarios, the actual audio-service harness, HTTP framing/headers, SDK serial/schema and interrupted publication checks.
- Device tests: baseline and new CLI observation PASS on the existing 1.0.21 device. **New firmware hardware behavior NOT RUN**; no USB firmware write was performed.
- Unverified: install 1.0.23 through Settings, then test XiaoZhi public service/activation/WSS/PTT/TTS, live heap/stack peaks and repeated exit recovery.

Application image: 2,283,296 B; OTA headroom: 338,144 B (12.9%); merged image:
2,414,368 B. Static D/IRAM: 180,480 B (13,716 data, 64,632 BSS, 102,132 IRAM text),
unchanged from 1.0.21. This is link-time allocation, not the device heap peak.
Host XiaoZhi dark/light Lua peaks: 64,265 / 64,177 B, no runtime errors, microphone
false after release; these are host values. The latest baseline device sample had
25,160 B free, 8,192 B largest block, 5,256 B historical minimum and 796 B main
stack remaining; the minimum includes the preceding diagnostic session.

Verified bundle: `build/firmware/c8e59c6537d2d05d7cca720bb35368c5ff6e5608b2b20654ac2b97d7a6b80c53/`.
Merged SHA-256: `c8e59c6537d2d05d7cca720bb35368c5ff6e5608b2b20654ac2b97d7a6b80c53`.
Application SHA-256: `7e6adede041783bcc37924e56201bc0c8756a335f0143aab6b86e6a065750f86`.
Matching ELF SHA-256: `78023893459cd676979458554ad5c9c053c9d7d97ed0ef9dcba41af91435a69c`.

Signed build `2026102604` is served at `http://192.168.1.10:8765/latest.json`.
The manifest and full image were fetched directly from the running LAN service
and verified using the release public key. An initial host fetch through a proxy
timed out; the direct LAN fetch passed. App Store remains XiaoZhi 0.2.3.

Final inspection added the redirect-buffer fix after 1.0.22 had been staged.
Version 1.0.23 supersedes it without overwriting the immutable 1.0.22 image.

Compared with the 1.0.21 baseline, the final application image grows by 1,952 B;
static D/IRAM is unchanged. Dynamic allocation lifetime and fragmentation improve,
but post-update device peaks remain pending.
