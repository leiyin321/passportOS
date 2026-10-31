<p align="right">
  <a href="system-updates.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Passport OS system updates

Current public distribution uses `https://leiyin321.github.io/passportOS/updates/latest.json`. The Store uses the same Pages project under `/apps/index.json`. Defaults and fork public keys are configured through [distribution](../../distribution/README.md); saved valid endpoint overrides take precedence. Signatures remain mandatory for HTTPS and private-LAN HTTP alike.

## Installation and update boundary

The first Passport OS installation still requires a USB firmware flash. Once a
device is running a Passport OS build with the system updater, later releases
can be checked, downloaded, verified, and installed from **Settings → System
Update**. The updater writes only the inactive OTA application slot and never
formats the package or app-data partitions.

## Release server layout

Host `latest.json` and its sibling application image on one public HTTPS origin:

```text
https://updates.example.com/passport/stable/latest.json
https://updates.example.com/passport/stable/passport-os-1.2.3.bin
```

TLS certificates are validated by ESP-IDF's certificate bundle. The host must
be configured as the device's system update source, and every firmware manifest
must also verify with the Passport OS built-in RSA-2048 release key. A catalog
publisher key or TLS alone is not enough to authorize firmware.

## Produce a signed release

Use the **app-only** image from a verified firmware archive, not the merged
initial-install image. The private key is the built-in system release key and
must stay offline/secret; this repository's ignored `local/publisher-private.pem`
is only for the existing development device.

To make Settings → About show the same firmware version as the signed manifest,
set `PASSPORT_OS_VERSION` for the release build and match it to `--version`:

```sh
PASSPORT_OS_VERSION=1.2.3 ./tools/validate.sh
```

```sh
passport firmware-release \
  build/firmware/<archive-sha>/FoloToy-AI-Passport.bin \
  --version 1.2.3 --build 2026102501 \
  --key local/publisher-private.pem \
  --public sdk/publisher-public.pem \
  --output build/system-updates/stable
```

`--build` is a strictly increasing integer (1..2,147,483,647); do not reuse or
decrease it. Publish the generated `latest.json` and `.bin` together at the same
HTTPS directory. The manifest is signed over this exact UTF-8 representation:

```text
passport-firmware/1
chip=esp32c3
version=1.2.3
build=2026102501
size=<application image bytes>
sha256=<lowercase image SHA-256>
image=passport-os-1.2.3.bin
```

The manifest format is `passport.firmware/1`, carries those eight fields plus
the 256-byte RSA signature encoded as lowercase hex, and is limited to 4 KiB.
The image must be 4 KiB–2.5 MiB and is fetched from the manifest's sibling path.
The updater rejects unknown/duplicate fields, non-monotonic build numbers,
invalid signatures, size/hash mismatches, invalid ESP images, and untrusted
hosts.

## Configure and install on a device

Configure the HTTPS manifest URL once over the trusted USB developer channel:

```sh
passport device update-source \
  https://updates.example.com/passport/stable/latest.json
```

Then use the physical keys:

1. Open **Settings → System Update**, press OK to check.
2. When a signed release is available, press OK to download it to the inactive
   slot. The UI shows both transferred/total bytes and a 10-step percentage progress bar.
3. After RSA signature, exact length, SHA-256, and ESP image validation pass,
   press OK again to install and restart.

On first boot, Passport OS runs its existing initialization/storage self-test
while the candidate is pending verification. It records the accepted release
sequence and marks the slot valid only after the self-test succeeds. A failed
self-test or reset before confirmation lets the ESP-IDF bootloader return to the
previous slot. A transfer interruption leaves the current slot selected; retry
the download from Settings.

The update source is system-owned. MiniApps cannot configure it, request an OTA
write, or select a boot partition. Do not publish an image built with a different
partition table, chip target, or incompatible bootloader. Initial migration from
vendor firmware remains a USB flashing operation documented in
[firmware release and recovery](firmware-release.md).

## Serve a local test release from macOS

Publish a signed image into `build/system-updates/local-update/`, then run the
loopback server and HTTPS Quick Tunnel in separate terminals:

