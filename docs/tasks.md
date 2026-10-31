[简体中文](tasks.zh_CN.md)

# Passport OS execution checkpoint

Updated 2026-10-25. Branch `feature/passport-os`, official BSP base
`006c2279852e352a8433b2c21e5e09be2ca36f77`. BSP unchanged; IDF5.5.3/LVGL9.5.0.
User authorizes development, commits, USB flashing and local networking. No whole-chip erase or eFuse changes.

| Stage | Verified progress | Remaining acceptance |
|---|---|---|
| M0 | Official baseline build 1,527,616 bytes; C3/8MB audited; full original Flash backed up | None for audit |
| M1 | Own launcher, lifecycle, Wi-Fi reconnect/reboot | PASS user-confirmed main screen + three buttons |
| M2 | Signed install, publisher enrollment/ownership, separate code/data, atomic recovery | PASS automatic LRU/data hash |
| M3 | Real HTTPS weather, WS text/binary, allowlisted redirects; denied endpoints/loops/oversize | Public WSS endpoint and prolonged outage |
| M4 | Bounded Lua, five apps, 500 actual sessions; Tetris approximately 18.2Hz | Physical play/visual QA |
| M5 | Consent, real RMS capture, exit worker cleanup | Acoustic calibration across environments |
| M6 | SDK/protocols/simulator and dual-slot OTA accept/reject/interrupted-pending rollback | Signed updater, host release tests, and LAN device OTA acceptance PASS; power-loss rollback remains |
| I0–I4 | Registry, compiler, streamed signed PAP2, native LVGL loader integrated | 1,767,840 bytes final app |
| I5 | 500 scrolls and 500 actual load/render/exit; native alpha/pixel hashes match host | Final 100 scroll/render cycles PASS |
| I6 | Offload/reinstall, staging recovery, reboot and USB interruption PASS | Release bundle + recovery guide generated |

Measured: grid cache peak 14,880 bytes during scrolling, zero after exit; 500 rendered
sessions drift -136 bytes (±512 tolerance), free heap 82,144, minimum 27,624, largest block
36,864. Decoder scratch I4 300 bytes/A1 684 bytes; no full RGBA buffer.
See `docs/test-reports/` and ignored `local/*result.txt` / `local/hardware-*.log`.

## XiaoZhi MiniApp and generic WebSocket audio follow-up

| Work item | Verified progress | Remaining acceptance |
|---|---|---|
| Generic OS API | App-scoped HTTPS custom headers/body, allowlisted WSS with headers, bounded JSON and public device identity exposed to Lua | Device protocol/TLS interoperability |
| Generic audio | 16 kHz mono 60 ms Opus capture/playback, bounded queues, consent and app-exit teardown; ESP-IDF codec pinned | Physical audio round-trip, task stack and heap recovery measurements |
| XiaoZhi MiniApp | v0.2.3 adds retry for transient OS network worker contention; bilingual fullscreen UI with app-owned palette | Verify service setup/retry on device; activate against official service and verify audio |
| Build/tests/resources | Static validation PASS; runtime 24/24; signed PAP validates; OS 1.0.20 image 2,280,576 bytes, 13.0% OTA partition free; host Lua peak 64,105 bytes dark / 64,177 light | On-device live heap peak and post-exit recovery |

Evidence: `docs/developer/xiaozhi-miniapp.md` and `docs/test-reports/xiaozhi-miniapp.md` (Chinese pairs available). The local development App Store now contains only XiaoZhi AI. No hardware flash was performed for this work item.

## Planned: GitHub collaboration and static releases after OS stabilization

**Status: not started; do not open the repository or enable public publishing yet.**
First complete the remaining Passport OS implementation and device acceptance gates,
then review repository contents for credentials, private keys, device logs, and other
private material before making the source public.

- Use GitHub PRs and Actions for community collaboration. Keep OS source/build/release
  governance separate from the MiniApp catalog, either in separate repositories or in
  separately protected release areas.
- For OS releases, CI builds and validates the firmware; a maintainer-approved release
  job signs the update manifest and publishes the app image plus `latest.json` over
  HTTPS static hosting (GitHub Pages or equivalent). Keep the firmware signing key out
  of source and unavailable to untrusted pull-request workflows.
