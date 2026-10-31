<p align="right">
  <a href="wifi-provisioning.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Wi-Fi Connection via Bluetooth Provisioning

When a user needs the device to connect to a Wi-Fi network, the
[`demo/blufi-provisioning` branch](https://github.com/FoloToy/ai-passport/tree/demo/blufi-provisioning)
provides a reference for Bluetooth-based Wi-Fi setup. The phone supplies the
SSID and password over BLE using BLUFI; the device connects as a Wi-Fi station
and reports connection status. Bluetooth carries the provisioning exchange,
not the application's Internet traffic.

This is an optional application reference. The current `main` Wi-Fi demo only
scans networks; it does not implement connection or provisioning. Do not enable
networking for applications that do not need it.

## Mini program name

Use the companion mini program identified by the
[exact Chinese search name](wifi-provisioning.zh_CN.md#mini-program-name).
Keep that name unchanged in setup instructions rather than inventing a
translated search name. It is the mini program's name, not the device's BLE
advertising name. The reference firmware advertises as `BLUFI_FoloPassport`.

The intended flow is to open provisioning on the device, enable Bluetooth and
the required permissions on the phone, open the mini program, select the target
device, and provide a 2.4 GHz Wi-Fi network's credentials. Confirm that the
device obtains an IP address, then test the application's actual network
request; a successful BLE connection alone does not prove Internet access.

## Code to inspect

- [`main/demo_blufi.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_blufi.c): BLUFI callbacks, Wi-Fi scanning/connection, credential handling, status reporting, and lifecycle.
- [`main/demo_blufi_security.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_blufi_security.c) and its header: BLUFI security negotiation callbacks; do not discard them when extracting the example.
- [`main/demo_radio.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_radio.c): shared initialization of NVS, `esp_netif`, and the default event loop.
- [`main/CMakeLists.txt`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/CMakeLists.txt) and [`sdkconfig.defaults`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/sdkconfig.defaults): source registration, dependencies, NimBLE/BLUFI, and cryptographic configuration. Copying a single C file is not sufficient.

## Integration and acceptance

Start from the application's current baseline, record the reference commit,
and adapt only the relevant networking logic. Do not merge the entire demo or
overwrite current BSP, partitions, or configuration with the branch's older
versions. Implement the application's own UI and keep provisioning state,
tasks, and Wi-Fi/BLE services in the application layer.

Define provisioning entry/exit, timeouts, retry limits, credential persistence,
and an explicit way to clear saved credentials. Never log or commit Wi-Fi
passwords. Preserve LVGL locking, non-blocking callbacks, and cleanup of tasks
and event handlers; budget internal RAM for concurrent Wi-Fi, BLE, and UI use.
The example is not a complete production authorization or security design.

Run the [validation gate](build-and-test.md), then follow the
[on-device testing handoff](../ai-guide.md#offer-on-device-testing). Test with
the named mini program: discovery, successful provisioning, wrong-password and
unavailable-network failures, reconnect/restart behavior, credential clearing,
repeated entry/exit, and an actual application network request. Build success
does not prove mini program compatibility or on-device networking; keep
unperformed checks under `Unverified`.

## Passport OS local phone portal

Settings → Wi-Fi creates a password-protected temporary AP and serves a self-contained page at `http://192.168.4.1/`. The nearby-network list is scanned by the ESP32-C3 (2.4 GHz), not by the browser. Up to 16 strongest records are retrieved, duplicate SSIDs are collapsed, and hidden networks remain available through manual entry. Scan failure permits retry/manual entry. Scanning runs on the HTTP server task; offline reconnect is briefly suppressed and resumed after scanning. Driver results and temporary JSON allocations are released on all result paths.

The page supports Chinese/English, network selection, password visibility, manual entry and optional standard `WIFI:T:WPA;S:...;P:...;;` / `nopass` shared-text import, including escaped delimiters. Imported values only fill the form; users review and submit. It cannot enumerate a phone's saved networks/passwords, decode QR images or invoke Apple's nearby password-sharing protocol. On iPhone, users may copy a saved password from Passwords → Wi-Fi and paste it into the page ([Apple guide](https://support.apple.com/en-euro/guide/iphone/iph96e6aa9bb/ios)). No external assets, credential logging, cloud upload or clipboard permission is required. The existing protected AP and POST credential flow remain.

Host regression executes the shipped JavaScript using Node.js (`tests/test_wifi_setup_page.py`) and checks escaped QR text, duplicate/unsupported fields, UTF-8 byte bounds and SSID rendering through `textContent`. Physical phone AP connectivity, scan results and saving/reconnecting require device acceptance; host tests do not establish these.

## Multiple saved networks (OS 1.0.63)

Up to eight distinct SSIDs are stored in a versioned 780-byte NVS blob under `appdata/wifi_profiles/saved`; passwords are never included in list/scan responses. Existing single driver credentials are imported only when this key is absent. A valid empty list is authoritative and does not re-import forgotten credentials. Malformed/unreadable data is reported instead of silently restoring forgotten entries. New credentials update an existing SSID or append; a full list rejects new entries rather than evicting one. A failed NVS operation retains the prior in-memory list/connection; crash consistency relies on NVS. NVS encryption is not enabled by this feature.

A 4 KiB worker owns NVS writes and connect/retry; callbacks only update connectivity bits. At boot or link loss it first tries the most recently successful network. After a 12-second attempt timeout it scans, preferring that network if untried/visible, otherwise the strongest untried saved network. Directed attempts cover known hidden SSIDs. After all eight are tried, it backs off 30 seconds and repeats. It does not roam or scan while connected. Successful connection moves the profile to the front only when its position changes. Manual scans and worker scans serialize through a radio mutex, including result retrieval/release.

The phone portal lists saved SSIDs, confirms forgetting, supports refresh and reports full-list/storage errors. `GET /saved` returns names/status/capacity only; `POST /wifi` and `POST /forget` accept bounded URL-encoded SSID/password fields and require `X-Passport-Setup: 1` (same-origin JavaScript; no cross-origin CORS preflight allowed). Forget uses an empty password field and matches SSID, not unstable list indexes. POST acceptance queues a worker operation, not proof of successful connection; list status 0=ok, 1=full/invalid, 2=storage failure, 3=pending, 4=requested network failed and another saved network connected. Setup auto-closes only after successful requested connection; fallback keeps it open for correction. Cancellation stops only the portal/AP, preserving saved networks and reconnection. Rollback to older OS versions may use the old driver's last single credential; no new layout or erase is performed.

The final profile uses ESP-IDF `WIFI_ALL_CHANNEL_SCAN`, RSSI AP sorting and two driver retries per AP to tolerate authentication/association transients before trying another saved network. OS 1.0.61 was an intermediate test build: persistence was correct, but single-attempt startup sometimes suffered authentication timeouts. Disconnect diagnostics include reason codes only. Build the static gzip asset with `python3 tools/passport/build_wifi_setup_page.py` after editing `os/services/network/wifi_setup_page.html`; host tests verify decompression exactly matches the canonical HTML. Browsers decode the gzip HTTP response automatically. The final on-device checkpoint is in the [phone setup report](../../test-reports/wifi-phone-setup.md).

OS 1.0.63 also keeps the portal open after forgetting a failed requested network: forget clears the pending join intent before queuing the operation. The submitted-join flag is atomic across the HTTP and main tasks. Only a later explicit successful join can auto-close setup; list management alone does not.

## Scan-to-join setup (OS 1.0.65)

Settings → Wi-Fi → Phone setup shows three steps. Step 1 displays a standard `WIFI:T:WPA;S:...;P:...;;` QR for the temporary WPA2 AP; the phone camera can offer to join without typing the generated key. OK advances to a second QR containing `http://192.168.4.1/`, then to the manual SSID/key/address fallback, then back to the first QR. UP/DOWN moves between steps; double OK cancels. Camera joining/opening remains a user-confirmed phone action, not an automatic phone credential import.

The existing random 12-character key, two-client limit, saved-network store, scan/list/forget endpoints and same-origin POST checks remain. AP credentials are stable across the three steps, refreshed on a new setup session, while the displayed-key buffer is cleared and QR resources freed on cancellation/success. Reopening an already active service reads its actual AP config rather than returning a placeholder SSID. Ordinary logs omit the QR payload/key.

The UI reuses LVGL 9.5's QR widget and encoder. I1 rendering and Canvas/QR are enabled explicitly; RGB888/XRGB8888/L8/AL88 remain disabled. A 176×176 I1 image uses a system-heap pixel buffer through a cloned draw-buffer allocator, while LVGL owns metadata and transient encoding buffers. The previous constructor buffer is freed on replacement, and widget deletion frees both metadata and pixel storage. A black/white image plus twelve-pixel white outline provides contrast and quiet space independently of theme. The object occupies x=32,y=82 with text below at y=272; normal body layout is restored on exit. QR generation is cached by step, not repeated on clock ticks.

`ps_wifi_setup_qr_payload` is ESP-IDF/LVGL-independent and bounds/escapes Wi-Fi fields. Regression covers UTF-8, delimiters/backslashes, open/WPA representation, maximum input/output and invalid controls. `wifi_qr_probe` is a bounded USB diagnostic for the current 176-row I1 image; its output encodes the temporary AP key and must remain private. It is not a MiniApp or network endpoint. See the [device report](../../test-reports/wifi-phone-setup.md) for offline camera decoding, font checks and cleanup cycles.

## Captive portal and same-phone hotspot — OS 1.0.67

After a phone obtains a DHCP lease on the encrypted setup AP, step 1 advances once to step 2. AP DHCP advertises 192.168.4.1 as DNS; an AP-bound UDP responder returns that address for A questions and NODATA for AAAA. HTTP connectivity probes redirect to the local page with a nonempty body. No HTTPS interception or invalid HTTP DHCP option 114 is used. See the [ESP-IDF reference](https://github.com/espressif/esp-idf/tree/v5.5.3/examples/protocols/http_server/captive_portal). Phone OS auto-opening remains platform-dependent; URL QR/manual fallback stays available.

For this same phone's hotspot, select “Connect to this phone's hotspot” and manually enter its hotspot name/password, or select an already saved profile. The phone cannot broadcast its hotspot while connected to the device AP: credentials are first durably stored, then Passport closes its AP after an approximately 1.8-second response window. Enable Personal Hotspot on the phone; Passport prioritizes that target for up to five minutes. On iPhone enable Allow Others to Join and, for compatible models, Maximize Compatibility (2.4 GHz; [Apple reference](https://support.apple.com/en-ca/guide/security/secfd166f620/web)). Subsequent use only requires enabling the saved hotspot. Nearby scanning is disabled in this mode because the same phone is not broadcasting yet.

Normal submission allows 40 seconds. Completion requires the actual connected SSID to equal the submitted target, a valid DHCP address, and 1.5 seconds of stable connection; after a 2.5-second success grace the setup closes automatically. This verifies LAN connectivity, not public Internet reachability. Connection to a different saved network does not count as success. Cancel ends monitoring and restores normal saved-network selection, but does not forget saved credentials. OK on the hotspot waiting page restarts configuration; double-OK cancels.

The existing bounded form/header rules also apply to POST `/hotspot` (deferred new credentials) and `/connect-hotspot` (deferred saved profile). GET `/status` exposes state, error, deferred and stored flags without credentials. `/connect` retains immediate saved-profile connection. The UDP worker and HTTP server stop before AP teardown; the full setup monitor persists during deferred handoff. Portable flow/DNS tests live in `tests/test_portal.c`; shipped page behavior is covered by `tests/test_wifi_setup_page.py`.

The system status bar uses the built-in LVGL Wi-Fi symbol for an active connection; its clock is horizontally centered. Settings → Wi-Fi shows Connected and the actual associated SSID (not a saved or pending target). Network names use the multilingual font even with English system UI; arbitrary SSIDs remain limited by installed glyph coverage.