```sh
PYTHONPATH=sdk python3 -m passport firmware-release \
  build/firmware/<archive-sha>/FoloToy-AI-Passport.bin \
  --version 1.0.0 --build 2026102502 \
  --key local/publisher-private.pem --public sdk/publisher-public.pem \
  --output build/system-updates/local-update
python3 tools/serve_system_updates.py --port 8765
cloudflared tunnel --url http://127.0.0.1:8765 --no-autoupdate
```

Configure the generated `https://<random>.trycloudflare.com/latest.json` URL
with `passport device update-source`. Keep both processes running during the
test. The tunnel gives the device a publicly trusted TLS endpoint while the Mac
serves the release files; the temporary URL changes when the tunnel restarts.
This is a development setup, not a production availability guarantee.

For a device and Mac on the same trusted Wi-Fi network, prefer the direct LAN
test path to avoid a public tunnel. Bind the test server to the Mac's private
LAN address and configure that exact address and port as the update source:

```sh
python3 tools/serve_system_updates.py --host 192.168.1.10 --port 8765
passport device update-source http://192.168.1.10:8765/latest.json
```

The firmware accepts this HTTP exception only for an explicitly configured
RFC1918 address and matching port. The release manifest and image still require
the built-in RSA signature and SHA-256 checks. Use HTTPS for public update
servers; the LAN path is for local development/testing only.

Firmware manifest requests use a 15-second timeout and image streaming uses a
60-second per-read timeout to tolerate a slow HTTPS tunnel. App requests retain
the 3-second timeout. The overall firmware transfer remains bounded to 15
minutes.

## Acceptance status

Host tests verify release signing, manifest tamper detection, duplicate-field
rejection, and filename/size bounds. Full `./tools/validate.sh` passed for
version 1.0.5, and the SDK signed and re-verified the published manifest/image.
The verified archive is
`build/firmware/c970187ec42e4ea05f992bcb94d309018e877d62386e1cd08b1eaf77988b5626/`.
The app image is 1,840,224 bytes, leaving 781,216 bytes (29.80%) in its OTA
slot; the initial-flash merged image is 1,971,296 bytes. App SHA-256 is
`5f4b470eee767c6a796dbc986a89134fb22ed242ed1db5179998c376eba794b7`; merged
image/archive SHA-256 is
`c970187ec42e4ea05f992bcb94d309018e877d62386e1cd08b1eaf77988b5626`.

On 2026-10-25, the verified app image was first installed over USB at `0x20000`;
the partition table, NVS, packages, and app data were not written. The Mac
server at `http://192.168.1.10:8765` then served signed release 1.0.5/build
2026102507 directly over the LAN. Device logs confirm the full 1,840,224-byte
image request, boot into `ota_1`, and successful startup self-test/slot
confirmation (`OTA self-test valid rc=0`). The user confirmed the update
succeeded. The server remains running and the device's configured source points
to this LAN endpoint.

The Quick Tunnel path was rejected for this test: the ESP32-C3 TLS read failed
at 223,312 bytes (`-0x7100`) even though the Mac origin had served the complete
file. The development-only LAN HTTP exception is limited to an explicitly
configured RFC1918 IP/port. RSA signature, image length, SHA-256, and ESP image
checks remain required. At Wi-Fi startup the device reported 86,728 bytes free
heap and an 86,672-byte minimum; OTA peak RAM was not instrumented. Interrupted
transfer recovery, forced rollback, and app-private-data contents still need
separate acceptance tests.

## 1.0.8 language release (2026-10-25)

- Added persistent English/Simplified Chinese system language selection and the read-only `passport.system.language()` MiniApp API.
- Localized system navigation, setting names/values/details, status/footer instructions, setup and permission prompts, Store/install/update notices, and runtime errors. App-authored names/content remain app-owned.
- Added the licensed Source Han Sans SC 16 px subset for missing system UI glyphs. Its generated C source is 110,986 bytes; linked glyph data uses 16,579 bytes of Flash. The built-in LVGL CJK font is chained as fallback.
- Build `2026102510`, ESP-IDF 5.5.3; app image 2,061,808 bytes in the 2,621,440-byte OTA partition (559,632 bytes / 21.34% free). Merged initial-flash image 2,192,880 bytes. Signed manifest/image verification passed; SHA-256 `04a0334c46e55d69171939f9e98d3578e98220fcd79a59e1502527d6aa4f0cfe`.
- Published at `http://192.168.1.10:8765/latest.json`; manifest and image both returned HTTP 200. Device-side OTA installation and visual language-switch acceptance remain pending.
- `./tools/validate.sh --static` and `./tools/passport/test.sh` passed, including 18 Lua runtime tests and the MiniApp language API test. Full ESP-IDF 5.5.3 build, merged image, and firmware layout verification passed. Device RAM peak while rendering Chinese text was not measured.

