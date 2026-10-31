**English** · [简体中文](README.zh_CN.md)

# Passport OS

A small operating system for the FoloToy AI Passport: ESP32-C3, 8 MiB Flash,
no PSRAM, color display and three buttons. Native ELF apps are the primary
application format; existing Lua apps remain supported.

## Play

Install the reviewed first-install firmware from the AI Passport community.
Connect Wi-Fi in Settings, browse App Store, and use Settings → System Update
for later OS upgrades. OS and apps update independently.

Public distribution: <https://leiyin321.github.io/passportOS/>.
The [installation and distribution guide](distribution/README.md) describes
first installation, recovery, local services and the release process.

## Develop

Use ESP-IDF 5.5.3. Follow [environment setup](docs/development/engineering/environment-setup.md),
then run `./tools/validate.sh`. Source and tools live together:

| Directory | Purpose |
| --- | --- |
| `main`, `os`, `components` | Launcher, system services, Native/Lua runtimes, BSP |
| `sdk`, `protocol` | Native C ABI, package/signing/device tools and protocols |
| `examples` | Native XiaoZhi, City Radio, Bluetooth Peripherals and examples |
| `distribution`, `tools/passport` | Curated catalog, static-site builder, local release tools |
| `.github/workflows` | PR validation and manually gated Pages deployment |

Start with [Native development](docs/developer/native-apps.md).
Install the SDK with `python3 -m pip install ./sdk`; `passport init` creates a
Native app by default. Use `--runtime lua` for Lua compatibility projects.
Forks can build with their own RSA public key and local/private release services.
Private signing keys never belong in source, firmware, Pages, PRs or public artifacts.

## Contribute and release

Submit source changes through pull requests. Include host tests, ABI/capability
requirements and device acceptance. Maintainers review, build and sign releases;
PR workflows have no signing key or deployment authority. Native apps are trusted
code and require the OS release publisher; enrolling a Lua publisher does not
turn arbitrary Native code into an approved app.

See [contribution rules](.github/CONTRIBUTING.md),
[distribution](distribution/README.md), [SDK](docs/developer/sdk-cli.md) and
[hardware documentation](docs/README.md). MIT license for this project; retain
upstream copyright and each [third-party license](distribution/THIRD_PARTY.md).
