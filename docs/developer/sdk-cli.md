[简体中文](sdk-cli.zh_CN.md)

# Passport MiniApp SDK and CLI

Install the editable SDK from the repository root; `pyserial` is required for USB commands. OpenSSL is used for RSA signing and verification. Image tools are optional and installed with `.[assets]`.

```sh
python3 -m venv .venv
.venv/bin/pip install -e ./sdk
.venv/bin/pip install -e './sdk[assets]'  # only when compiling PNG/SVG assets
passport --help
passport api > local/api-v1.json
```

The CLI defaults to `/dev/cu.usbmodem101` for its USB commands in this development setup. On another port or OS, pass `--port PORT` to `install`, `trust`, `wifi`, or `device`. The CLI opens the port for the duration of each command; do not run two USB commands at once.

## Command reference

| Command | Purpose |
| --- | --- |
| `passport init DIR --id org.example.app [--runtime lua|declarative|native-elf]` | Create a starter Manifest and entry point |
| `passport keygen --private FILE --public FILE` | Create a new RSA-2048 publisher key pair; refuses to overwrite files |
| `passport pack DIR --key PRIVATE --public PUBLIC -o FILE` | Validate source files, build declared image assets, sign and verify PAP1/PAP2 |
| `passport validate FILE --public PUBLIC` | Verify package lengths, signature, key identity, entry digest, Manifest and declarative UI preflight |
| `passport publish-index DIR --public KEY [--public KEY ...] --base-url HTTPS_URL -o INDEX` | Verify packages and generate the static catalog |
| `passport serve DIR [--host 127.0.0.1] [--port 8040]` | Serve a development directory over unauthenticated HTTP |
| `passport assets build DIR` / `inspect FILE` / `report FILE` | Compile and inspect PAI1 images and PAP2 asset budgets; see [asset guide](assets.md) |
| `passport trust --public PUBLIC [--port PORT]` | Send a publisher public key for on-device key-ID confirmation |
| `passport install PACKAGE --public PUBLIC [--port PORT]` | Send a verified package over the bounded USB READY/ACK protocol |
| `passport device OPERATION ... [--port PORT]` | Invoke a supported system developer command; see operation list below |
| `passport wifi --ssid SSID [--port PORT]` | Send Wi-Fi credentials over USB with a hidden password prompt |

`passport device` operations are `list`, `metrics`, `home`, `launch ID`, `offload ID`, `pin ID on|off`, `remove ID`, `view`, `reconnect`, `service IP HTTP_PORT WS_PORT`, `catalog URL`, `update-source URL`, `screen on|off`, `icons`, `cancel_cycle`, and `data_hash ID KEY`. `remove` requires typing `DELETE`; it deletes the app's private data as well as its package. USB is a trusted developer channel and is not available to MiniApps.

## Publisher identity and package lifecycle

Generate one long-lived publisher key for an app identity and keep its private part outside Git, for example under ignored `local/`. Package identity and private-data ownership are the pair `(publisher_key_id, app id)`. Keep the key when updating an app. A new publisher cannot take over an ID already registered by another key, including an offloaded app. The device trusts the built-in publisher and supports up to four additional RSA-2048 publisher keys; `passport trust` prints the key ID, but a person must approve that ID on the device.

`passport pack` produces deterministic signed packages for unchanged inputs. PAP1 carries one entry; PAP2 carries the entry and compiled assets. The signature covers the complete package content. `passport validate` does not execute Lua and does not guarantee visual correctness. Run the simulator/runtime tests and test on the target device before release.

## Static catalog and local development server

The device accepts at most eight entries in `passport.catalog/1`. Generate an index from `.pap` files with `passport publish-index`; the CLI verifies each package against one of the repeated `--public` publisher keys and rejects duplicate app IDs. The generated contract is [`catalog-v1.schema.json`](../../protocol/catalog-v1.schema.json). Build the App Store around multiple catalog pages before attempting a larger public catalog; the current device limit is eight entries.

The index is not independently signed. It is discovery metadata served from the configured HTTPS origin; every downloaded package is separately checked against its catalog SHA-256 and publisher signature. A catalog key list supplied to the host CLI is for build-time verification and does not enroll those publishers on a device. Device publisher trust remains a separate user-approved step.

`passport serve` binds to localhost by default. For an explicitly authorized test device on a trusted LAN, run `passport serve dist --host 0.0.0.0 --port 8040`; this exposes every file in `dist` without authentication. Keep private keys and unrelated local files outside the served directory. For production, deploy the generated index, package files and icons to HTTPS static hosting. Each MiniApp developer operates any dynamic backend their own app requires.

## Validation boundaries

- `manifest-v1.schema.json` describes both authoring fields and the compiled package form. The SDK applies stricter cross-field and filesystem checks while packing; firmware remains authoritative.
- `ui-v1.schema.json` and `passport.declarative.validate_ui()` check supported declarative structures and device resource limits. UI rendering is still a device-level check.
- Lua files are UTF-8/NUL/size checked and signed, but the packer does not run application code. The simulator catches runtime behavior that host package verification cannot.
- `passport publish-index` validates package contents/signatures but does not enroll trusted publishers or apply human moderation policy.
- `passport serve` is a local development server, not an Internet service or production deployment tool.

See the [quickstart](quickstart.md), [package protocol](package-protocol.md), [Capability API](capability-api.md), [declarative UI/1](declarative-ui.md), [assets](assets.md), and [display layout](display-layout.md).

## Reliable serial diagnostics and publication

OS 1.0.22 implements `pin ID on|off`; the legacy `pin ID` means on.
`data_hash` takes `ID KEY`. Use one connection for command completion and subsequent
worker logs, for example:

```sh
passport device --port /dev/cu.usbmodem1101 --verbose --observe 20 launch org.passport.xiaozhi
```

`--observe` is bounded to 0..60 seconds. `--verbose` prints all serial lines; otherwise
only the existing Passport/metric lines are shown. Missing completion or an explicit
FAIL is a nonzero CLI exit. Do not run another serial monitor on the same port.
Review logs before sharing: application text and platform logs can contain private data.

Packing and index publication atomically replace their output files. Firmware publication
commits the complete image before atomically switching latest.json, rejects a decreasing
build, and refuses to overwrite an existing version's image with different content.
Use a single publisher process; these atomic writes do not provide a distributed
release lock or a public-hosting deployment transaction.

## Native ELF

Firmware 1.0.34 adds `passport init DIR --id org.example.native --runtime native-elf`, `passport native build DIR`, and `passport native inspect DIR/app.app.elf`. Signing, installation and launch reuse the existing tools; Native packages require the built-in OS publisher. See [Native application development](native-apps.md) for the ABI, limits and example.

Native ABI 2 requires OS 1.0.35 and adds generic network, identity, storage and audio services. Set manifest `abi:2` and declare permissions/domains; ABI 1 retains its original restrictions. See [ABI 2 services](native-services.md) and [Native XiaoZhi](xiaozhi-native.md).
