[简体中文](bluetooth-peripheral.zh_CN.md)

# Bluetooth peripheral acceptance checkpoint — 2026-10-29

Current device: Passport OS 1.0.50, OTA self-test valid; Native Bluetooth Peripherals 0.1.3 installed and published to the local App Store. Native XiaoZhi 1.0.4 and Lua XiaoZhi 0.2.8 remain listed. No full-chip erase, Git commit or push was performed.

**Saved-gesture functional acceptance: PASS, confirmed by the user on 2026-10-29.** UP/DOWN successfully swipe the short-video feed in both directions, and OK pauses/resumes playback on iPhone 12 Pro Max/iOS 27 using OS 1.0.50 and Native app 0.1.3. This validates the iPhone gesture-button mapping mode; it does not validate direct drag or wheel mode.

Historical finding: The user tested an iPhone 12 Pro Max running iOS 27 and reported no visible short-video response to drag/wheel operations. The real link subsequently reached encrypted Ready with both mouse and Consumer notifications subscribed. Trace shows accepted/transmitted wheel reports with matching release packets and no send errors. These observations do not establish that iOS or the video app executed a swipe.

App 0.1.3 adds the requested **iPhone gesture buttons** mode: UP sends mouse Button 2, DOWN Button 3, OK Button 1, each followed by release. The user must map Buttons 2/3 to the saved swipe gestures in the iPhone connected-device AssistiveTouch settings. Saved-gesture execution was confirmed by the user. OS cannot invoke a phone gesture by its saved name over standard HID.

## Validation

Build: PASS — ESP-IDF 5.5.3 complete gate for OS 1.0.50, matching archive verification, signed Settings OTA and boot ELF match.

Host tests: PASS — complete final static gate, bounded report parser/descriptor lengths, Native ABI-extension size checks, offline guards, drag direction/release/return, media control, explicit bond reset, backpressure shutdown and Button 2/3/1 pulse/release mapping. Font checks preserve the previous built-in inventory, GB2312 level 1 and all non-ASCII characters in the app.

Device tests: PASS for OTA, installed Native app, real iPhone encrypted connection and HID subscription/transmit path. OS 1.0.48 additionally passed 10/10 radio start/advertise/exit cycles, then Native XiaoZhi playback (51 decoded packets), capture (49 sent Opus packets), ABI 1 Hello and Lua startup/exit regression. Radio tests were unpaired; they do not prove paired lifecycle or long-duration behavior.

Unverified: separate Consumer Control media/volume actions, all target-app/iOS variants, paired long-duration reconnects, human font/interaction quality, optical latency and power. Classic headphone audio/telephone calls are hardware-unsupported, not a pending software test.

## Resource measurements

| Artifact | Bytes |
|---|---:|
| OS OTA | 2,420,720 |
| Merged initial-install image | 2,551,792 |
| Native ELF file | 8,272 |
| Native loaded span | 7,652 |
| Signed package | 9,123 |
| Reserved executable pool | 16,384 |

The OTA application remains below decimal 2.5 MB. The merged image includes the bootloader/partition/OTA-data region and padding before the application slot, and exceeds that optional size target. The existing OTA/data partitions are unchanged.

Measured on OS 1.0.48: warm idle free heap 108,348 bytes; advertising free heap about 51 KB; cumulative radio-test minimum free heap 49,528 bytes. After 10 cycles free heap was 108,328 bytes, reported drift 20 bytes. Pool usage returned to zero. The controller/host tasks use 4 KiB stacks each; observed unused margins before phone pairing were 2,076/1,892 bytes. LVGL maximum used was 10,612 bytes in the 16 KiB pool. System counters include concurrent Wi-Fi activity and are not isolated app allocations.

The shared RLE 2-bpp font has 4,352 glyphs and 270,277 linked read-only bytes; it replaces both the old shared supplement and duplicate built-in CJK font while retaining their character inventory. Wi-Fi optional IRAM fast paths and static RX/BA windows were adjusted to budget BLE. TLS validation, Lua and executable memory protection remain enabled.

Historical OS 1.0.47 could leave a roughly 47 KB largest block after first BLE use; the 24,524-byte Opus allocation then failed despite enough total free heap. Reboot with the namespace already loaded restored about 63 KB and capture. OS 1.0.48 prewarms the pinned `nimble_bond` namespace during boot and uses static reusable mailboxes, avoiding late persistent heap objects between codec/stack allocations. Subsequent radio/capture regression passed.

OS 1.0.49/50 reduce redundant app canvas/style updates; however, the existing serial Hello benchmark still produced 31 refresh completions for ten inputs. It is not evidence of a completed end-to-end latency fix. Further physical/renderer profiling remains open; no improved phone latency is claimed.

## Artifact and resume

Archive: `build/firmware/d47ff47a8338788af1189d59dd5f369fdf4c608381923ece2613f9c51cee5e2e`.

ELF SHA-256: `95afda057d00755d55cbcbcf4526d8bb794eca5276e8dcf3dd59295bcc26bf39`.

Package SHA-256: `ee36378c9c704d5ded2991a2dabc20d2d342708b6e9e5e6ba1affeecb31854db`.

Local OS/App Store services remain at ports 8765/8040. Raw logs, phone packet traces and intermediate failed results stay under ignored `local/bluetooth/`; publish only aggregate counters. USB `ble_debug` reports link/encryption/subscription/suspend and queued/sent/completed/rejected/failed counters. `ble_trace on/off` controls temporary packet logging; disable it after debugging. Recommended path: retain the confirmed iPhone button-to-saved-gesture mapping. Further work concerns other phones/apps, long-duration reconnects and the remaining measurements. See [developer/iPhone guide](../developer/bluetooth-peripheral.md).

## App 0.1.4: two-entry home

The home now contains only iPhone gesture buttons and Forget pairing. Button 2/3/1 press/release behavior is unchanged from the user-accepted 0.1.3 implementation. Clear pairing still requires a second OK; holding OK cancels. Host tests cover navigation, offline handling, packets, release, confirmation/cancellation and backpressure. Native ELF: 5,928 file bytes, 4,868 executable bytes; signed package: 6,779 bytes. No new RAM measurement was performed.

Published to the local App Store and verified over HTTP (package SHA-256 `a2983497fd19cf0585c7498fda61f4977d127d3e16a4a11847d2e2f3726991a0`). USB device testing was not run: no serial device was available; installation could not open the previously used port. Install/update from App Store for screen acceptance. Pairing data was not cleared. Full gate results are recorded locally in `local/bluetooth/menu014/gate.log`.

Validation: complete repository gate PASS (host/static tests, ESP-IDF firmware build and merged-image verification); Native app build PASS. Firmware was not published or flashed for this app-only change.