- For MiniApps, accept source/Manifest/package contributions through PRs. CI validates
  manifests, permissions, package format, IDs and versions, then generates the static
  catalog index and deploys it with versioned `.pap` packages and icons. Do not make
  the index a hand-maintained list.
- MiniApp backends remain the responsibility of each app developer. Pages serves only
  static catalog/package files; app network services are independently hosted and
  declared/permissioned by the app.
- Before implementation, finalize publisher trust/signature policy for community
  packages, catalog schema and compatibility rules, review/moderation ownership,
  rollback/deprecation behavior, and protected deployment environments.
- Acceptance: a clean public checkout can reproduce the OS build and host tests; an
  approved OS release is discoverable and signature-verifiable by device OTA; a
  contributed MiniApp PR produces a validated package and generated index entry; the
  device can browse/download/install it from static HTTPS; invalid or untrusted content
  is rejected; no backend is required for catalog distribution.

## Resume safely

1. Read this file, `AGENTS.md`, specs and developer docs. Check Git and serial owners.
2. Activate `~/esp/esp-idf-v5.5.3/export.sh`. Run `tools/passport/test.sh`, then `tools/validate.sh`.
3. Only after successful build, flash the verified application to active OTA0 at `0x20000`.
   Serial `/dev/cu.usbmodem101` must have one owner. Do not flash initialization images again.
4. Run `tests/passport/hardware_cache.py`, `hardware_faults.py`, and asset regression.
5. Finish bilingual report/release bundle and archive verification; stop own local services.

Partitions: OTA0 `0x20000/0x280000`, OTA1 `0x2a0000/0x280000`, SPIFFS packages
`0x520000/0x260000`, appdata NVS `0x780000/0x80000`. Current board already initialized.
Original private backup `local/original-flash.bin` + checksum and chunk backups must remain private.
Wi-Fi and signing keys live only under ignored `local/`; never copy them into a release.
Development services on ports 8040/8041/8042 were started by this session; stop them at delivery.
Main screen and physical buttons were confirmed by the user. App-specific display checks and browser visual QA remain pending. Do not call all V1 acceptance complete.

Final deliverable: `build/release/`, verified archive under `build/firmware/55fd19bd2c3da64fa84a622a50688f43a986b3e0b493b88f8bebc97a196c8f73/`. Final app SHA256 `86fb164ec07e8e83f8c18ebd7b06ec538f735677272ff8123bf07456329fb67e`. Final 100 rendered cycles PASS, drift -112 bytes; automatic LRU/fault isolation/Store install PASS. Development servers have been stopped. See `developer/firmware-release.md`.

## Current launcher revision

The user requested a graphic Home after confirming the former text launcher. Home now uses
four native icon/text cards; see `developer/launcher-ui.md`. Complete build/host gate PASS;
flashed app 1,768,784 bytes (944 bytes more), SHA256
`1fa05a79d35dd30097d049b9fcc51d2cad845ada17a7e27664dff9f03a21754b`.
Current archive: `build/firmware/b20e17cdf2794c0192d91586361c409dfdd111496b2e63d78593ca2bf5dfe9fc/`.
This supersedes the previous release paths/hashes above; the prior stress/OTA measurements remain
valid evidence for their recorded build, not a claim they were rerun on the UI-only revision.

## App Store service recovery

After the user reported Store unreachable, confirmed port 8040 was not listening. Restarted catalog server on 8040 and WS echo on 8041, verified signed index contains seven apps, re-approved `192.168.1.10:8040/8041` for this boot, fetched/rendered the catalog on the device and installed `org.passport.appstore-grid`. The final online acceptance also fetched the catalog and installed Tetris; the device has returned to Home. Keep both server processes running while the user browses the Store.

## D0–D6 display layout, card lists, and phone Wi-Fi setup (2026-10-25)

