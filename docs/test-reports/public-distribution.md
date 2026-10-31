[简体中文](public-distribution.zh_CN.md)

# Public distribution acceptance — OS 1.0.75

Final target: `leiyin321/passportOS`, one Native-first OS/SDK/app repository.
Public OS **1.0.75**, signed build **2026110102**, Pages project `/passportOS`.

Build: **PASS**. Host tests: **PASS**. Device tests: **PASS** within the scope below.

## Source, build and public services

- Clean exported source retains attribution and test fixtures, without private history,
  signing keys, device backups, raw diagnostics or build products. Original working
  checkout and upstream remote are preserved. Exported host tests and actual GitHub
  Linux host/ESP-IDF builds passed. The glibc `features.h` name collision and GCC-only
  control-flow warnings found during first CI runs were corrected, not suppressed.
- Complete ESP-IDF 5.5.3 local gate passed. Application **2,423,200 bytes**, headroom
  **76,800 bytes** below 2.5 MB; merged **2,554,272 bytes**. Matching bundle
  `build/firmware/0816c0c2099da6ddca9f9568198af6d0114546009688233b0a3ada8eb53e39e2/`;
  ELF SHA-256 `4f5282889d1750901db35e3c000b47265abb5107d1c4d9907f819926da170984`;
  app SHA-256 `d0212517bc4db0e7c678e9108c46b3800c658e30cc7e5f5369ac60affe2ef70d`.
- Public endpoints verified over HTTPS: `/updates/latest.json` and `/apps/index.json`.
  Host checks downloaded and verified firmware plus all three PAPs, their signatures,
  hashes, identities, versions and sizes. Catalog uses project-relative package URLs.
- Actual Pages workflow verification/deployment passed; manual deploy, publication
  variable and `main` environment policy are configured. CI has no signing key.
  The retained signed **1.0.73** image was still downloadable and hash-verifiable
  after deploying 1.0.75. Retention/rollback/immutable-package tests pass.
- Installed SDK wheel includes Native headers, support code, template and MIT license.
  Native init/build passed outside the checkout. All three reformatted Native sources
  rebuilt to ELF bytes identical to the existing signed packages. Lua remains supported;
  Lua XiaoZhi is retired from the curated public catalog.

Evidence: [Linux host CI](https://github.com/leiyin321/passportOS/actions/runs/41219237807),
[firmware CI](https://github.com/leiyin321/passportOS/actions/runs/41219237872),
[Pages deployment](https://github.com/leiyin321/passportOS/actions/runs/41219583861),
[release](https://github.com/leiyin321/passportOS/releases/tag/v1.0.75).

## Actual board acceptance

The connected board completed public Pages OTA to 1.0.73 and its current-version
check. A local signed 1.0.74 bootstrap enabled the improved download policy, then
**1.0.75 was checked/downloaded/verified/installed directly from public Pages**.
Download plus verification took **210.83 seconds**, followed by restart; running
slot `ota_0`, state **2**. About reported **1.0.75 / 2,423,200 bytes**. The earlier
public transfer took about 453 seconds; these observations are not a controlled
network benchmark or a throughput guarantee.

After reboot, public latest-version check correctly reported current. The public
Store listed exactly Native Bluetooth Peripherals 0.1.4 (**6,779 bytes**), City Radio
0.1.2 (**13,586 bytes**) and Native XiaoZhi 1.0.4 (**13,404 bytes**). To prove a real
download, Bluetooth's cached code was offloaded while keeping its registry/data,
then reinstalled from the public Store in **4.23 seconds**. The subsequent action
reported current and offered to open, rather than reinstall. Registry has three
Native apps, Wi-Fi/language remained available, and no pairing-clear operation ran.
Serial was closed after return Home. No USB Flash write or erase was performed.

OTA on the preceding boot: sampled free heap **96,724 bytes** while downloading,
whole-boot minimum **67,876 bytes**, post-transfer **108,208 bytes**. New 1.0.75 boot:
free **108,616**, minimum **106,260 bytes** before service tests. After current check
and Store download/install: global minimum **70,152 bytes**, sampled free
**103,376**, largest block **77,824 bytes**, main stack margin **1,508 bytes**.
About 5 KiB remains in cached catalog/UI state; this is not claimed as a leak or
full reclamation. These are whole-boot minima and samples, not isolated RAM peaks.
Raw logs and unsigned operational details remain under ignored `local/open-source/`.

## First installation and remaining checks

The reviewed community-format image is **8,388,608 bytes**, initializes package
storage and appdata, and replaces settings/apps on first install. SHA-256:
`b4df071223bd3530e8e1043592395b8a8aaafb28173ce6a866214d4df1f60e5e`.

Unverified: destructive first installation on a spare board, cold first-boot default
server activation without saved overrides, all language/theme visual combinations,
regional CDN performance, interrupted-power rollback and hardware fault injection.
Current-board OTA preserves data; arbitrary private-data and Bluetooth-bond bytes
were not independently compared. No community article was posted by this workflow;
firmware and player instructions are ready for that separate publication.

Final unsigned Native-example CI: [firmware/Native source gate](https://github.com/leiyin321/passportOS/actions/runs/41222649873). Local compilation covered all four Native examples; SDK wheel and community first-install asset were also downloaded/verified as published. Private vulnerability reporting and one-review/static-check protection are enabled for the public repository.
