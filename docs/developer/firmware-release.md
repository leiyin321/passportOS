[简体中文](firmware-release.zh_CN.md)

# Firmware delivery and recovery

For current public Pages, community first-install images and Native-first releases, use the [distribution guide](../../distribution/README.md). The commands below describe the older local developer bundle; a firmware readback is not a prerequisite for installation.

Release output is local `build/release/`; regenerate from a verified archive with
`python tools/passport/release.py build/firmware/<SHA256>` in an activated IDF5.5.3 environment.
The archive retains matching ELF/MAP, binaries and manifest. Verify with
`python tools/archive_firmware.py verify build/firmware/<SHA256>`.
From the release directory run `shasum -a 256 -c SHA256SUMS` before use.

## Existing Passport OS board

Close every serial monitor/test. Read `ota_status` over the trusted USB channel to check the active
slot; the tested board is OTA0. Updating that slot while it is stopped in the ROM flasher uses:

```sh
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem101 -b 460800 write_flash 0x20000 build/release/PassportOS-app.bin
```

OTA1 starts at `0x2a0000`. Do not overwrite an active pending-verification trial; return to a valid
slot first. This command is a development update, not an atomic network OTA protocol.
It preserves package storage, private appdata and Wi-Fi NVS. Do not use the initial system image
or EMPTY images on an existing OS board if its data must survive.

## First migration to the OS partition layout

This replaces firmware, partition table, Wi-Fi NVS and vendor partitions. Preserve any required
original device data first; the development board's private complete backup is `local/original-flash.bin`.
The initial system image includes bootloader, partition table, empty OTA metadata and OTA0 application.
It does not include initialized package/appdata storage; first migration needs all three writes:

```sh
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem101 -b 460800 write_flash 0x0 build/release/PassportOS-initial-system.bin 0x520000 build/release/packages-EMPTY.bin 0x780000 build/release/appdata-EMPTY.bin
```

No whole-chip erase is needed. The current board has already completed migration: do not repeat it.
Reprovision Wi-Fi and install signed apps through the SDK. Slots each hold 2.5MiB; packages 2.375MiB;
private appdata NVS 512KiB. Full Flash remains 8MiB. Original vendor firmware is recoverable only from
a matching private backup; restoring all 8MiB also replaces every OS package, setting and app save.

## Developer delivery

The bundle contains public publisher key, seven signed examples, SDK source, manifest/API specifications,
asset compiler, native/browser simulator source and developer documentation. Install with
`python -m pip install -e './sdk[assets]'`; simulator needs a host C compiler. Example sources and compiler
lockfile are included. Native simulator build paths assume the release directory as the working directory.
A separate ESP-IDF checkout is needed for firmware development; full OS source remains in this repository.
No private signing key, Wi-Fi secret or device backup is distributed.

Build and host tests passed; actual USB flashing, application/asset/recovery tests passed.
Application visual/play QA, browser visual inspection, public WSS and physical power loss during programming
remain unverified. Settings now exposes a signed dual-slot network updater. Its host release tests
and firmware build pass, but device HTTP download/installation and power-cut rollback still require
the acceptance matrix in [system updates](system-updates.md).
