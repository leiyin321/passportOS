# XiaoZhi MiniApp validation report

[简体中文](xiaozhi-miniapp.zh_CN.md)

Date: 2026-10-26

## Scope

Validates the XiaoZhi MiniApp in `examples/xiaozhi/` and provider-neutral Passport OS HTTPS, WSS, JSON, public identity, and Opus APIs. The MiniApp owns XiaoZhi provisioning semantics, protocol messages, activation, and presentation. OS code provides generic transports and audio codecs only.

## Results

| Check | Result | Measurement |
|---|---|---|
| Repository checks | PASS | 595 text files scanned |
| Static/host validation | PASS | `./tools/validate.sh --static`; runtime suite 24/24, package suite 13/13, plus repository, BSP, asset, UI, OTA and API tests |
| XiaoZhi simulator flow | PASS | Provisioning response, WSS headers, v1 hello, PTT start/audio/release and TTS path; 5 snapshots, 0 Lua errors |
| MiniApp host resource sample | PASS (host simulator only) | v0.2.3 retry flow: Lua peak 64,105 bytes dark / 64,177 bytes light; no Lua errors. The v0.2.2 PTT sample is retained in the release history below. |
| Signed package | PASS | v0.2.3 `dist/xiaozhi.pap`, 14,679 bytes; signature verified; SHA-256 `7ba3cc27e03a319d19c385a198f05a4786d05f5f74f674d9d92406ccfacc4c11` |
| ESP-IDF firmware | PASS | ESP-IDF 5.5.3 / ESP32-C3; app image 2,280,576 bytes; merged image 2,411,648 bytes; OTA slot headroom 340,864 bytes (13.0%) |
| Physical App Store install/launch | PASS; appearance test PASS; network retry pending | User confirms system appearance switching works; verify XiaoZhi 0.2.3 service setup/retry on hardware. |

## Resource accounting

The ESP32-C3 OTA app partition is 2,621,440 bytes. Firmware 1.0.20 uses 2,280,576 bytes, leaving 340,864 bytes (13.0%); the merged initial-flash image is 2,411,648 bytes. ELF section accounting reports 180,748 bytes of linked DRAM region occupancy (102,400-byte reserved/dummy region + 13,716-byte data + 64,632-byte BSS). This is static link-time occupancy; it does not include runtime heap peak. Host Lua peaks are simulator measurements, not ESP32 RAM measurements.

Firmware artifact directory: `build/firmware/724c75da75838d2b481aad39c0437293a55bef827a169814151044c49fde31c6/`. Full image SHA-256: `724c75da75838d2b481aad39c0437293a55bef827a169814151044c49fde31c6`.

The audio path uses three-entry bounded Opus TX and RX queues. It captures 16 kHz mono 60 ms frames. Codec contexts and the decoded PCM scratch buffer are released by the audio worker; the worker is stopped and deleted, and queues and semaphore are deleted when the MiniApp exits. Runtime logs report task stack high-water, free heap before/after codec close, largest free block, and post-teardown free heap (`AUDIO reclaimed`). These are instrumentation points, not physical-device measurements.

## Device failure diagnosis and pending acceptance

The initial install attempt used OS 1.0.16, whose package limit predates XiaoZhi's 64 KiB Lua heap; that failure was resolved by the system update. The user has confirmed the App Library opens and XiaoZhi launches. Firmware 1.0.20 and app 0.2.3 are published to the Mac local update and App Store services. The 0.2.3 network retry path passes in the host simulator; device contention/retry confirmation remains pending.

- Update XiaoZhi to v0.2.3 from App Store. With Wi-Fi online, launch it and confirm service setup completes or retries without the false Wi-Fi prompt.
- With the device online, launch the app while another network request is active and confirm transient queue contention retries without showing “Connect Wi-Fi”.

