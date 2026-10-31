[简体中文](quickstart.zh_CN.md)

# Developer quickstart

See [tasks](../tasks.md) and the milestone reports for tested behavior and remaining checks.
Image/sprite development is covered by the [asset guide](assets.md).
Requirements: Python 3.10+, OpenSSL CLI, a host C compiler; ESP-IDF 5.5.3 for firmware builds.
No account, developer cloud backend or PC process is required to run cached offline applications.

```sh
python3 -m venv .venv
.venv/bin/pip install -e ./sdk
passport --help
passport validate examples/packages/counter.pap --public sdk/publisher-public.pem
```

Create a directory with `manifest.json` and `main.lua` or `ui.json`. Copy an example and choose
an original reverse-DNS ID. Declare only needed capabilities and select `display.mode` (`standard` by default, `fullscreen` for full-screen games). Apps can query `passport.display.get_viewport()` in Lua. Check the [package protocol](package-protocol.md) and [display API](display-layout.md).
Do not generate a new publisher key for every app update: data identity incorporates publisher key.

```sh
mkdir -p local dist
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out local/my-private.pem
openssl pkey -in local/my-private.pem -pubout -out local/my-public.pem
passport pack examples/counter --key local/my-private.pem --public local/my-public.pem -o dist/my-counter.pap
passport validate dist/my-counter.pap --public local/my-public.pem
```

The device embeds `sdk/publisher-public.pem` and supports up to4 additional enrolled RSA2048 publishers.
Use `passport trust --public local/my-public.pem`, compare the displayed key ID and confirm on the
device. Enrollment needs no OS rebuild; denial leaves the key untrusted. An enrolled key still cannot
claim an ID owned by another publisher, so use `passport init local/my-app --id org.yourname.app`.
The built-in private key is deliberately absent from Git; signed examples work without it.
Never publish private keys or device Wi-Fi configuration.

```sh
passport wifi --ssid 'YOUR_2_4_GHZ_SSID'  # hidden password prompt, USB provisioning
passport install examples/packages/counter.pap --public sdk/publisher-public.pem
passport device list
passport device launch org.passport.counter
passport device view
passport device home
passport device offload org.passport.counter
# Reinstall the same signed app: saved data remains.
passport device remove org.passport.counter  # explicit DELETE confirmation
```

Alternatively, on the device open **Settings** → OK, join the displayed
`Passport-XXXX` WPA2 network using the one-session key shown on screen, then open
`http://192.168.4.1` on your phone. Enter the target 2.4 GHz SSID and password.
The local setup page saves credentials to NVS and closes its temporary AP after
Wi-Fi obtains an IP address. See the [display and phone setup specification](display-layout.md).

USB is a trusted physical developer/admin channel, not an application capability. Binary installs use
READY + 128-byte chunks + ACK offset + final verified install result. Never send raw binaries without
this flow control. Button events and workers communicate through a bounded queue.

Publish a static catalog; no application server is needed for declarative or offline Lua apps:

```sh
passport publish-index dist --public local/my-public.pem --base-url https://YOUR_HOST/apps -o dist/index.json
passport serve dist --host 0.0.0.0 --port 8040
passport device service 192.168.1.10 8040 8041
```

On the device approve the exact displayed Mac IP/HTTP/WS ports. Library OK opens App actions (Open/Set as default/Offload/Pin/Delete with confirmation). Only one default app is stored at a time; it launches on boot, and double-OK returns to Home. Single OK confirms; long OK remains available to the app. Removing or offloading the selected app clears the default.
Open App Store, press OK to fetch the
catalog and select an app. Store and Library cards show each app version and PAP package size in KiB (1 KiB = 1,024 bytes). Store size comes from the catalog; Library size is the actual cached file length. Missing/offloaded package sizes display `-- KiB`. This excludes persistent app data and filesystem overhead. Settings → About shows the running OS application image size in bytes, excluding the OTA backup slot and other partitions. OK checks the installed version: a missing or older app is installed/updated; when already current, the OS prompts “Already up to date. OK: open app.” Press OK again to launch it. After installation, “Installed. OK: open app.” also launches directly without another download. Local service approval lasts for the boot/session configuration and is
not a grant to scan the LAN. The default developer service uses Mac ports 8040 (catalog) / 8041 (echo).
The catalog verifies package SHA256 as well as package publisher signatures. `passport serve` binds to
localhost by default; `--host 0.0.0.0` explicitly exposes the selected directory to the development LAN.
Serve only a directory containing public app artifacts, never keys or private files. This simple server
has no authentication and is for a trusted development network only.