- Standard/fullscreen viewport manager, clipping host, Manifest validation, Lua `passport.display.get_viewport()`, declarative bindings and UP/DOWN/OK navigation implemented. Apps and Store lists use 2×2 icon/title cards; footer hints shortened to fit the 224px strip.
- `./tools/validate.sh` PASS with ESP-IDF 5.5.3; host Wi-Fi form parser and existing C/Python suites PASS.
- Real Passport matrix: Weather 240×272 standard, Chat 240×272 standard, Tetris and VoiceBird 240×320 fullscreen. VoiceBird microphone capture started. 200 alternating Tetris/counter render/exit cycles PASS in 52.58s. Heap changed 75,476→75,344 bytes; image handles returned to 0 and mode to standard after every exit.
- New Settings → phone provisioning starts a temporary WPA2 AP with per-session random key; local form at `http://192.168.4.1`, bounded decoder, NVS save, AP shutdown after IP. Settings screen and AP start were checked on the flashed device; parser host cases PASS. Phone association and HTTP form POST remain unverified.
- Last validated app image: 1,798,288 bytes; merged image 1,929,360 bytes; archive `build/firmware/25188f317d2fd1827dc62363d466f3bec2d715731709d4b03bd405268236a54a/`. Flashed app-only at `0x20000` to preserve NVS and package data.
- Reflash initially reset NVS; the user restored Wi-Fi with the host `passport wifi` command. Final device checks now confirm fresh Weather data, WS text echo, seven-item App Store catalog fetch, and App Store download/install/launch of Tetris. Local Store/WS services on ports 8040/8041 remain running.
- A repeated VoiceBird run exposed a missing codec wake between sessions. `ps_audio_start` now calls `bsp_audio_wake`; the final firmware captured 209 frames over 4.22s after a previous session had already put the codec to sleep.
- Tetris board rendering now scans occupied row spans to stay inside the Lua callback budget. Final Store package hash `4bec4c5c717cce2417c434f34aceb91a2de1c1c443261362289a904e78e75bef`.
- Full metrics and limits: `docs/test-reports/display-layout.md` and paired Chinese report. Human visual review of new cards/Settings and physical long-OK timing remain open.

## System date and time (2026-10-25)

- The existing Wi-Fi got-IP handler starts SNTP against the public `pool.ntp.org`; this update adds the default `Asia/Shanghai` (UTC+08:00) timezone, an `HH:MM` status-bar clock, and Settings → Date & Time details.
- Before first sync, the status bar shows `--:--` and details show the wait state. The system clock continues during Wi-Fi outages in the same boot; after a power-loss reboot it waits for network sync again. This clock is the base for future alarms; apps should defer scheduling after reboot until system time is valid.
- `./tools/validate.sh --static` and `./tools/validate.sh` PASS with ESP-IDF 5.5.3. App image 1,827,520 bytes (OTA partition 2,621,440 bytes, 30% free); merged image 1,958,592 bytes. Archive: `build/firmware/80ba98ee0212a8cab76a74f2a2dd658e5af6bd46e5ec03e4911d3413fa981604/`; full-image SHA-256 `80ba98ee0212a8cab76a74f2a2dd658e5af6bd46e5ec03e4911d3413fa981604`; ELF SHA-256 `b3ab2954895b179004d4912a36ada27d45b876576be0b1171cdef00532da1223`. The clock adds one LVGL status label, with no new task or network buffer; heap delta was not measured separately. No flash was performed; on-device NTP sync/timezone/status rendering remain unverified.

## Signed dual-slot system updater (2026-10-25)

## System alarm (2026-10-25)

## Settings scrolling and missing alarm glyph (2026-10-25)

## Alarm settings clarity and launcher cleanup (2026-10-25)

- Alarm details now show the active field and visibly bracket the selected hour, minute, or enable state. If the alarm is disabled, opening its details selects Enabled first so UP/DOWN immediately turns it on; when enabled, editing starts at Hour. The screen spells out short OK field cycling and the 1.5 second auto-save delay in English and Simplified Chinese.
- Removed the user-facing Icon Library card, Settings row, and icon browser. The shared System Icon Registry remains available to system UI and MiniApps.
- Full `PASSPORT_OS_VERSION=1.0.16 ./tools/validate.sh` PASS: repository checks, all host suites, ESP-IDF 5.5.3 build, archive and merged-image validation. App image 2,067,840 / 2,621,440 bytes (553,600 free, 21.12%); merged image 2,198,912 bytes. No device was connected, so no device RAM or physical interaction measurements were taken.
- Signed release 1.0.16 / build 2026102519 is served by the existing Mac updater at `http://192.168.1.10:8765/latest.json`. HTTP manifest and image fetch PASS; the downloaded 2,067,840-byte image SHA-256 matches `82bcd67fbeee4858dd69f92d31afed6870d7cca8478704a0bb2c764b0ea81fd6`.
- User-facing alarm flow and launcher documentation updated in both languages. The hardware should be updated through Settings → System Update; no USB flashing was performed.

