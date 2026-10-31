[简体中文](bluetooth-peripheral.zh_CN.md)

# Native Bluetooth peripherals

Source: [`examples/bluetooth_peripheral`](../../examples/bluetooth_peripheral/main.c). App ID `org.passport.bluetooth-peripheral`, version 0.1.4, Native ABI 2 with the optional HID extension. Upgrade OS to 1.0.50 before installation. Existing ABI 1, core ABI 2 and Lua apps retain their contracts.

## Hardware scope

ESP32-C3 provides Bluetooth LE, not Classic Bluetooth. Standard A2DP headphone audio, AVRCP and HFP calls cannot be implemented on this board's radio. This app does not route phone audio to the speaker or answer telephone calls. See [Espressif's Bluetooth capability matrix](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/bt-architecture/overview.html).

## iPhone setup

### Recommended: saved-gesture button mapping

User acceptance passed on iPhone 12 Pro Max/iOS 27 with OS 1.0.50/app 0.1.3: both swipe directions and OK pause/resume work after mapping the saved gestures. This is the recommended path for that tested setup. Other devices/apps require separate acceptance.

App 0.1.4 defaults to **iPhone gesture buttons**. Select it with OK, pair Passport Remote, then open iPhone Settings → Accessibility → Touch → AssistiveTouch → Devices → Passport Remote → Customize Additional Buttons. This is the connected-device button page, not the floating AssistiveTouch menu's single/double-tap settings.

- Press physical UP while configuring: bind **Button 2** to the saved upward-swipe gesture.
- Press physical DOWN: bind **Button 3** to the saved downward-swipe gesture.
- Bind **Button 1** to Single Tap; physical OK sends it.

The OS sends ordinary button press/release reports. It cannot select an iOS saved gesture by its name over BLE. The mapping is configured and stored on iPhone. Some saved gestures enter placement mode; if a gesture marker appears, use OK to try confirming the pointer position, or place it on the phone. Do not assume one-press automatic execution until verified on the target iOS/app. This mode does not automatically append a confirmation click, which could otherwise pause the video unexpectedly. Hold OK opens the app menu; double OK exits.

### Home and pairing

Version 0.1.4 has exactly two home entries: **iPhone gesture buttons** and **Forget pairing**. UP/DOWN selects, OK opens. Direct drag, wheel, pointer calibration, media remote and headphone information have been removed from the app.

Select gesture buttons to advertise **Passport Remote** for up to three minutes. Enable AssistiveTouch on iPhone and pair in Bluetooth settings or AssistiveTouch → Devices → Bluetooth Devices. Existing bonds and iPhone button assignments are preserved when updating the app. Keep the iPhone awake. See [Apple's pointer-device guide](https://support.apple.com/en-gb/111775).

**Forget pairing** requires a second OK confirmation. It clears OS-owned HID bonds; also forget Passport Remote on iPhone before pairing again. Bonds are device-wide and shared by Native HID apps, not app-private storage. At most three bonds and one concurrent connection are supported.

## Generic Native API

Include [`passport_native_hid.h`](../../sdk/native/include/passport_native_hid.h) and declare `bluetooth.hid`. This first extension is restricted to trusted Native ABI 2 apps; Lua cannot request the permission. It uses generic pointer/Consumer reports, not iPhone or video-provider logic in OS.

Core ABI 2 stays 80 bytes on C3. The optional extended host is 96 bytes. Check `host->abi==2` and `host->size>=sizeof(passport_native_host_hid_v2)` before casting; the application callbacks remain 20 bytes. An app compiled for the core 80-byte table continues to work with the larger host.

| Function | Contract |
|---|---|
| `hid_start(flags)` | Async start; 0 preserves bonds, 1 explicitly clears them before pairing; reject other flags and overlapping starts |
| `hid_state()` | 0 off, 1 starting, 2 advertising, 3 connected/pairing, 4 encrypted mouse subscription ready, 5 error, 6 stopping, 7 unavailable |
| `hid_report(1,data,4)` | Three button bits, signed relative X/Y/wheel; each axis -127…127 |
| `hid_report(2,data,2)` | Little-endian usage: 0 release, B5 next, B6 previous, CD play/pause, E2 mute, E9 volume up, EA volume down |
| `hid_stop()` | Async stop; new starts wait for teardown; no app memory or callback is retained |

Reports use an eight-entry copy queue and a 40 reports/second quota. Return 1 means accepted into the queue, not confirmation of a phone action. Unsupported reports, missing encryption/subscription, busy queue and quota excess return 0. Failure to transmit a queued report tears down the connection so held inputs cannot remain indefinitely active. Release every click/consumer action; avoid sending both halves of a double-click as single-clicks.

Only an active foreground owner starts the radio. NimBLE owns GATT, encryption, bonds and notifications; bounded worker tasks keep initialization and teardown outside button callbacks. A tiny reusable OS mailbox remains after first use. Other tasks, connections and protocol allocations must be reclaimed. Stop is fail-closed if the host cannot exit safely; restart the OS if state remains Error/Stopping. Pairing uses bonded encrypted Just Works with no display PIN or MITM claim.

## Build and publish

Use the normal `passport native build`, `pack`, `validate`, `install`, and `publish-index` commands. This app requires the built-in trusted publisher, just like other Native apps. Do not place private publisher keys or raw bond/connection logs in the package or repository. See [Native apps](native-apps.md), [Native services](native-services.md), and [package protocol](package-protocol.md).

The prototype DIS PnP identity uses Bluetooth-source internal/test vendor `0xFFFF`, product `0x0001`, revision `0x0100`; it does not claim a third-party assigned product identity. Suspend/resume blocks reports while suspended and releases held inputs on resume.

Keep iPhone awake during setup. The current HID Information flags do not advertise remote wake; a suspended host blocks reports until it exits suspend. USB `ble_debug` distinguishes this from missing encryption/subscription. Acceptance status: [Bluetooth checkpoint](../test-reports/bluetooth-peripheral.md).
