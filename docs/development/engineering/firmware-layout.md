<p align="right">
  <a href="firmware-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Passport OS Flash partitions

The current target is an ESP32-C3 with 8 MiB of Flash. The partition table is
[`partitions.csv`](../../../partitions.csv) at the repository root. This is the
layout used by the latest validated firmware:

| Region/partition | Offset | Size | Purpose |
| --- | ---: | ---: | --- |
| Bootloader and partition table | `0x000000` / `0x008000` | 32 KiB / 4 KiB | ESP-IDF boot and partition metadata |
| `nvs` | `0x009000` | 24 KiB | ESP-IDF and system key/value settings, including Wi-Fi configuration |
| `phy_init` | `0x00F000` | 4 KiB | Radio PHY initialization data |
| `otadata` | `0x010000` | 8 KiB | OTA slot selection/state |
| `ota_0` | `0x020000` | 2.5 MiB | First system firmware slot |
| `ota_1` | `0x2A0000` | 2.5 MiB | Second system firmware slot for an OTA candidate image |
| `packages` | `0x520000` | 2.375 MiB | SPIFFS at `/cache`; installed `.pap` app packages |
| `appdata` | `0x780000` | 512 KiB | App registry and per-app NVS data |

The layout ends at `0x800000`, covering the 8 MiB address space. The application
image in the latest merged firmware is 2,283,296 bytes. That leaves 338,144 bytes
(about 12.9%) in one 2.5 MiB OTA slot. This is firmware-partition headroom, not
runtime RAM, and does not mean both OTA slots contain an image.

System firmware updates use the OTA slots; Store-downloaded apps live in
`packages`, and user/app settings live in `appdata`. Those are separate from the
system firmware. Release 1.0.23 is delivered through Settings OTA to the inactive application
slot, preserving the partition table, installed packages and app data.
Writing the merged image at `0x0` can overwrite `nvs`, `otadata`, and other
regions, so it must not be treated as a user-data-preserving update.

Run `./tools/validate.sh --firmware` to check image offsets, partition bounds,
overlap, and app capacity. For a build's concrete table, use `idf.py
partition-table` or inspect `build/partition_table/partition-table.bin`.
