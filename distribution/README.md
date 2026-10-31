[简体中文](README.zh_CN.md)

# Public and local distribution

One repository, `leiyin321/passportOS`, contains OS, SDK, app sources and static
release tooling. Public URLs are `https://leiyin321.github.io/passportOS/updates/latest.json`
and `https://leiyin321.github.io/passportOS/apps/index.json`. Pages needs no dynamic
backend. App-specific dynamic services belong to their developers.

## First installation and recovery

Use the community's reviewed **first-install** image at address `0x0`, ESP32-C3,
8 MiB Flash. The complete image initializes package/app-data storage and replaces
existing firmware, Wi-Fi, settings and apps. Do not use it to upgrade an existing
OS whose data should survive: use Settings OTA instead. USB recovery remains
available if Wi-Fi/OTA fails. The image does not contain Wi-Fi credentials,
installed apps or a private signing key.

Build the community image only from a verified firmware archive:

```sh
python tools/passport/community_image.py build/firmware/ARCHIVE_SHA build/PassportOS-first-install.bin
```

Record its SHA-256 and publish it with the version, hardware target, destructive
first-install notice, three-button instructions, Wi-Fi setup and public service
URLs. Do not present a merged initial image as an app-only OTA file. The OTA
manifest signature covers the application image, not every byte of the community
image; the complete image also requires maintainer review and its published hash.

## Build the public site locally

```sh
python -m pip install ./sdk
# The signing key is local, outside the repository/CI build.
passport firmware-release build/FoloToy-AI-Passport.bin --version VERSION --build MONOTONIC_BUILD --key /private/release.pem --public distribution/release-public.pem -o build/updates
python tools/passport/pages.py build --firmware build/updates --packages dist --selection distribution/apps.json --public distribution/release-public.pem --output build/pages
python tools/passport/pages.py bundle build/pages build/passport-distribution.zip
```

Outputs are immutable versioned files plus `updates/latest.json` and
`apps/index.json`. The builder verifies signatures/hashes, constructs relative
app URLs (including project-site subpaths), and copies only selected payloads.
It refuses an existing destination. Deploy the whole site atomically; never
serve a new index before its files. Private keys and unrelated files are not copied.
The current curated Store is Native XiaoZhi, City Radio and Bluetooth Peripherals;
legacy Lua XiaoZhi is not selected. Lua runtime and examples remain available.

## Local development / forks

```sh
passport keygen --private local/fork-private.pem --public local/fork-public.pem
PASSPORT_RELEASE_PUBLIC_KEY="$PWD/local/fork-public.pem" PASSPORT_DISTRIBUTION_URL='' PASSPORT_OS_VERSION=1.0.0 ./tools/validate.sh
passport init local/my-app --id org.yourname.app
passport native build local/my-app
passport pack local/my-app --key local/fork-private.pem --public local/fork-public.pem -o local/my-app.pap
```

Forks must keep a stable own key across app updates. A new trust root requires
initial USB installation of that fork's OS; the public OS does not accept a
fork-signed firmware or Native app just because its URL changed. To distribute a
fork publicly, replace `distribution/release-public.pem` (the committed **public** key), keep the SDK fixture key unchanged, edit `distribution/config.json`
and curated selection, then use the same workflow. SDK wheels include C headers,
support code and template; activate ESP-IDF for the RISC-V compiler.

To run the same static outputs locally, in separate terminals:

```sh
passport serve build/pages/apps --host 0.0.0.0 --port 8040
passport serve build/pages/updates --host 0.0.0.0 --port 8765
passport device service LAN_IP 8040 8041 --port SERIAL_PORT
passport device update-source http://LAN_IP:8765/latest.json --port SERIAL_PORT
```

Approve the local service on the device with OK. Use a private IPv4 LAN address;
public plain HTTP is rejected. Local Store approval is session-scoped, so repeat
it after a reboot. The update source persists. `passport device catalog HTTPS_URL`
sets a persistent HTTPS Store override. Firmware defaults come from
`distribution/config.json`; `PASSPORT_DISTRIBUTION_URL` overrides them (empty
means no public default). Saved valid overrides take precedence over defaults.
Changing server does not change signature checks. `passport init --runtime lua`
explicitly creates a Lua app.

## GitHub collaboration and publication

1. Fork/branch and submit **source** PRs with tests, permissions, minimum API/ABI,
   licensing, privacy/backends and physical acceptance evidence. Never submit keys.
2. Maintainers review native code, build/test OS and apps, sign on a trusted local
   machine, and update `distribution/apps.json`. No signing secrets in PR jobs.
3. Create a reviewed GitHub Release containing `passport-distribution.zip`.
   Keep firmware versions and build numbers monotonic; do not overwrite versions.
4. Enable Pages with GitHub Actions and set repository variable
   `PASSPORT_PUBLISH_ENABLED=true`. Configure the `github-pages` environment with
   `main` branch protection/required reviewers as available for the account.
5. Manually run **Verify and optionally deploy Passport distribution** for that
   release tag. Verification reconstructs the site with the repository public key.
   Select deploy only after review; push/PR alone does not deploy the site.
6. Verify the public HTTPS manifest, all sizes/hashes, actual device update,
   latest-version response, Store and a real package download/install.

The firmware build workflow is manual and uploads review artifacts only; it does
not sign or publish a release. `version.txt` supplies the default OS version. Automatic upstream merging is disabled;
manual upstream sync additionally requires `PASSPORT_UPSTREAM_SYNC=true`.
Pages release jobs have no private key. Keep previous signed releases for recovery;
rollback publication must still obey the device's monotonic version/build rules.

[GitHub's Pages workflow requirements](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
require deployment permissions and a Pages environment. Observe the
[Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits):
1 GB published site and a soft 100 GB/month bandwidth limit. Retain a bounded set
of releases and move large archives/debug artifacts to appropriate release storage.

## Remaining platform boundaries

Native code is trusted and can affect OS stability; the ABI is not process
isolation. Firmware signing-key rotation/revocation and anti-rollback beyond the
current version/build policy are not implemented. Pages availability and app
backend availability are separate. The project does not promise App Store-style
sandboxing or a hosted AI backend.