```sh
.venv/bin/pip install -r server/dev/requirements.txt
.venv/bin/python server/dev/echo.py --port 8041
./simulator/build.sh
python3 simulator/serve.py --port 8050
# Visit http://127.0.0.1:8050; arrows and Enter map to physical buttons.
python3 tests/passport/test_packages.py
python3 tests/passport/test_runtime.py
```

Simulator runs the same C runtime and Lua engine. Host pointer size, clock, font/rendering, persistent
storage and mock network/audio differ. Simulator PASS is not hardware PASS. Use the independent
owned WS server for real network tests; WSS accepts `--cert`/`--key` for a server you operate.
No third-party public echo endpoint is required.

Hardware acceptance is opt-in by the developer, after authorizing the target device:
`python tests/passport/hardware.py --suite storage` (also apps/store/stress/reconnect).
It does not flash. Read logs under ignored `local/`, and keep them private.
Firmware build/recovery instructions are in the release guide when available; never flash app.bin at 0x0.

For multiple app publishers, repeat `--public` once for each package publisher when generating the catalog.
Each app package is independently signature-verified; a catalog entry or HTTPS URL does not establish
publisher trust. For a public static catalog use `passport device catalog https://YOUR_HOST/apps/index.json`.
The configured HTTPS host is the system Store allowlist; every redirect is reauthorized.

See the [SDK and CLI reference](sdk-cli.md) for all commands and the [declarative UI/1 contract](declarative-ui.md)
for `ui.json` limits and supported actions.

## Native ELF

Firmware 1.0.34 adds `passport init DIR --id org.example.native --runtime native-elf`, `passport native build DIR`, and `passport native inspect DIR/app.app.elf`. Signing, installation and launch reuse the existing tools; Native packages require the built-in OS publisher. See [Native application development](native-apps.md) for the ABI, limits and example.

Settings → Storage provides a whole-Flash usage bar (blue: running OS image, purple: OTA reservation, amber: apps/cache, green: data reservation, silver: system/overhead, gray: app-available space), OS/OTA reservation summary, package breakdown, persistent NVS entries, and per-app details. UP/DOWN cycles through these pages; OK refreshes a background snapshot. Per-app data allocation is an estimate of live NVS entries × 32 bytes, excluding page/namespace and garbage-collection overhead; it is not payload length. Package file lengths exclude filesystem overhead. App storage availability is SPIFFS availability, not unused OTA space. Refresh is read-only and does not remove packages or data.

The overview bar uses detected physical Flash capacity and the actual partition table, not SPIFFS capacity. OTA reservation includes all app-slot bytes outside the running image, including current-slot padding. Data reservation is the complete app-data partition, not a claim that every byte contains live data. System/overhead includes boot/control/settings regions, alignment gaps and SPIFFS capacity unavailable to files. These six byte categories sum exactly to physical Flash. Displayed KiB values are independently rounded down; only gray space is available to app files. Current/previous package breakdown remains on the next page.

The bar and category rows share that left-to-right order, with app-available space last at the far right. Each row has a matching color swatch; swatch spacing follows the active font line height in both languages.

Native ELF is the primary app path. `passport init` now defaults to Native; use `--runtime lua` for the explicit Lua workflows above. See the [public/fork distribution guide](../../distribution/README.md).
