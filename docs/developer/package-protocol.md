[简体中文](package-protocol.zh_CN.md)

# Passport package protocol 1

Status: implemented V1 contract; check [tasks](../tasks.md) and the test reports for hardware coverage and remaining acceptance.
The wire contract is shared by `sdk/passport` and `os/package/package.c`.
All integer fields are unsigned little endian; no compression. Native ELF payloads use the restricted profile below.

| Offset | Bytes | Meaning |
|---|---:|---|
| 0 | 4 | ASCII `PAP1` |
| 4 | 4 | Manifest UTF-8 byte length, 1..4096 |
| 8 | 4 | Entry UTF-8 byte length, 1..16384 |
| 12 | 4 | Signature length, exactly 256 |
| 16 | manifest length | JSON manifest |
| following | payload length | Single text entry, no embedded NUL |
| last | 256 | RSA-2048 PKCS#1 v1.5 / SHA-256 signature of ALL preceding bytes |

Exact total length is required; trailing bytes, truncation and oversized fields are rejected.
Manifest canonical output: JSON sorted keys, no insignificant whitespace, ASCII escapes.
Signatures are deterministic for a fixed manifest, source and RSA key. TLS is not package identity.
`publisher_key_id` is first 16 lowercase hex characters of SHA-256(public SubjectPublicKeyInfo DER).
`sha256` is full lowercase SHA-256 of entry bytes. Firmware accepts only its configured trusted key.
The firmware has one built-in publisher and up to4 additional RSA2048 publishers enrolled through the trusted USB/system confirmation flow.
Keep the matching private key outside version control; key replacement must be explicit.

Authoring manifests follow [`manifest-v1.schema.json`](../../protocol/manifest-v1.schema.json). Required fields: `format:passport.app/1`, reverse-DNS `id` (max 63 UTF-8 bytes),
`name`, numeric dotted `version`, `publisher_key_id`, `runtime:lua|declarative|native-elf`,
`entry:main.lua|ui.json`, `min_os_api:1`, `data_schema:1`, `permissions`, `limits`, `sha256`.
Duplicate keys, dangerous permissions and unsupported schema/API versions fail closed.
App versions are strict numeric `MAJOR.MINOR.PATCH` values. Store cards show the version and compare
it numerically with the installed package. A missing or older package is downloaded; an equal or
newer installed version is treated as current, and the next OK opens the app.
V1 intentionally accepts schema 1 only, preserving data rather than attempting an untested migration.
`limits`: `script_heap_kib` 1..64 (default 32), `fps` 1..20, `persistent_kib` 1..4.
Permissions: `ui`, `graphics`, `input`, `timer`, `storage.private`, `network.fetch`,
`network.websocket`, `audio.features`. No `system` flag can grant trusted authority.

Firmware version, Capability API, PAP format, app version and data schema are independent.
Changing PAP header semantics requires a new magic/version. Optional manifest fields may be added;
raising minimum API or schema requires a matching supported firmware. No bytecode or ZIP. Native ELF is allowed only through the profile below.

Identity = trusted publisher public-key hash + app ID. Persistent namespace derives from both.
Package cache and data live in separate Flash partitions. Offload must remove code and retained code
versions, keeping identity/entry/data. Remove & Delete Data requires an explicit system action.
An update stages `.new`, verifies signature/content, retains `.old`, then switches active code.
A failed active package can fall back to the verified previous version. SPIFFS power-cut behavior
still requires the hardware fault-injection matrix; do not infer atomic durability from a rename alone.

PAP2 image bundles retain this signature envelope and PAP1 compatibility. See the [asset protocol](assets.md) for the entry-length prefix, bounded index, offsets, PAI1 image formats and streaming verification. The static Store catalog wire shape is [`catalog-v1.schema.json`](../../protocol/catalog-v1.schema.json); its current device limit is eight entries. It carries discovery metadata only and does not replace package signatures or publisher approval.

## Native ELF profile (firmware 1.0.34+)

PAP1/PAP2 may carry a binary ELF entry of at most 32 KiB; the UTF-8/NUL restrictions and 16 KiB limit apply only to Lua/declarative entries. Native manifests require `entry:app.app.elf`, `target:esp32c3`, `abi:1`, `trusted:true`, and `limits.native_exec_kib` (1–16) instead of `script_heap_kib`. Only the built-in OS publisher may sign Native packages. Permissions are limited to UI, graphics, input and timer. See [Native development](native-apps.md) for the ELF validation profile, ABI, tools and trust limits.

Native ABI 2 requires OS 1.0.35 and adds generic network, identity, storage and audio services. Set manifest `abi:2` and declare permissions/domains; ABI 1 retains its original restrictions. See [ABI 2 services](native-services.md) and [Native XiaoZhi](xiaozhi-native.md).

Native OS 1.0.51 adds `audio.playback`, restricted to trusted Native ABI 2 apps. With `network.fetch` it enables the optional HTTPS MP3 media host extension without microphone permission. Declare each stream/redirect domain explicitly. See [media ownership, API and limits](city-radio.md).