- Activate/connect against the official service and verify Chinese/English rendering.
- Hold OK through speech upload; release and verify capture stops while response playback continues.
- Verify TTS Opus playback and reconnect/error behavior.
- Record live task stack high-water, heap before/after codecs, largest free block, and post-exit heap recovery from serial logs.
- Confirm audio service teardown and repeated launch/exit do not leak memory.

The package is published only to the local Mac development App Store catalog for user testing. Do not promote it to a public catalog until the device acceptance items above pass.

## Full-screen readability update 0.2.1

The user confirmed the App Library opens and XiaoZhi launches, then reported that the app content used too little screen area and the text/background contrast was weak. Version 0.2.1 switches the Manifest to fullscreen and uses a full-viewport dark background, high-contrast status panels, and wider page cards. The Voice, Conversation, and Connection pages now share the 240×320 viewport. The signed package is regenerated at `dist/xiaozhi.pap`; the current catalog at `http://192.168.1.10:8040/index.json` is served directly from `dist/`.

`./tools/validate.sh --static` passes, including all 20 runtime tests. The XiaoZhi test asserts a 240×320 fullscreen viewport, full-screen background draw command, and high-contrast status surface; the provisioning/WSS/PTT flow remains passing. Package signature/manifest validation passes. Package size is 13,602 bytes and SHA-256 is `d85538bf23febc372c291edd15a85f507110329c786caf537cb5e02094a3fa36`; the live index and package were fetched from the LAN service and their version/size/hash matched. Physical display readability and Chinese glyph rendering still require the user to check the updated app on hardware.

## System update publication

The build was repeated with `PASSPORT_OS_VERSION=1.0.17` so Settings → About matches the update manifest. Signed build `2026102520` is available from the Mac service at `http://192.168.1.10:8765/latest.json`. App image SHA-256: `799c6daacfa47a5197d4ed9422052b76783fff2e06294a1076af186d8bbffe8e`; full merged image SHA-256: `5021f4fb6a7a6b04ce1682c8d7213b6754e9f3b0ac10c713c356e6ce61cbb43a`. Manifest and full image were fetched from the live service and verified with the system release public key. Device update/install was not initiated.

## Local App Store publication

The cause of the empty Store was `dist/index.json` containing an empty `apps` array. The running catalog server on port 8040 was healthy. Following the request to restart MiniApp work with an empty Store, added only XiaoZhi AI (v0.2.0) to the catalog and retained no prior apps. The live LAN endpoint returned the one-entry index and the 12,995-byte signed package; catalog hash, package hash, and publisher signature verification passed. The device loaded the listing, then rejected the package on OS 1.0.16 because of the 64 KiB heap limit; upgrade to 1.0.17 before retrying.

## System appearance API and app-owned palette 0.2.2 / OS 1.0.20

OS now persists Dark/Light appearance in NVS and exposes the current value through read-only `passport.system.display_mode()` (`"dark"` or `"light"`). Apps decide whether to follow it. `passport.text(value, rgb24)` lets an app set text color; omitting it uses a readable system-mode default. XiaoZhi 0.2.2 reads the mode and draws its own high-contrast palette, including white background and dark text in Light mode. OS does not force app colors.

`./tools/validate.sh --static` passed: 603 repository text files scanned, runtime 24/24, package 13/13, compiler 12/12, plus BSP, font, layout, API, and OTA checks. XiaoZhi host scenarios produced five frames per mode without Lua errors. Lua peak was 64,105 bytes Dark and 64,177 bytes Light; microphone state was false after release. These are simulator results; hardware RAM peak and post-exit reclamation remain unmeasured.

Signed OS 1.0.20 build `2026102601` is published to the local update service. App image 2,280,576 bytes; merged image 2,411,648 bytes; OTA headroom 340,864 bytes (13.0%). App image SHA-256 `d33d67b9d8bd80ce0c9ae9a61f838d2addbcf97f8f351795b820349f564e0998`; merged image SHA-256 `91253dd4ebdb7250316b488babf975346ab8b1a91444d61b9fa2c853df928a45`; ELF SHA-256 `987927e90863dc8990fe05b64acb9909fbd900211cfb979534653fe3c82fba81`. XiaoZhi 0.2.2 was the appearance-aware release at that time; it was superseded by 0.2.3 after the network retry fix. The user confirmed system appearance switching passes on hardware. The v0.2.3 service retry path still needs hardware confirmation.

