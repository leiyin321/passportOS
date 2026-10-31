[简体中文](public-distribution.zh_CN.md)

# Public distribution acceptance

Target: `leiyin321/passportOS`, Pages project path `/passportOS`.
Public candidate: OS **1.0.73**, signed build **2026103103**.

Build: PASS, complete ESP-IDF 5.5.3 gate. Application **2,423,024 bytes**,
**76,976 bytes** below 2.5 MB. Archive
`build/firmware/ae3e56c7ca754f57c69ce94eaf8058dc09c0bbd42d51a6b2f1846490de44f1dd/`;
matching ELF SHA-256 `d3bc5211a0c614424b647cd08f6c64416561f8bfb441a727276a39fdfef56f48`.
The image contains both actual public Pages service URLs.

Host tests: PASS for the OS gate and exported source snapshot. Site builder tests
cover independent fork keys, signature/hash rejection, relative app URLs, immutable
output, bundle reconstruction and ZIP traversal rejection. An installed SDK wheel
created and compiled a Native app outside the checkout with bundled C resources.
Clean-source font tests no longer depend on managed component downloads; asset
requirements are explicit in CI. Export excludes private history, private keys,
raw logs and generated firmware; retains attribution and signed test fixtures.

The curated catalog contains Native XiaoZhi 1.0.4, Bluetooth Peripherals 0.1.4,
and City Radio 0.1.2. Legacy Lua XiaoZhi remains retired; Lua runtime compatibility
and examples remain. Source PRs have no signing or deployment secret. Production
and fork public keys are configurable without changing the fixed test-fixture key.

Device tests: NOT RUN for public delivery at this checkpoint. GitHub deployment,
public HTTPS payload verification and real Pages OTA/app-download acceptance will
be appended after execution. Community first-install image is generated with clean
package/data partitions; an actual destructive first installation on a spare board
remains unverified. The current board's settings/data must be preserved by OTA.
