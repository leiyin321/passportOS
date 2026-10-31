[简体中文](community-post.zh_CN.md)

# Passport OS 1.0.75 — an OS and Native apps for AI Passport

Publication draft for the AI Passport community. Author: `leiyin321`.
This is a community OS based on the official FoloToy BSP, not manufacturer firmware.
Target: ESP32-C3 AI Passport, 8 MiB Flash, no PSRAM, three buttons.

## Features

- Launcher and independently downloaded Native apps; Lua compatibility remains.
- Native XiaoZhi AI voice, City Radio and iPhone Bluetooth gesture controller.
- Wi-Fi phone QR/captive setup, multiple saved networks and same-phone hotspot setup.
- Chinese/English, light/dark mode, volume, clock/alarm, screen timeout and storage details.
- Signed public System Update and App Store hosted as static GitHub Pages files.

## First installation

Download `PassportOS-first-install-1.0.75.bin` and `SHA256SUMS` from the
[release](https://github.com/leiyin321/passportOS/releases/tag/v1.0.75).
Flash the **8 MiB complete first-install image at `0x0`**, with the board connected
by a USB data cable. This replaces existing firmware, Wi-Fi, settings, applications
and app data. It is not an ordinary data-preserving upgrade. Existing Passport OS
users should instead use Settings → System Update. Never flash the app-only OTA
file at `0x0`. A readback backup is not required to install.

UP/DOWN selects, short OK confirms, double OK returns Home. Configure a 2.4 GHz
network in Settings → Wi-Fi using the phone QR/local page, then browse App Store.
For a hotspot on the same phone, choose hotspot mode and save first, then enable
the phone hotspot when Passport releases its AP. On iPhone enable Allow Others to
Join and Maximize Compatibility where available.

## Updates, apps and source

- [Distribution site](https://leiyin321.github.io/passportOS/)
- [Source and SDK](https://github.com/leiyin321/passportOS)
- [Local/fork and collaboration guide](README.md)
- [Actual acceptance and boundaries](../docs/test-reports/public-distribution.md)

The board passed public HTTPS OTA, latest-version detection, Native catalog browsing
and forced public app download/reinstallation. The complete first-install image was
generated and checked but has not been destructively flashed onto a spare board.
Audio/AI services are maintained by their respective app/backend providers, not Pages.
Native apps are trusted code reviewed/signed by the OS maintainer, not isolated processes.
Developers can fork the single repository, use their own key and local static services,
and submit mature source contributions for review.
