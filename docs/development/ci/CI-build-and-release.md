<p align="right"><a href="CI-build-and-release.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport OS build and release

`.github/workflows/build-firmware.yml` is manually dispatched. It builds and verifies
ESP32-C3 firmware with ESP-IDF 5.5.3 and uploads a review artifact; it does not sign
firmware or create a GitHub Release. `version.txt` supplies the default OS version.
A valid `PASSPORT_OS_VERSION` build override is available for development.

OS tags are `v<version>`, for example `v1.0.75`. Apps have independent PAP versions,
not separate OS firmware tags. Maintainers run the complete gate, verify the archive,
compile/test selected apps, sign locally and create the reviewed release bundle.
See the [distribution commands](../../../distribution/README.md).

`.github/workflows/pages-distribution.yml` is a separate manual workflow. It downloads
a reviewed release's `passport-distribution.zip`, verifies firmware/apps with the
committed distribution public key, retains verified previous payloads and generates
the static site. Deployment additionally requires the deploy input, publication
variable and `main` Pages environment policy. Signing keys are never provided to CI.
All third-party Actions use full commit SHAs; verify pins against official repositories
before changing them. PR checks do not deploy or sign anything.

## First installation versus updates

The OS product layout in `partitions.csv` has dual 2.5 MiB app slots, package storage
and independent app data. The current app starts at `0x20000`; do not use the baseline
hardware demo's app offset. Merged build artifacts are not the complete initialized
community image. Generate the 8 MiB first-install file with
`tools/passport/community_image.py` from a verified archive. Flash at `0x0` only for
first installation or an explicitly chosen complete reset; it replaces settings/data.
Existing OS devices use Settings OTA, which writes the inactive app slot.
An app-only OTA `.bin` must never be written at `0x0`.

Publication includes version, hardware target, checksums, destructive first-install
notice and player controls. See the [community draft](../../../distribution/community-post.md)
and [acceptance report](../../test-reports/public-distribution.md). A source build
or a successful upload is not a physical flashing/first-boot test.
