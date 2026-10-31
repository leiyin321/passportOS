[简体中文 — complete original task specification](PASSPORT_OS_CODEX_HANDOFF.zh_CN.md)

Latest checkpoint: OS 1.0.75 (signed build 2026110102), installed directly from public Pages OTA and boot validated. Source is published at https://github.com/leiyin321/passportOS; Pages https://leiyin321.github.io/passportOS serves signed OS updates and three Native apps. Local and GitHub Linux host/firmware gates, Pages deployment, all public payload signatures/hashes, latest-version check and forced Native Store reinstall PASS. Public OTA took 210.83 s; Bluetooth package reinstall 4.23 s. App 2,423,200 bytes, 76,800 below 2.5 MB. See [public acceptance](test-reports/public-distribution.md) and [distribution guide](../distribution/README.md). First-install 8 MiB image and SDK wheel are release assets; destructive spare-board first installation and community article publication remain outside this acceptance. Original worktree/upstream remote are retained; the isolated publication checkout is `local/open-source/public-repository`. No USB Flash write/erase; serial closed.

Store checkpoint — 2026-10-31: legacy Lua XiaoZhi (`org.passport.xiaozhi`, 0.2.8) retired from the live catalog and download directory at user request. Three entries remain: Native XiaoZhi 1.0.4, Bluetooth Peripherals 0.1.4 and City Radio 0.1.2. HTTP catalog, old-download 404 and remaining package size/hash checks PASS. Source examples, device-installed applications and persistent data are unchanged.
# Passport OS implementation handoff


## Current checkpoint — 2026-10-29

Device OS is 1.0.50, installed through Settings OTA. The local Store serves Native Bluetooth Peripherals 0.1.3, Native XiaoZhi 1.0.4 and Lua XiaoZhi 0.2.8. Later user authorization and the [Native plan](PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.md) supersede the original Lua-only restriction below. Trusted Native ABI 1/2 coexist with Lua; ABI 2 has an optional 96-byte HID host extension with an unchanged 80-byte core. Existing user data and OTA/data partitions are preserved. Persistent media/alarm volume remains available in Settings → Sounds.

The user confirmed Bluetooth saved-gesture acceptance on iPhone 12 Pro Max/iOS 27 with OS 1.0.50 and Native 0.1.3: UP/DOWN swipe the video feed and OK pauses/resumes playback. Recommend iPhone AssistiveTouch device mapping of mouse Buttons 2/3/1 to saved upward/downward gestures and Single Tap. Direct drag/wheel did not pass the earlier test and are not covered by this success. Other devices/apps, Consumer media control and long-duration reconnects remain unverified. Classic headphone audio/calls are unsupported by C3 hardware. Keep the phone awake: the current profile does not advertise remote wake and blocks reports while the host suspends it.

The latest full OS build and app host gate passed. Historical OS 1.0.48 passed ten radio lifecycle cycles plus Native voice playback/capture and ABI 1/Lua startup regression. Do not substitute those resource measurements for a new full device acceptance. Resume from the exact versions, artifacts and outstanding issues in the [Bluetooth checkpoint](test-reports/bluetooth-peripheral.md), [iPhone/SDK guide](developer/bluetooth-peripheral.md), [Native report](test-reports/xiaozhi-native.md), [display report](test-reports/display-performance.md) and [volume report](test-reports/system-volume.md). Raw logs and keys remain local; no Git commit/push has been performed.

The complete user-supplied specification is preserved verbatim in the linked Chinese edition.
This English index preserves the normative scope and points to the execution evidence.

Build on official FoloToy AI Passport main on `feature/passport-os`, ESP-IDF 5.5.3,
ESP32-C3, 8MB Flash, no PSRAM, 240x320 ST7789P3, three ADC ladder keys, ES8311.
Reuse official BSP drivers. Implement a new OS Launcher; do not reuse the hardware demo UI.

Required architecture: one foreground AppSession; session-owned opaque resources;
only system services own hardware/network; UI locking; asynchronous network/audio callbacks.
Identity, Package, Instance and Persistent Data are independent. Package eviction never deletes data.
Signed dynamic PAP applications, declarative and one bounded Lua VM; no native ELF execution.
Strict Capability API; no raw sockets, Flash, GPIO, Wi-Fi admin or unsafe Lua libraries.
HTTPS/WSS domain allowlists, approved fixed LAN HTTP/WS services, per-connect DNS/IP checks,
no loopback/link-local/unapproved private addresses, bounded connections/messages/traffic.