## 1.0.9 OK button fix (2026-10-25)

- OK press-down no longer executes the page action. A short or double click executes once after classification; a long OK press executes only the long action. UP/DOWN remain immediate. MiniApps receive the OK down/up events together after a short tap, and long OK returns Home without a prior OK callback.
- Added a host regression test for OK press/click/double/long/release classification. `./tools/passport/test.sh` passed; full ESP-IDF 5.5.3 build and firmware layout verification passed.
- Signed build `2026102511`, version `1.0.9`; app image 2,061,872 bytes, 559,568 bytes (21.35%) free in OTA slot. Merged image 2,192,944 bytes. SHA-256 `4ab16642c806849d205caa5d4d9be440c349a5cb9c7de845b8f22ff28bc3043d`. Published to the local update service; device-side button and OTA acceptance are pending.

## 1.0.10 button event timing (2026-10-25)

- BSP now emits `PRESS` immediately, emits `CLICK` at release only when no long event was sent, and emits `RELEASE` after it. A held key emits `LONG` at the 1,800 ms threshold and no `CLICK`; its later release still emits `RELEASE`. Rapid taps are separate short clicks, without the driver's delayed double-click merge.
- System OK page actions execute on the release-classified short-click event. UP/DOWN stay immediate. MiniApp OK short taps receive paired down/up callbacks at release; long OK exits before either short callback. Public SDK docs and BSP tests specify and verify this order.
- Signed build `2026102512`, version `1.0.10`; app image 2,062,128 bytes, 559,312 bytes (21.33%) free in OTA slot; merged image 2,193,200 bytes. SHA-256 `889c3256966b7efa46dc7adacb04ecf64ae56f792f2ad1340a28a1b743aeed0a`. Manifest and image were published and verified on the local update service. Device-side long/short press behavior and OTA installation remain to be confirmed on hardware.


## 1.0.6 release notes (2026-10-25)

- When System Update finds an installed or older signed build, it displays “Passport OS is up to date” and does not enter the download/install flow.
- The download page retains transferred/total bytes and adds a 10-step percentage bar plus the numeric percentage.
- Release `1.0.6` / build `2026102508`; app image 1,840,320 bytes; SHA-256 `3ebcd4fca52e5d5fd248cd30c2a56393532fd65a48cbb501f35944365913dd76`. The local signature and manifest/image verification passed. Update service: `http://192.168.1.10:8765/latest.json`; device installation acceptance is pending.


## 1.0.7 release notes (2026-10-25)

- Settings → About displays the firmware version embedded in the image. Signed manifest version `1.0.7` matches the `PASSPORT_OS_VERSION=1.0.7` build setting.
- Release build `2026102509`; app image 1,840,384 bytes; SHA-256 `2da846bd6315eafa67748db4e0eecc1c59d1e291e21475bd6584e07924c22cd7`. The local service serves `http://192.168.1.10:8765/latest.json`; manifest/image verification passed. The user confirms both the OTA system-update flow and the About firmware version display are verified on-device.

## 1.0.11 button gestures and Store versions (2026-10-25)

