[简体中文](miniapp-sdk-audit.zh_CN.md)

# MiniApp standards and developer tooling audit

Date: 2026-10-25 · Scope: Manifest/PAP, capabilities, declarative UI, SDK/CLI, examples, catalog and host validation.

## Findings and changes

- The Manifest schema did not describe fields actually used in authoring manifests or the compiled PAP2 asset records. Replaced it with a schema covering both source and packaged asset forms; SDK validation now checks required types/fields, unknown top-level and limit fields, version/API bounds, permissions, asset records, and entry/runtime pairing.
- `passport pack` and `passport validate` previously accepted any UTF-8 declarative entry, including unsupported nodes/actions, undeclared image resources and missing capabilities. Added bounded UI/1 preflight for parser/resource limits, supported nodes/actions, bindings, timers, data requests, asset references and required permissions. Removed `navigate`, which only changed a state key and did not navigate, from both SDK and device runtime.
- Declarative UI had no published machine-readable contract or developer reference. Added UI/1 schema and paired docs, including current renderer limitations and precise action/data behavior.
- Static catalog publication accepted only one publisher key although the OS supports several publishers. `passport publish-index` now accepts repeated `--public`, verifies each package against the supplied publisher keys, rejects duplicate app IDs, and rejects more than the current device limit of eight entries. Added the catalog schema and documented that index metadata is not signed; package signatures and device publisher approval remain authoritative.
- The sample HTTP server bound to every interface implicitly. It now defaults to `127.0.0.1`; LAN exposure requires an explicit `--host 0.0.0.0` and the docs warn that the server has no authentication.
- Integrated the MiniApp SDK/runtime/asset test suite into `./tools/validate.sh --static` so the developer package tools are part of the regular host gate.

## Validation

| Check | Result |
| --- | --- |
| `./tools/passport/test.sh` | PASS: sessions, endpoint/redirect policy, audio features, packages, declarative preflight, updater, simulator runtime, icon registry, asset format/loader/compiler |
| Package protocol tests | PASS, 12 tests including multi-publisher generation, catalog limit and source/compiled Manifest fixtures |
| Declarative UI tests | PASS, 5 tests including every shipped declarative package, duplicate keys, invalid nodes/actions, bounds, permissions and asset references |
| `./tools/validate.sh --static` | PASS; repository check scanned 590 text files and all host suites passed |
| `./tools/validate.sh --firmware` (ESP-IDF 5.5.3 / ESP32-C3) | PASS; app 2,067,776 / 2,621,440 bytes, 553,664 bytes free (21.12%); merged image 2,198,848 bytes |
| JSON syntax checks for three protocol schemas | PASS (`python3 -m json.tool`) |
| Hardware App Store and publisher enrollment | NOT RUN |

The app image is 64 bytes smaller than the previous 2,067,840-byte build after removing the nonfunctional declarative `navigate` action. This is a Flash image measurement, not a RAM/runtime measurement. No device was connected; physical package installation, publisher enrollment, catalog navigation, Chinese rendering, and post-exit heap readings remain unverified.

## Remaining work

- Device App Store currently accepts at most eight entries and has no pagination. Keep generated indexes within that limit; implement and resource-test paging before expanding a public catalog.
- `passport validate` does not execute Lua. Developers must run simulator/runtime behavior tests, then device acceptance; package validity is not proof that app behavior or rendering is correct.
- Community publisher moderation, key revocation/rotation and catalog ownership policy remain release governance decisions; multi-key catalog generation does not enroll keys on devices.
- Public Pages deployment and a clean public checkout/reproducible release workflow remain deferred until Passport OS is stable and approved for open source.
- The XiaoZhi implementation adds a host-simulated config request, authenticated WSS v1 handshake, push-to-talk state path, bounded Opus APIs, and bilingual conversation UI. Target-server activation, real Opus frames, playback, resource behavior, and bilingual rendering still require device acceptance. XiaoZhi handshake/configuration remains app code. See [XiaoZhi AI MiniApp](../developer/xiaozhi-miniapp.md).