| Milestone | Required gate |
|---|---|
| M0 | Upstream baseline build, hardware/pins/resources audit and runtime ADR |
| M1 | Launcher/settings/STA reconnect, queues/scopes, 100 mock session cycles |
| M2 | Signed PAP loader/cache, isolated data, counter offload/reinstall persistence and denials |
| M3 | Declarative actions/bindings, HTTPS weather and authorized WS, endpoint security tests |
| M4 | Bounded Lua, generic graphics, offline Tetris, crash/timeout/OOM recovery and measured FPS |
| M5 | Authorized audio features and VoiceBird, microphone teardown and denied-permission test |
| M6 | On-device Store, CLI/simulator, all five dynamically installed apps, firmware and recovery |

Five applications: declarative public-API weather, offline persistent counter, Lua Tetris,
Lua voice-controlled flight (RMS only, no voice transmission), WS text/binary live channel
using a project-owned local server and configurable owned WSS endpoint.

Developer deliverables: versioned Manifest/PAP/Capability protocols, reproducible signed packer,
validator, local server, static catalog publisher, simulator, complete examples and test fixtures.
Device Store must browse/install/cache/run/update/offload/pin/delete with visible errors.

Each milestone: implement → host tests → integration/simulator → hardware where available →
record resources/results/pending work → update tasks → commit → continue.
Never label simulator results as hardware PASS. Record free/minimum/largest heap, task stacks,
VM heap, FPS/CPU/frame times, image size, startup P50/P95 and 500-cycle resource drift.
Cover failures/security/storage/power loss/reconnect/session teardown/VM abuse.

Use measured image sizes to choose aligned dual OTA slots plus separate cache/data.
OTA must stay disabled for unattended updates until verified signature/hash, boot self-test,
power-cut and rollback acceptance. Do not claim a safe updater from a partition table alone.
No full-chip erase, eFuse, permanent Secure Boot/Flash Encryption or unbacked vendor-data overwrite.
Back up and document recovery before destructive repartitioning; segmented flashing is preferred.
Never commit credentials, private keys or private raw logs.

Completion requires the full real-device user journey: connect Wi-Fi → Store-download weather →
public data → persistent counter → Tetris → offload/reinstall with saved progress → authorized
VoiceBird → authorized Mac WS → offline cached games → clean Launcher exit, without reflashing.
Until this journey is verified, the project remains in development.

## GitHub collaboration and static publishing (historical plan, now implemented)

Implementation: the user approved a Native-first monorepo at `leiyin321/passportOS`, local signing and manually gated Pages publication. The [distribution guide](../distribution/README.md) and [acceptance report](test-reports/public-distribution.md) supersede the historical plan below.

Original precondition: do not open the source or enable public publishing until the planned OS implementation
and device acceptance gates are complete. Before opening, audit repository history and
artifacts for credentials, private signing keys, raw device logs and other private data.

Use GitHub pull requests and Actions for both OS and MiniApp contributions. Prefer
separate OS and catalog repositories, or equivalent permissions and protected release
areas. OS CI builds/tests firmware; a maintainer-approved protected job signs the OTA
manifest and publishes it with the app image to HTTPS static hosting such as GitHub
Pages. Never expose the firmware signing key to untrusted PR workflows.

MiniApp CI validates source, Manifest, permission declarations, IDs, versions and package
format, then generates and deploys the catalog index, versioned `.pap` packages and icons.
Each app developer independently hosts any dynamic backend the app needs; Pages serves
static distribution only. Before implementation, define community publisher trust and
signing, moderation, compatibility, revocation/rollback and deployment ownership.

Acceptance: clean public checkout reproduces OS build and host tests; approved OS releases
are discoverable and verifiable by device OTA; a contributed MiniApp produces a validated
package/index entry that can be browsed, downloaded and installed over static HTTPS; bad
or untrusted content is rejected. This is post-stabilization work, not current capability.

See [execution checkpoint](tasks.md), [hardware audit](hardware-audit.md),
[package protocol](developer/package-protocol.md), and milestone reports under `test-reports/`.