- All three buttons expose immediate press/release edges and classified single, double, and long gestures. A single OK confirms; double OK returns Home and suppresses the two constituent single callbacks; long OK is available to MiniApps, including press-to-talk down/up handling.
- Store and Library cards display app versions. Selecting a Store app compares its installed version with the catalog: missing/older apps download and update, while current or newer installs show an up-to-date prompt; the next OK opens the app. After installation, OK opens the new app directly without repeating the download.
- Host regression tests cover gesture classification, MiniApp callbacks, push-to-talk, and strict semantic-version comparisons. Full repository validation passed.
- Version UI uses 320 bytes of static cache/state and no heap allocation; comparison uses 24 bytes of stack for numeric components. ELF `.dram0.bss` is 61,440 bytes. Runtime peak free heap was not measured on hardware.
- Signed build `2026102514`, version `1.0.11`; app image 2,063,888 bytes, leaving 557,552 bytes (21.27%) in the 2,621,440-byte OTA slot; merged image 2,194,960 bytes. App SHA-256 `5c7d704229781e6bddad69269035215e47aa14097bfdc91da89cdacf509961ba`; merged image SHA-256 `b21d6936204ac2a6b7e503e2303e90681454cd6ba29b106a331e9b9188eb6f54`.
- Published on the local update service at `http://192.168.1.10:8765/latest.json`; signed manifest and image verification passed and both endpoints returned HTTP 200. Hardware OTA boot and visual Store/Library acceptance remain pending.

## 1.0.12 Chinese double-click hint glyphs (2026-10-25)

- Added the missing double-click glyphs to the system CJK font subset so the Home-return hint renders on the device.
- Added host font coverage checks that verify both Unicode mappings and nonempty glyph bitmaps.
- The subset now covers 132 system code points; its generated C source is 112,548 bytes and linked font data is 16,817 bytes of Flash, 238 bytes more than the previous subset.
- Signed build `2026102515`, version `1.0.12`; app image 2,064,128 bytes, leaving 557,312 bytes (21.26%) in the OTA slot; merged image 2,195,200 bytes. App SHA-256 `5589d4f8369c38c6c0d4ad872eaa29745d26b49de3cda152cb0652c2124c4be2`; merged SHA-256 `cb78fd52ed60d4ab2302fc3737c3cbc54c61a8aba69439bbf58835115adbd340`.
- Published to the local update service and signature/hash verification passed. Device-side OTA and visual confirmation of the Chinese hint remain pending.

## 1.0.13 immediate UP/DOWN taps (2026-10-25)

- UP/DOWN no longer enter double-tap classification. Every short physical release emits one immediate `CLICK`, so rapid repeated taps remain separate movement actions. OK retains its 180 ms single/double classification; double OK still returns Home and suppresses both short events.
- MiniApps receive UP/DOWN short callbacks individually. `on_button_double` remains reserved for compatibility and is no longer delivered; apps that need direction-key double actions can time successive short callbacks.
- BSP regression tests cover rapid UP and DOWN taps, driver double callbacks being ignored, OK double suppression, long press, and physical release. Full repository validation and ESP-IDF firmware build passed.
- Signed build `2026102516`, version `1.0.13`; app image 2,064,176 bytes, leaving 557,264 bytes (21.25%) in the 2,621,440-byte OTA slot; merged image 2,195,248 bytes. App SHA-256 `8c30456a4f96ed26dc3c67277b6e31c3e455ca50f3a196fa19b58651444d7ebb`; merged SHA-256 `ef48b92b11f95289497582282f3ae2939ae1344109b2e478ae4bf80e8a66de56`.
- Published to the local update service; signed manifest/image verification passed. Device OTA and physical repeated-navigation feel remain to be checked.

## 1.0.17 XiaoZhi MiniApp release (2026-10-25)

- Built the XiaoZhi MiniApp and generic HTTPS/WSS/Opus OS changes with `PASSPORT_OS_VERSION=1.0.17`; firmware build/layout validation passed.
- Signed and published build `2026102520` to the running Mac update service at `http://192.168.1.10:8765/latest.json`.
- App image: 2,278,640 bytes of 2,621,440 (342,800 bytes / 13.1% free); SHA-256 `799c6daacfa47a5197d4ed9422052b76783fff2e06294a1076af186d8bbffe8e`.
- Merged initial-install image: 2,409,712 bytes; SHA-256 `5021f4fb6a7a6b04ce1682c8d7213b6754e9f3b0ac10c713c356e6ce61cbb43a`.
- The live server returned HTTP 200 for the signed manifest and all 2,278,640 image bytes; SDK RSA signature, exact size, and SHA-256 verification passed. Device update/install was not initiated.