## Network worker contention recovery 0.2.3

The reported “Connect Wi-Fi in Settings first” screen could be produced when `ps_net_request()` rejected submission because the shared OS HTTP worker was busy; the app treated every immediate `false` as proof Wi-Fi was offline. Version 0.2.3 now displays a localized network-wait status and retries after one second. Transient HTTP `-1`, 408, 429, and 5xx results retry after three seconds. Permanent HTTP errors remain visible and do not loop.

A simulator fault injection rejects the first HTTP submission while Wi-Fi remains available, then accepts the retry. The regression verifies that the waiting message appears first and the XiaoZhi service configuration succeeds after retry. Both this regression and the normal config/WSS/PTT scenario passed; the full `./tools/validate.sh --static` gate passed with 24/24 runtime tests. Dark/light host Lua peaks with the retry change are 65,146 / 65,074 bytes, with no Lua errors.

Signed package v0.2.3 is published in the local App Store catalog. Size 14,679 bytes; SHA-256 `7ba3cc27e03a319d19c385a198f05a4786d05f5f74f674d9d92406ccfacc4c11`. The live catalog and package were fetched from port 8040 and matched the published files byte-for-byte; publisher signature validation passed. Hardware test should update XiaoZhi from App Store and launch it with the device already online; verify service configuration proceeds without the false Wi-Fi prompt. Firmware was not changed for this MiniApp-only fix.

## Device follow-up: persistent network wait and OS diagnostics

The user reports v0.2.3 remains on “Waiting for network…” despite OS Wi-Fi working. USB `device metrics` and `device view` confirmed the MiniApp is active, the UI is in that retry state, and the OS has its Wi-Fi and HTTP tasks running. This proves the symptom persists but does not yet identify whether the request is rejected by queue guards or fails in DNS/TLS/HTTP transport. A Mac HTTPS POST to the official endpoint confirmed public DNS/TLS reachability, but without a device identity it correctly returned HTTP 400 and is not a device-path test.

OS 1.0.21 build `2026102602` adds sanitized stage logs for queue rejection, network policy, response allocation, TLS/open, request write, headers, and response read; it never logs URL, identity, body, or authorization headers. The full build and host suite passed (24/24 runtime tests). The signed release is published at `http://192.168.1.10:8765/latest.json`; app image 2,281,344 bytes, merged image 2,412,416 bytes, OTA headroom 340,096 bytes (13.0%). App hash `1d281b70a4ecebf63b0a036d3238561a9954fc7a9935837a7379e9c4b498da15`; merged hash `fabcb93fb2040fc7c47b52d58dca9a955f9823d21818c5217678223f82722bc6`; ELF hash `a11f1b8872cf127a0675edb771104cfa742896240c19b7c602f47b7c24e14d6f`. Live OTA signature, exact size, and hash passed. Next device step: install 1.0.21 via Settings → System Update, reproduce the wait, then collect the `passport_http` log lines to apply the root-cause fix.

## OS 1.0.23 review follow-up

The 1.0.21 device boot was confirmed by its version/ELF identity; Wi-Fi obtained
an address. Source review found a 9,308 B request job versus an observed 8,192 B
largest free block, retained Lua source, and incorrect chunked-response handling.
The 1.0.23 fixes and exact validation/resource evidence are in the
[system review](../development/passport-os-review.md). The signed OTA is live.
This is not yet a successful device activation/voice test; install the update and
complete the remaining checks recorded there.

## TLS allocation failure: OS 1.0.24 / MiniApp 0.2.4 (2026-10-26)