- Settings first-level menu now uses an eight-row viewport with UP/DOWN navigation and a right-side scrollbar. Host-tested viewport positioning keeps the current entry visible at list boundaries.
- Fixed the missing alarm glyph by extending the Source Han Sans subset from 132 to 133 code points (U+95F9). The generated font includes U+6EDA and the built-in LVGL CJK fallback covers U+52A8; font coverage tests check both paths.
- Full `PASSPORT_OS_VERSION=1.0.15 ./tools/validate.sh` gate passes. App image 2,067,776 bytes with 553,664 bytes (21.12%) free; merged image 2,198,848 bytes. Linked font data 16,932 Flash bytes; generated C source 113,325 bytes; no page-local heap font allocation.
- Signed release 1.0.15 / build 2026102518 is published to the local updater. HTTP image fetch, signature and digest verification pass. Hardware was not connected per user direction; physical scrolling and glyph rendering remain pending. Report: `docs/test-reports/settings-scroll.md`.

- Added a system-owned daily alarm at Settings → Alarm. Users adjust hour/minute and enable state; values persist under `osconfig`, flushed 1.5 seconds after the last edit. Default: disabled at 07:00.
- Reuses the synchronized Asia/Shanghai system clock. No alarm is scheduled while time is invalid; it fires at most once per local calendar day. On trigger, Passport OS exits the foreground MiniApp, turns on the display, and plays a repeating BSP codec/speaker tone. Any key stops the alert and returns Home. No MiniApp alarm API is exposed.
- Added a host-testable clock matcher/field adjustment module and tests. Full `PASSPORT_OS_VERSION=1.0.14 ./tools/validate.sh` gate passes. App image is 2,067,328 / 2,621,440 bytes (554,112 bytes, about 21.1% free); merged initial-flash image is 2,198,400 bytes. Bundle: `build/firmware/a8af1b4675f2264cc21c1824ce04595746c514b0f026e8230de6d1d359699a6c/`; merged-image SHA-256 `a8af1b4675f2264cc21c1824ce04595746c514b0f026e8230de6d1d359699a6c`, app SHA-256 `515dccadc899adcd9f09952ad131f5b1fce013bd958347c71dc28bad01db93f0`.
- Signed release `1.0.14` / build `2026102517` is published at the running Mac update service `http://192.168.1.10:8765/latest.json`. Manifest signature validates; fetched 2,067,328-byte image hash matches the archive. No device was connected/flashed; Settings interaction, scheduled firing, audio, peak RAM, and codec recovery after dismissal remain for device-side acceptance. Wake-from-deep-sleep is not promised.
- Paired specifications: `docs/developer/system-alarm.md`; build/release/resource and pending acceptance details are in `docs/test-reports/system-alarm.md` and its Simplified Chinese version.

- About now reads the firmware semantic version from the ESP app descriptor. Release builds set `PASSPORT_OS_VERSION` to match the signed manifest. Version 1.0.7 / build 2026102509 is signed and served locally; host build, tests, manifest and image verification pass. User confirms both the system-update OTA flow and the About firmware version display are verified on-device. App image 1,840,384 / 2,621,440 bytes (781,056 bytes free, 29.79%); merged image 1,971,456 bytes; no extra persistent RAM.