## 1.0.18 OTA startup stack fix (2026-10-25)

- Fixed the `passport_http` worker stack overflow that caused 1.0.17 to roll back during startup. Large network jobs are heap-owned and the FreeRTOS queue carries only a pointer.
- Full host and ESP-IDF 5.5.3 validation passed. App image: 2,278,416 bytes; 343,024 bytes (13.08%) remain in the 2.5 MiB OTA slot. Merged image: 2,409,488 bytes; SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`.
- Signed release `1.0.18` / build `2026102521` is published by the running local Mac service at `http://192.168.1.10:8765/latest.json`. App SHA-256: `ecd9e121907b68b187deb1eeda97d5d28d5c7f602272793ef3fcbbb64d4cf99e`. The live manifest and complete image size/hash were verified.
- Device evidence and matching panic symbolication are documented in `docs/test-reports/ota-1.0.17-rollback.md`. Release 1.0.17's automatic rollback to 1.0.16 passed. The user then installed 1.0.18: `ota_1` booted, storage self-test passed (`OTA self-test valid rc=0`), Wi-Fi came online, and About showed 1.0.18. Startup minimum free heap was 85,264 bytes; network task stack high-water mark was 3,884 bytes.

## 1.0.19 App Library card stack fix (2026-10-25)

- App Library card rendering no longer verifies/parses each installed package to obtain its version. Version metadata is prepared when the list refreshes and then read from cache by the renderer.
- Full host and ESP-IDF 5.5.3 validation passed. App image: 2,278,416 bytes; 343,024 bytes (13.08%) remain in the OTA slot. Merged image SHA-256: `c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`.
- Signed release `1.0.19` / build `2026102522` is published at the running local update service `http://192.168.1.10:8765/latest.json`; manifest and complete image signature/size/hash were verified. App SHA-256: `2b340c94f700ac2691ad03f1a5df61a62d1dc0c55749d61999e6358f84262f2e`.
- Matching panic evidence and the reproduction/fix report are in `docs/test-reports/app-library-card-crash.md`. Device retest after installing 1.0.19 remains pending.

## 1.0.20 system appearance and MiniApp API (2026-10-26)

- Added a persistent Dark/Light appearance option in Settings. MiniApps can read the selected value through read-only `passport.system.display_mode()`; the OS does not impose app colors. `passport.text(value, rgb24)` allows app-owned text color. XiaoZhi 0.2.2 uses its own palette and switches to a white background with dark text in Light mode.
- Signed build `2026102601`, version `1.0.20`, published to the local Mac update service at `http://192.168.1.10:8765/latest.json`. App image: 2,280,576 bytes; OTA partition headroom: 340,864 bytes (13.0%); merged image: 2,411,648 bytes.
- App image SHA-256: `d33d67b9d8bd80ce0c9ae9a61f838d2addbcf97f8f351795b820349f564e0998`; merged image SHA-256: `91253dd4ebdb7250316b488babf975346ab8b1a91444d61b9fa2c853df928a45`; ELF SHA-256: `987927e90863dc8990fe05b64acb9909fbd900211cfb979534653fe3c82fba81`.
- Full ESP-IDF 5.5.3 build and firmware validation passed. Static/host gate passed after the change: 23 runtime tests, 13 package tests, 12 compiler tests, plus repository, BSP, display, font, OTA, and API checks. OTA manifest/image signature and hashes were verified from the running local service. Hardware appearance switching remains pending.

## 1.0.21 MiniApp HTTPS diagnostics (2026-10-26)