A USB observation of the pre-fix device reproduced the failure before any HTTP response: `stage=open`, `ESP_ERR_HTTP_CONNECT`, certificate verification `0x4290`, then handshake `-0x3000`. In the installed ESP-IDF 5.5.3 headers, `0x4290` combines RSA public-operation failure (`0x4280`) with MPI allocation failure (`0x0010`). Wi-Fi policy passed. This is evidence of allocation failure during certificate verification, not proof of an invalid server certificate or offline Wi-Fi. The boot-lifetime minimum free heap was 40 bytes; app exit recovered to 77,740 bytes. These measurements describe the pre-fix firmware, not the new release.

OS enables ESP-IDF dynamic TLS record buffers and collects unreachable Lua objects before HTTP/WSS submission. Certificate-chain and hostname checks remain enabled. MiniApp 0.2.4 stops transient retries after three attempts, shows HTTPS status/attempt count, supports manual retry on Connection, displays activation codes directly, includes board.type, and accepts an optional empty WebSocket token as the official client does. XiaoZhi-specific headers, account activation, and protocol stay in the app. References: [official configuration client](https://github.com/78/xiaozhi-esp32/blob/main/main/ota.cc), [official WebSocket client](https://github.com/78/xiaozhi-esp32/blob/main/main/protocols/websocket_protocol.cc), [ESP-IDF TLS options](https://github.com/espressif/esp-idf/blob/v5.5.3/components/mbedtls/Kconfig).

Build: PASS, complete validation gate. Host tests: PASS, runtime 29/29 including bounded transport failure, activation-only response, and empty token. Simulator Lua peak is 65,530 bytes (64-bit host, very little margin under 64 KiB; not a board measurement). Existing actual-worker and audio lifecycle harnesses pass. Firmware application size: 2,287,648 bytes; OTA headroom: 333,792 bytes; merged image: 2,418,720 bytes. App package: 14,928 bytes.

Published OS 1.0.24 build 2026102605 and MiniApp 0.2.4 to the existing local services. Both served binaries match their catalog/manifest size and SHA-256. Firmware archive/full-image SHA: `d2849ea314494984194db93c9af465bc524c7bd4c9f06dfd81693f9b83845e79`; ELF SHA: `3c914f7b9a893e6ac2cc24d6b2c841c3c62981da4744a76006ff250522fc5eec`; app image SHA: `afda069643744f36813313e0df1841d3066bc0a9f3ce8e33a3289bfe10d863a3`; MiniApp SHA: `b799f5ee436d39e480c94c47d82e61d0687dc76858b64419445dc24dbb496530`.

Device tests: failure reproduced; repaired firmware pending Settings OTA installation. Unverified: new TLS peak, successful official service configuration, account activation, WSS/PTT/audio, and repeated exit recovery. No USB firmware flash was performed.

## WebSocket handshake follow-up: 1.0.25 / 0.2.5

The next USB capture reached WebSocket but reproduced RSA/MPI allocation failure (`0x4290`) and a failed 4,437-byte TLS allocation. A single injected OK press+click started two WebSocket clients: the MiniApp cleared its pending flag on OS status 101, which means client task started, not TLS/WebSocket connected. Version 0.2.5 keeps the guard until `ws_connected()` becomes true or a failure/timeout occurs, retains only the selected language table, and identifies WSS/hello errors. A signed 0.2.5 package was installed through the MiniApp installer (not firmware flashing). Physical retest produced exactly one client start for the press+click, but still failed TLS: free heap before handshake 20,484 bytes, largest block 7,424, Lua 31,917; boot-lifetime minimum 36 bytes.

OS 1.0.25 reduces the single LVGL RGB565 draw tile from 40 to 16 rows: 19,200 to 7,680 bytes, releasing 11,520 bytes for shared heap. Resolution and the LVGL widget pool are unchanged. Host state-machine regression holds the socket in connecting state across press/release/click and verifies one open call. Hardware TLS success, updated minimum heap, rendering throughput, and exit recovery still require OTA testing; do not interpret the deterministic buffer reduction as a measured TLS peak.

Release verification: complete Build PASS; Host tests PASS (30 runtime cases). Live OTA 1.0.25 build 2026102606 and Store 0.2.5 binaries passed size/hash checks; the installed MiniApp matches the published package byte for byte. App image 2,287,648 bytes, SHA `289aa5b6dd05b9c6c69fdab8c468121735f4501390817628cd9dca1b7c9cda19`; full image/archive SHA `3134393bb12074c44f918f5cfe596abc4b32994407e24f8dd24eb881a0083907`; ELF SHA `d25eda36f424ae2ad9c6216defc49dd948a48cd5b156cd021830cfeed6626d15`. MiniApp 14,999 bytes, SHA `484b1b38f507fe91645a48a113ba84eb6c4ebe6f7acde4b74d710556dfa0009e`. Device TLS acceptance remains pending Settings OTA.

## WSS authority routing failure: OS 1.0.26

On the 1.0.25 device, the next capture logged `Certificate validated`: the earlier TLS allocation failure no longer occurred in this attempt. Before handshake: free heap 31,996 bytes, largest block 12,288, boot minimum 9,852, Lua 31,917. The WebSocket upgrade then failed with HTTP 502 and missing Sec-WebSocket-Accept. A real HTTPS upgrade comparison from the Mac to `/xiaozhi/v1/`, using identical upgrade parameters and no device credentials, returned 101 with Host `api.tenclass.net`, but 502 with Host set to the resolved IP plus port. This isolates the observed failure to authority routing rather than hello JSON.

The generic OS adapter now sends the approved hostname to the native WebSocket handshake while dialing only the policy-validated IP. TLS verification continues to use the hostname and certificate bundle. A small isolated ESP-IDF 5.5.3 adapter replaces only the owned TCP/TLS transport connect callback, preserving its context, socket access and error handling; the native WebSocket framing implementation remains unchanged. The private transport struct ABI requires review when upgrading ESP-IDF. No developer-specific headers or XiaoZhi provider were added to OS.

Regression test executes the adapter with a fake underlying transport and verifies distinct dial IP, Host authority and TLS identity; allocation/config failures at all six construction stages release every handle, and a second active connection is rejected. Hardware 1.0.26 WebSocket/hello/audio acceptance remains pending OTA; the Mac 101 comparison is not device end-to-end validation.

Release: OS 1.0.26 build 2026102607 published; live manifest/image size and hash match. Build PASS; Host tests PASS including the adapter sanitizer test and 30 runtime tests. App image 2,288,224 bytes (333,216 bytes OTA headroom), SHA `afa91a1d3615654ea3d3329a862e61782b24645a2c2928211b6bdecf449e6ad7`; full image/archive SHA `d2e7fb69feb9ad58b2491a0388b905f59e9bfd3852e9db642990f05d9ef21e7e`; matching ELF SHA `dc86f14af0124738e46028dbc6e897bc420ee4de4e4aff046e720bd1a5b3c2c4`. MiniApp remains 0.2.5. No USB firmware flash.

## Opus negotiation and audio task stack: 0.2.6 / OS 1.0.27

The app incorrectly required the server downlink rate to equal the 16 kHz microphone/output rate. Opus can decode supported-rate streams directly to a selected output rate ([decoder API](https://www.opus-codec.org/docs/opus_api-1.6/group__opus__decoder.html)). 0.2.6 accepts mono Opus at 8/12/16/24/48 kHz and declared frames up to 60 ms, retains invalid format/channel/rate/oversized-frame rejection, and defers capture start until the following tick. OS already configures the Opus decoder and I2S output to 16 kHz; no resampler or proprietary provider was added. Actual server rate was not captured, so 24 kHz must not be stated as a measured device value.

Tests: 31 runtime tests pass, including eight valid/invalid hello combinations. A real host libopus test encoded a 60 ms tone at each of 8/12/16/24/48 kHz and decoded each into exactly 960 samples at 16 kHz. This verifies codec behavior on the host, not the ESP codec binary.

Hardware progressed past negotiation and exposed a stack-protection panic. A hardware breakpoint using the matching 1.0.26 ELF (`dc86f14af0124738e46028dbc6e897bc420ee4de4e4aff046e720bd1a5b3c2c4`) identifies the faulting thread as `passport_audio`; its native capture frame reserves 4,752 bytes against a 6,144-byte task stack. A later TCB inspection still showed the old approximately 6 KiB stack; debugger register-override experiments are therefore not acceptance evidence for the new stack. OS 1.0.27 raises only this task to 10,240 bytes, adding 4,096 bytes of heap cost while audio is active. New stack headroom, encoder/decoder heap requirements, end-to-end voice, and exit reclamation remain unverified pending OTA. No firmware was USB-flashed. Debugger was shut down and target reset to normal execution.

Build PASS; Host tests PASS. Published OS 1.0.27 build 2026102608 and MiniApp 0.2.6, verified live hashes/sizes. Firmware app 2,288,224 bytes, SHA `5185821c97353da98d40d6776303016ee0bcd255dae0c4aa72048b3d4ae3e265`; archive/full image SHA `7d243bb0217afcb33c1d093c04faf8bf647da8b57d6a86df8c77d035dcb82c9a`. MiniApp 15,316 bytes, SHA `2341132e7c660b8500ae31f8c7878b8baad1e5131590eb8eafb14ef7a6e11607`.

## Microphone consent vs stream startup: OS 1.0.28 / MiniApp 0.2.7

The reported “Microphone permission denied” did not prove that consent was denied. OS code called `ps_runtime_audio_allowed(false)` whenever the already-authorized audio worker could not start; the MiniApp also mapped every false `audio.stream_start()` result to its denial string. The runtime now keeps consent separate from capture health. On asynchronous worker startup failure it clears current capture flags, calls generic `on_audio_error(1)`, and lets main-loop cleanup release the failed worker. XiaoZhi checks local audio startup before sending `listen/start`, displays a localized retry message for audio resource failures, and reserves the denial message for an explicit user choice.

Verification: 33 runtime tests pass, including immediate and asynchronous startup failures without a permission-denied message, plus the regular permission-denial paths. Chinese message glyphs were checked in the compiled app font/fallback chain. `tools/check_repo.py` passes. The firmware build phase passed. The subsequent `./tools/validate.sh --static` rerun passed all host tests, including 33 runtime cases and font coverage. Firmware code and size were not changed after the full build. Live OTA and Store package sizes/hashes match their signed manifests.

Published OS 1.0.28 build 2026102609 (app 2,288,432 bytes, SHA `0479851836f53c7644911ceb3760d855b63242fe59a0fc54b45571ae7a004ea4`; full image/archive SHA `9644e72c23aa50ba246dc1367798ee72e9ca2e3eb2f8548b4a1ed568f8fb9a88`; ELF SHA `08893a31816c675de1961a7899e79eeeacb32839b23fc3dfa30af8c7da8b74f3`) and XiaoZhi 0.2.7 (15,511 bytes, SHA `349124afce669f79eac8eca79dbdd8e8d2c9c37393b8316d28ecb053fa070072`). Device-side audio startup and successful PTT remain unverified until the update is installed and retried.

## Audio task allocation order: OS 1.0.29

The user installed 1.0.28/0.2.7 and reported “Microphone temporarily unavailable”. The preceding on-device measurements were about 22,272 bytes free heap / 9,728-byte largest block after WSS setup and 19,580 / 7,680 after attempted audio startup. The audio service allocated approximately 4.8 KiB of queues before requesting a 10 KiB contiguous task stack; logs had no `AUDIO requested`, consistent with task creation failing before the worker started.

The worker now claims its 9,216-byte stack before allocating the stream queues. A startup semaphore keeps it blocked until queue setup completes. Failure logs name the allocation stage and report free heap/largest block. The host audio lifecycle test now injects failures for both semaphores, task creation, and each stream queue, verifying complete cleanup.

Validation: the full `PASSPORT_OS_VERSION=1.0.29 ./tools/validate.sh` gate passed with ESP-IDF 5.5.3. App image 2,289,120 bytes / 2,621,440 (332,320 bytes, 12.68% OTA headroom); merged image 2,420,192 bytes. Archive/full SHA-256 `4a758ee3d9e6b8e11c430cdfba3d2552b7ff2f261ca5cecae11a36e9a6f61944`; app image SHA-256 `630446dbeb6bdb0d186a3aed53418386ee00f5bf0d5fe0379a7e346a2b178a63`; matching ELF SHA-256 `a80313c29c73b5e5bcd67b2b200b3d71439840761c6d9a771135dcfe050518ad`. The audio host test passes all allocation-failure and teardown cases. Signed OS 1.0.29 / build 2026102610 is published at the Mac update service; downloaded manifest and firmware are byte-identical to the staged release and the image SHA matches.

RAM is not measured on new firmware yet. The 9 KiB task stack is only a design budget based on the prior 4,752-byte capture frame measurement; actual codec high-water, heap peak, successful PTT, and post-exit audio resource recovery require installing 1.0.29 through Settings → System Update and repeating the voice test. No USB firmware write was performed.

## WSS heap fragmentation reproduced: OS 1.0.31 / MiniApp 0.2.8

The user installed 1.0.29 and reproduced the unavailable-audio hint. The device reported app runtime heap 46,472 B / largest block 15,872 B after launch; after HTTPS/WSS setup it reported 22,428 B free with a 7,680-byte largest block. Every stream start failed before task creation at `stage=task-stack bytes=9216`; Opus or BSP initialization had not run. The 1.0.29 queue-order change was therefore insufficient. Repeated logs about 100 ms apart were caused by XiaoZhi retaining `ptt_held` after a failed start, so `on_tick` kept retrying.

OS 1.0.30 prepares the same 9,216-byte idle worker stack immediately after the app runtime starts and before the main loop processes its queued network work. OS 1.0.31 retains that behavior and also allows a prepared worker to serve ordinary, non-streaming microphone apps. The worker blocks without initializing or reading the microphone. It is activated only after the app's audio permission and stream request; it remains available between PTT sessions and is reclaimed on app exit or permission denial. MiniApp 0.2.8 clears `ptt_held` on synchronous and asynchronous errors; host injection confirms a single attempt over subsequent ticks and requires a release before retry.

The read-only USB serial observation itself reset the ESP32-C3 (`USB_UART_CHIP_RESET`); it only read startup/runtime logs and did not write Flash. Device-reported measurements above are from OS 1.0.29; no OS 1.0.30 hardware test has yet run.

Validation: full `PASSPORT_OS_VERSION=1.0.31 ./tools/validate.sh` passed with ESP-IDF 5.5.3; 33/33 runtime tests pass, including stream preparation, ordinary microphone compatibility, and the two XiaoZhi retry regressions. App image 2,289,984 / 2,621,440 bytes (331,456 bytes / 12.64% OTA headroom); merged image 2,421,056 bytes. Archive/full SHA-256 `efa58799b17ab088d892c749af76dfba4050bfc6d7a9c29358268f9485e300d6`; app SHA-256 `eca67a0597bf1011cf4cece4d7949176e2bd8024a29bfd4414592297a5b6c277`; ELF SHA-256 `f525b78ef221570dff2543e35da44216b64cc825af62c5014076ec16c6dc6d15`. Signed OS 1.0.31/build 2026102612 is available from the local update service; the downloaded signed manifest and image match. XiaoZhi 0.2.8 (15,551 bytes, SHA-256 `0a05b02fb0d6b07b13ec452a531d5ab4320ddc79a5af749eef0466281ed3bef2`) is on the local App Store; live catalog and package match and the package signature validates.

At that checkpoint, installation and device acceptance were pending. The later 1.0.31 WSS failure and 1.0.32 follow-up are recorded below.

## TLS regression from pre-reservation: OS 1.0.32

The user's report was reproduced on OS 1.0.31. Device logs showed app runtime heap 36,668 B after the audio stack was reserved, then 22,796 B free / 8,704 B largest block after the HTTPS configuration request. During WSS setup, certificate signature verification failed with mbedTLS `0x4290`; the TLS handshake returned `-0x3000`. This is consistent with RSA verification being unable to allocate from ordinary internal heap while the audio stack occupied it. Wi-Fi was online and the websocket client started, so this was not a service hostname/hello-protocol failure.

OS 1.0.32 keeps the 9,216-byte worker stack in the ESP32-C3 Retention RAM heap via `xTaskCreateWithCaps(..., MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT)`. Task deletion uses `vTaskDeleteWithCaps`; the TCB remains in internal RAM. This leaves ordinary heap blocks available to TLS and preserves the stack size needed by the earlier codec overflow evidence. Logs now report free ordinary heap, largest block, and free Retention RAM after preparation. The host audio service test verifies the requested stack capability and every resource cleanup path.

The serial observation again triggered `USB_UART_CHIP_RESET`; it read logs only and did not write Flash. No 1.0.32 device run has occurred yet. Full ESP-IDF 5.5.3 validation and host tests pass (runtime 33/33). App image 2,290,112 / 2,621,440 B (331,328 B / 12.64% OTA headroom); merged image 2,421,184 B. Archive/full SHA-256 `f365f8c0dbf45b616200c332d0acd8c49cab850b1a0881e36cc20da8b84b7b6d`; app SHA-256 `e50eb345f1fbabda1fcba303d52aa4892584757ad3202b4f51e50b977efe244b`; ELF SHA-256 `0ce5b949821e3e74cb3f3dc07ef89d921f17cb206298b16ff7f3cd93d27136ae`. Signed OS 1.0.32/build 2026102613 was downloaded from the local service and verified. XiaoZhi 0.2.8 remains live in the Store (15,551 B, SHA-256 `0a05b02fb0d6b07b13ec452a531d5ab4320ddc79a5af749eef0466281ed3bef2`).

Pending: install OS 1.0.32 and retain XiaoZhi 0.2.8; verify WSS reaches hello, speech upload/TTS playback, audio task stack high-water, heap peak, Retention RAM use, and resource reclamation on app exit/repeated sessions.

## Deferred audio stack allocation: OS 1.0.33

The user confirmed the same `WSS/hello: Network or protocol error` after installing 1.0.32. Serial observation identified OS 1.0.32 on `ota_0`; Wi-Fi connected successfully. Together with the earlier `0x4290` and ESP-IDF mbedTLS allocator source, the evidence points to the RSA certificate verifier's approximately 4.4 KiB `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` allocation. Reserving the 9 KiB audio task stack in Retention RAM still consumes the chip's shared internal RAM. The prior `largest8` metric combines memory capabilities and could not show the largest block TLS could use.

OS 1.0.33 removes audio worker preallocation both at app launch and when microphone permission is granted. Permission now updates runtime state only. The 9,216-byte worker stack is allocated with `MALLOC_CAP_RETENTION` only when Lua calls `audio.stream_start()` and actual capture begins, so it no longer competes with WSS certificate verification. System `METRIC` logs now report free bytes and largest blocks separately for Internal and Retention RAM. Audio failure and teardown behavior is unchanged.

Validation: full repository and host suite passed (runtime 34/34, package 13/13, icon compiler 12/12, plus BSP, network, OTA and other host checks); ESP-IDF 5.5.3 ESP32-C3 build and image layout verification passed. A regression assertion confirms microphone consent alone does not allocate the audio worker. Host tests do not establish device WSS acceptance. Install 1.0.33 through Settings → System Update, verify TLS reaches HTTP 101/hello, then verify PTT upload, TTS playback, actual Internal/Retention RAM peaks, and heap reclamation after app exit.