- Added Settings → System Update with separate check/download/install confirmations, byte progress, signed `passport.firmware/1` manifest validation, built-in RSA-2048 key verification, exact-size streamed download, SHA-256 and ESP image checks before selecting the inactive slot. Startup confirms the candidate only after the existing OS/storage self-test; rollback is provided by the enabled ESP-IDF app rollback support. Package and app-data partitions are not written by the updater.
- Added a USB developer command to configure/clear a trusted HTTPS manifest URL and the `passport firmware-release` SDK command to sign/publish `latest.json` plus the app-only image. First OS installation remains USB flashing; this service supports later upgrades only.
- `./tools/validate.sh --static` and the full isolated `./tools/validate.sh` PASS with ESP-IDF 5.5.3. Host update-tool tests PASS (3 cases: signed release/tamper rejection, duplicate fields, and filename/size bounds). The SDK `firmware-release` CLI also signed the archived app image and verified the resulting manifest/image round trip.
- Verified archive: `build/firmware/14a8e835a508436325c77238e26052fae9181e163bd7d73bbbad1d75bf2ecfd2/`. App image 1,838,960 bytes of 2,621,440 (782,480 bytes / 29.85% free); merged initial-flash image 1,970,032 bytes. App SHA-256 `90d9ae429c47f8b17e82ba81d342299daf6a912e1142c194ccbef1f86c4834ac`; merged image and archive SHA-256 `14a8e835a508436325c77238e26052fae9181e163bd7d73bbbad1d75bf2ecfd2`; ELF SHA-256 `e2ecf0de9eb6c855d239e5b354239715c98c92fdfe9ba88cb5e99ffd669e8069`.
- Local macOS release server and Cloudflare Quick Tunnel are running. The remote HTTPS manifest and 1,838,960-byte image both returned successfully; signed release is `1.0.0` / build `2026102502`. Device `/dev/cu.usbmodem101` was given the verified app image at `0x20000` (hash verification passed; NVS/packages/appdata offsets untouched), and the HTTPS update source was configured successfully. Physical Settings-driven OTA check/download/install and post-boot data/rollback checks are in progress. Peak RAM remains unmeasured; stream block is 512 bytes plus the existing TLS/network stack.
- Live hardware follow-up (2026-10-25): the device passed update-source configuration and signed manifest check, but downloads stopped with HTTP -1 at 19,144 / 1,839,184 bytes, then 220,147 / 1,839,200 bytes. The Mac origin served the entire file both times. Added byte/status diagnostics; raising stream socket timeout from 3 to 15 seconds advanced the second transfer, so it is now 60 seconds. Total transfer deadline remains 15 minutes. The 60-second build is under validation; hardware retry and post-boot data/rollback checks remain pending.
- Final direct-LAN acceptance (2026-10-25): release `1.0.5` / build `2026102507` was signed and served by this Mac at `http://192.168.1.10:8765/latest.json`. Device checked the manifest, downloaded the full 1,840,224-byte image, installed into OTA1, rebooted, and logged `OTA self-test valid rc=0`; user confirmed success. Server observed HTTP 200 for manifest and image. This follows Quick Tunnel mbedTLS read error `-0x7100` at 223,312 bytes; direct LAN completed the transfer. Initial recovery/bootstrap wrote only the app at `0x20000`; OTA itself wrote the inactive slot and does not touch packages/appdata. Boot Wi-Fi metrics: free 86,728 bytes, minimum 86,672, largest block 77,824; OTA-time RAM peak was not captured. Power-loss rollback injection and exact app-private data comparison remain unverified. Local signed update server remains running. Private-LAN HTTP is development-only; signature, size, hash, and ESP image checks remain enforced.

## MiniApp developer standards and tooling audit (2026-10-25)

- Added source/package Manifest, declarative UI/1, and Store catalog v1 machine-readable schemas with paired developer guides and a command reference. Manifest validation now covers real authoring and compiled PAP2 asset forms.
- `passport pack` / `passport validate` preflight declarative UI nodes/actions, parser/resource bounds, declared asset references, manifest permissions and HTTPS datasource allowlists. Removed the no-op `navigate` action from SDK and device runtime.
- `passport publish-index` accepts multiple publisher keys, verifies every package, rejects duplicate IDs, and enforces the current device catalog limit (8). Catalog pagination, Lua execution/lint in `passport validate`, hardware Store acceptance, and community key governance remain open; report: `docs/test-reports/miniapp-sdk-audit.md`.
- `passport serve` defaults to localhost; LAN binding is explicit. MiniApp host tests are now part of `./tools/validate.sh --static`.
- Static gate PASS, including 12 package tests, 5 declarative tests, and full `tools/passport/test.sh`. Firmware build/layout PASS (ESP-IDF 5.5.3); app image 2,067,776 / 2,621,440 bytes (553,664 free, 21.12%); merged image 2,198,848 bytes. No device was connected and no device RAM was measured.