- Added sanitized diagnostics for MiniApp HTTPS submission and worker failures. Logs distinguish Wi-Fi/WebSocket/worker queue rejection from policy, allocation, TLS/open, body write, response-header, and response-read failures. No URL, request body, device identity, or credential headers are logged.
- Signed build `2026102602`, version `1.0.21`, published to the local update service. App image: 2,281,344 bytes; OTA slot headroom: 340,096 bytes (13.0%); merged image: 2,412,416 bytes.
- App image SHA-256: `1d281b70a4ecebf63b0a036d3238561a9954fc7a9935837a7379e9c4b498da15`; merged image SHA-256: `fabcb93fb2040fc7c47b52d58dca9a955f9823d21818c5217678223f82722bc6`; ELF SHA-256: `a11f1b8872cf127a0675edb771104cfa742896240c19b7c602f47b7c24e14d6f`.
- Full ESP-IDF 5.5.3 and host gate passed (runtime 24/24). The live manifest and image were fetched from the local update service and RSA signature/size/hash verified. After installing this OTA, reproduce the XiaoZhi service check and capture `passport_http` logs to determine the remaining cause.

## 1.0.22 review and reliability update

Signed build `2026102603` is live on the Mac service. App image 2,283,280 B; OTA
headroom 338,160 B (12.9%); merged image 2,414,352 B. App SHA-256:
`fb59175bb4f6c07fe0cfd52d11864e150dba5a94cda78c1725d71a75021e2d8d`. Full build/host gate and archive verification passed; the live
manifest and complete image signature/size/hash passed over a direct LAN fetch.
Device installation/acceptance is pending. See the [review](../development/passport-os-review.md)
for network-memory, HTTP framing, audio, JSON, alarm and SDK changes.

Publication now atomically replaces files and commits latest.json last. Existing
version image bytes cannot be changed; use a new version and increasing build.
Do not run competing publisher processes against the same release directory.

## 1.0.23 final review release

Signed build `2026102604` supersedes the staged 1.0.22 image. Final review
corrected GET redirect buffer ownership and prevented empty-body POST redirects.
The actual network-worker harness now covers constrained allocation, redirect
responses, chunk trailers, partial writes, error cleanup and 500 reclaimed requests
under AddressSanitizer/UBSan; socket/RTOS behavior remains stubbed.

App image: 2,283,296 B; OTA headroom: 338,144 B (12.9%); merged: 2,414,368 B.
App SHA-256: `7e6adede041783bcc37924e56201bc0c8756a335f0143aab6b86e6a065750f86`.
Full firmware gate/archive verification and live LAN signature/size/hash checks
passed. Install 1.0.23 through Settings for hardware acceptance. See the
[review](../development/passport-os-review.md) for the full artifact identity.

## 1.0.29 XiaoZhi audio worker allocation fix

Signed build `2026102610` is published at the running Mac update service
(`http://192.168.1.10:8765/latest.json`). The audio task now obtains its 9,216-byte
stack before the stream queues, avoiding the largest-block reduction that made
task creation fail after WSS setup. Full ESP-IDF 5.5.3 and host gates pass.

App image: 2,289,120 B; OTA headroom: 332,320 B (12.68%); merged image:
2,420,192 B. App SHA-256:
`630446dbeb6bdb0d186a3aed53418386ee00f5bf0d5fe0379a7e346a2b178a63`; full
archive/image SHA-256:
`4a758ee3d9e6b8e11c430cdfba3d2552b7ff2f261ca5cecae11a36e9a6f61944`; ELF
SHA-256: `a80313c29c73b5e5bcd67b2b200b3d71439840761c6d9a771135dcfe050518ad`.
The served signed manifest and complete image were downloaded and verified.
Device codec high-water, RAM peak, successful PTT and exit cleanup remain to be
measured after the user installs the update in Settings.

## 1.0.30 XiaoZhi audio preallocation

The 1.0.29 device reproduced a task-stack allocation failure after WSS setup:
22,428 B free, largest block 7,680 B, while the worker requested 9,216 B.
Version 1.0.30 prepares the idle, non-capturing stream task when opening an app
that declares `audio.stream`, before its network callbacks start. It retains
the task between push-to-talk sessions and reclaims it on app exit. The signed
OS release and XiaoZhi 0.2.8 are published locally; verify actual codec memory,
stack high-water, speech and teardown after installing them.

