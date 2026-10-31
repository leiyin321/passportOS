<p align="right">
  <strong>简体中文</strong> · <a href="firmware-layout.md">English</a>
</p>

# Passport OS Flash 分区

当前工程目标设备为 8 MiB Flash 的 ESP32-C3。分区表定义在仓库根目录
[`partitions.csv`](../../../partitions.csv)。下表是本次已构建固件使用的布局：

| 区域/分区 | 偏移 | 大小 | 用途 |
| --- | ---: | ---: | --- |
| Bootloader 与分区表 | `0x000000` / `0x008000` | 32 KiB / 4 KiB | ESP-IDF 启动及分区元数据 |
| `nvs` | `0x009000` | 24 KiB | ESP-IDF 和系统配置键值；Wi-Fi 配置保存在这里 |
| `phy_init` | `0x00F000` | 4 KiB | 无线 PHY 初始化数据 |
| `otadata` | `0x010000` | 8 KiB | OTA 槽位选择/状态 |
| `ota_0` | `0x020000` | 2.5 MiB | 第一个系统固件槽 |
| `ota_1` | `0x2A0000` | 2.5 MiB | 第二个系统固件槽，可用于 OTA 更新候选镜像 |
| `packages` | `0x520000` | 2.375 MiB | SPIFFS `/cache`，安装的 `.pap` 应用包 |
| `appdata` | `0x780000` | 512 KiB | App Registry 和各应用独立 NVS 存档 |

布局终止于 `0x800000`，正好覆盖 8 MiB 地址空间。当前合并镜像中的固件应用
大小为 2,283,296 字节；相对单个 2.5 MiB OTA 槽还剩 338,144 字节（约 12.9%）。
这是镜像分区的余量，不是运行时 RAM，也不代表两个 OTA 槽都已写入固件。

系统应用更新走 OTA 槽；商店下载的小程序包存放于 `packages`，用户/应用配置存放于
`appdata`。二者和系统固件分区相互独立。1.0.23 通过设置中的 OTA 写入非活动
应用槽，保留分区表、已安装应用和 App 存档。全量合并镜像从 `0x0` 写入，可能覆盖
`nvs`、`otadata` 等区域，不能视为保留用户数据的升级方式。

用 `./tools/validate.sh --firmware` 检查镜像偏移、分区边界、重叠和容量；查看具体
构建产物可使用 `idf.py partition-table`，或检查 `build/partition_table/partition-table.bin`。