### System update publication

Published the verified XiaoZhi build as version `1.0.17` / build `2026102520` to the already-running Mac update service (`http://192.168.1.10:8765/latest.json`). App image: 2,278,640 bytes; SHA-256 `799c6daacfa47a5197d4ed9422052b76783fff2e06294a1076af186d8bbffe8e`. The live endpoint served the manifest and complete image; signature and hash validation passed. Device installation was not initiated. See `docs/developer/system-updates.md`.

### Local MiniApp catalog publication

Diagnosed the empty App Store: live service on port 8040 was healthy, but `dist/index.json` had an empty `apps` array. Added only XiaoZhi AI v0.2.0 to the directory (kept the user's earlier removal of old apps), validated the PAP signature, regenerated the catalog with `passport publish-index`, and fetched both index/package from the live LAN service. Device refresh/install remains pending.

### XiaoZhi install failure diagnosis

The App Store served the signed package correctly, but the device rejected it at manifest limits (`PAP rejected: limits`). The boot ELF `1478e2d69...` matches the archived OS 1.0.16 image (`4d56f53e7d7defb77e48802e8b86190b3c8f8dff48548b0cc78437de42352bb7`); XiaoZhi requires a 64 KiB Lua heap, supported by the already-published 1.0.17/build `2026102520`. Next: install 1.0.17 via Settings → System Update, verify About, and retry XiaoZhi. No firmware flash or data erase was performed.

### OTA 1.0.17 startup rollback and fix

The user installed 1.0.17 through Settings. It booted from `ota_1`, then the `passport_http` worker hit a stack protection fault and ESP-IDF rolled back to 1.0.16. The matching ELF trace identified a 9,308-byte automatic `job` object on an 8,192-byte worker stack (captured SP 2,980 bytes below its lower bound). Version 1.0.18 moves one job allocation to the heap and queues only a pointer, with a regression test. The full `PASSPORT_OS_VERSION=1.0.18 ./tools/validate.sh` gate passed. App image: 2,278,416 bytes, leaving 343,024 bytes in the OTA slot; merged image SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`. Signed version `1.0.18` / build `2026102521` is live on the Mac update service, with fetched manifest and image size/hash verified. The user installed 1.0.18; serial logs confirm `ota_1`, storage self-test valid, Wi-Fi online, and no candidate-boot panic. Startup minimum free heap was 85,264 bytes and network task stack high-water mark was 3,884 bytes. The user confirmed About shows 1.0.18. See `docs/test-reports/ota-1.0.17-rollback.md`.

### App Library card stack fault and fix

On 1.0.18, opening the App Library card caused a main-task stack protection fault. The matching ELF showed a stack pointer 132 bytes below the lower bound while inside newlib formatted output. The renderer synchronously loaded and verified the installed PAP to obtain its card version, nesting package parsing under the large `draw()` stack frame. Version 1.0.19 preloads versions during list refresh and reads the cache while drawing; a source-contract regression test covers this. The full `PASSPORT_OS_VERSION=1.0.19 ./tools/validate.sh` gate passed. App image 2,278,416 bytes, OTA slot free 343,024 bytes; merged SHA-256 `c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`. Signed 1.0.19/build `2026102522` is published on the local update service and its manifest/image verified. Device retest after OTA install remains pending; see `docs/test-reports/app-library-card-crash.md`.

XiaoZhi 0.2.1 addresses the user's launch-screen feedback: the Manifest now requests fullscreen, and its Voice/Conversation/Connection pages fill the 240×320 viewport with dark backgrounds and brighter high-contrast cards. Static host validation passes (runtime 20/20); live `dist/` App Store index/package are regenerated and signature-verified. Hardware visual acceptance remains pending; see `docs/test-reports/xiaozhi-miniapp.md`.

OS 1.0.20 adds persistent Dark/Light appearance and read-only `passport.system.display_mode()`. Apps choose whether to follow the system and can set text color with `passport.text(value, rgb24)`. XiaoZhi 0.2.2 now draws its own contrasting palette based on the readable system mode. Static validation passes with runtime 24/24; signed OS and app package are published to the local update and App Store services. XiaoZhi 0.2.3 network worker contention retry is validated in the simulator and published to the local App Store; verify on-device. System appearance switching has been confirmed by the user. See `docs/test-reports/xiaozhi-miniapp.md`.

## System review and optimization 1.0.23

The architecture/source review and implemented memory, HTTP framing, audio, JSON, alarm, SDK and atomic publishing fixes are recorded in [the review](development/passport-os-review.md). Full ESP-IDF and host gates passed. OTA 1.0.23 build 2026102604 is published and its live signature/size/hash verified. Device remains at the last observed 1.0.21 until Settings installs the release; post-update resource/voice/network acceptance is pending.

- 2026-10-26: Reproduced XiaoZhi TLS RSA/MPI allocation failure over USB; published OS 1.0.24 and XiaoZhi 0.2.4 with dynamic TLS buffers, pre-connection Lua collection, bounded retries and activation feedback. Build and host tests pass; OTA device verification pending. See [report](test-reports/xiaozhi-miniapp.md).

- 2026-10-26: WSS HTTP 502 traced to IP-valued Host header after TLS verification succeeded. OS 1.0.26 preserves hostname authority and validated-IP dialing; build, host tests, live package verification pass. Device hello/audio pending OTA. See [report](test-reports/xiaozhi-miniapp.md).

- 2026-10-26: Fixed XiaoZhi audio startup failures being reported as user permission denial. Consent remains granted on stream errors; app gets `on_audio_error(1)`. Published OS 1.0.28 and XiaoZhi 0.2.7; host/device audio acceptance pending OTA. See [report](test-reports/xiaozhi-miniapp.md).
- 2026-10-26: Fixed XiaoZhi audio task creation under WSS heap fragmentation by allocating its 9 KiB task stack before stream queues; added stage/heap failure logs and allocation rollback tests. OS 1.0.29/build 2026102610 is published to the Mac OTA service. Full build and host tests pass; device PTT, codec stack high-water, RAM peak and exit reclamation remain pending OTA. See [report](test-reports/xiaozhi-miniapp.md).
- 2026-10-26: Device logs on OS 1.0.29 confirmed 9,216-byte audio stack creation failed after WSS (largest block 7,680 B); MiniApp also retried every tick while OK stayed logically held. OS 1.0.30 now reserves a non-capturing worker at app launch before networking and retains it between PTT sessions; XiaoZhi 0.2.8 clears held state after errors. Full gate passed; OS 1.0.30/build 2026102611 and MiniApp 0.2.8 published and verified. Device speech, memory peak/high-water, and teardown need retesting after OTA/Store install. See [report](test-reports/xiaozhi-miniapp.md).
- 2026-10-26: OS 1.0.31 retains the pre-reserved audio stream worker and preserves compatibility with ordinary microphone capture for stream-capable apps. Full gate and 33 runtime tests pass; build 2026102612 supersedes 1.0.30 on the OTA service. XiaoZhi 0.2.8 remains current in the Store. Real-device speech/resource acceptance remains pending installation. See [report](test-reports/xiaozhi-miniapp.md).
- 2026-10-26: Reproduced 1.0.31 WSS certificate verification failure (`0x4290`, then TLS `-0x3000`) due to audio stack reservation consuming ordinary heap. OS 1.0.32 allocates the 9 KiB worker stack from ESP32-C3 Retention RAM and uses caps-aware task deletion. Full gate and 33 runtime tests pass; signed build 2026102613 is live. Device WSS/PTT and heap recovery remain pending OTA. See [report](test-reports/xiaozhi-miniapp.md).
- 2026-10-26: User reproduced the WSS failure on 1.0.32. Retention allocations still share internal RAM with TLS capability allocations. OS 1.0.33 defers worker-stack allocation until actual capture starts; permission grants and app launch no longer reserve it. Metrics now separate Internal/Retention free and largest blocks. Full host suite and firmware build/layout passed. See [report](test-reports/xiaozhi-miniapp.md); device OTA acceptance is pending.