Signed release `2026102611` is live at `http://192.168.1.10:8765/latest.json`.
App image: 2,290,000 B; OTA headroom: 331,440 B (12.64%); merged image:
2,421,072 B. App SHA-256:
`0b8af29458b0e8c8f9e33066c21a05b7461cd348f87525690f8218394bb4921a`; full
image/archive SHA-256:
`c1ad1929d76b4fbae290ec0ae679cc0386b8c8f00beed7cf65f06c32f6d8e90c`; ELF
SHA-256: `4943a6aa92fc263e9116b0a6b67a783fe727de0c0a8be480ce8fa07787b0b43d`.
The downloaded OTA manifest and image passed signature/size/hash verification.
The Store catalog now offers XiaoZhi 0.2.8; live package SHA-256 is
`0a05b02fb0d6b07b13ec452a531d5ab4320ddc79a5af749eef0466281ed3bef2`.

## 1.0.31 audio API compatibility follow-up

Retained the pre-network worker reservation and made it usable by ordinary
microphone capture as well as Opus streaming. Signed build `2026102612`
supersedes 1.0.30 at the local update service. App image: 2,289,984 B; OTA
headroom: 331,456 B (12.64%); merged image: 2,421,056 B. App SHA-256:
`eca67a0597bf1011cf4cece4d7949176e2bd8024a29bfd4414592297a5b6c277`; full
archive/image SHA-256:
`efa58799b17ab088d892c749af76dfba4050bfc6d7a9c29358268f9485e300d6`; ELF
SHA-256: `f525b78ef221570dff2543e35da44216b64cc825af62c5014076ec16c6dc6d15`.
The OTA manifest and image passed live signature/size/hash verification.

## 1.0.32 TLS-safe audio task stack

On 1.0.31, WSS certificate signature verification failed with mbedTLS `0x4290`
and the handshake then returned `-0x3000`. The pre-reserved 9 KiB stack occupied
ordinary heap needed by RSA verification. Version 1.0.32 allocates the task stack
from the ESP32-C3 Retention RAM heap using `xTaskCreateWithCaps` and frees it
with `vTaskDeleteWithCaps`; its TCB remains internal. Full ESP-IDF and host
gates pass. Signed build `2026102613` is live on the local OTA service.

App image: 2,290,112 B; OTA headroom: 331,328 B (12.64%); merged image:
2,421,184 B. App SHA-256:
`e50eb345f1fbabda1fcba303d52aa4892584757ad3202b4f51e50b977efe244b`; full
archive/image SHA-256:
`f365f8c0dbf45b616200c332d0acd8c49cab850b1a0881e36cc20da8b84b7b6d`; ELF
SHA-256: `0ce5b949821e3e74cb3f3dc07ef89d921f17cb206298b16ff7f3cd93d27136ae`.
The live signed manifest and image passed download and hash verification.
XiaoZhi 0.2.8 remains in the Store. Device WSS/audio acceptance is pending.

## 1.0.33 deferred audio allocation

The user reproduced the WSS failure on 1.0.32. Although the stack was allocated
from Retention RAM, that region remains eligible for mbedTLS's
`MALLOC_CAP_INTERNAL` allocations and competes for the same internal RAM. OS
1.0.33 no longer allocates the worker at app launch or microphone consent; it
creates the 9,216-byte Retention-capable stack only when audio capture actually
starts. Runtime heap telemetry separately reports Internal and Retention
totals and largest blocks.

ESP-IDF 5.5.3 full validation passed: repository and host tests (33/33 runtime,
13/13 packages, 12/12 asset compiler, plus BSP/network/OTA checks), firmware
layout verification, and merged-image verification. App image: 2,290,064 B in
the 2,621,440 B OTA slot (331,376 B free, 12.64%); merged initial-flash image:
2,421,136 B. App SHA-256:
`94335b5472da45f44963fc179520718a8d911666cc6935514f20948cb7dc10f6`; merged
image/archive SHA-256:
`80e51f89fee7b5506682f9c33bd8ed7d7f345a54e46608139a889154eb47f48f`; ELF
SHA-256: `3b13c485917542593e48ead7f68eaf6ac83fcf0319d23c0b50fb747dfd0a2e38`.
Signed build `2026102614` is published at
`http://192.168.1.10:8765/latest.json`; the live manifest and full image were
downloaded and verified for RSA signature, size, and SHA-256. Device acceptance
still requires installation, WSS hello, PTT/TTS, and measured RAM/reclamation.
