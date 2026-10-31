[English](firmware-release.md)

# 固件交付与恢复

当前 Pages、社区首次安装镜像及 Native 优先发布请使用[分发指南](../../distribution/README.zh_CN.md)。下文为较早的本地开发包流程，首次安装不要求先回读原固件。

本地发布目录为 `build/release/`。激活 IDF5.5.3 后，用
`python tools/passport/release.py build/firmware/<SHA256>` 从已验证归档重新生成。
归档保留匹配的 ELF/MAP、镜像和清单。执行
`python tools/archive_firmware.py verify build/firmware/<SHA256>` 验证归档；
在发布目录运行 `shasum -a 256 -c SHA256SUMS` 校验发布文件。

## 已安装 Passport OS 的设备

关闭所有串口监视/测试。通过可信 USB 通道的 `ota_status` 确认当前分区；本次设备为 OTA0。
进入 ROM 刷写器后，更新该分区：

```sh
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem101 -b 460800 write_flash 0x20000 build/release/PassportOS-app.bin
```

OTA1 地址为 `0x2a0000`。不要覆盖尚待验证的试运行分区，先返回有效分区。
这是开发更新方式，不是原子网络 OTA 协议。它保留包存储、应用私有数据和 Wi-Fi NVS。
需要保留现有数据时，禁止使用初始化系统镜像和 EMPTY 镜像。

## 首次迁移到 OS 分区布局

这会替换固件、分区表、Wi-Fi NVS 和厂商分区。先保留所需原始数据；开发设备的完整私有备份为
`local/original-flash.bin`。初始系统镜像含 bootloader、分区表、空 OTA 元数据、OTA0 应用，
不包含初始化后的包/应用数据存储。首次迁移需要以下三个写入：

```sh
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem101 -b 460800 write_flash 0x0 build/release/PassportOS-initial-system.bin 0x520000 build/release/packages-EMPTY.bin 0x780000 build/release/appdata-EMPTY.bin
```

不需要整片擦除。当前设备已经完成迁移，禁止重复初始化。随后用 SDK 配置 Wi-Fi、安装签名应用。
两个 OTA 槽各 2.5MiB，包分区 2.375MiB，私有数据 NVS 512KiB，总 Flash 8MiB。
原厂固件只能从匹配的私有备份恢复；恢复完整 8MiB 同样会覆盖所有 OS 应用、设置和存档。

## 开发者交付

发布包包括发布者公钥、七个签名示例、SDK 源码、Manifest/API 规范、资源编译器、
原生/浏览器模拟器源码和开发者文档。安装 `python -m pip install -e './sdk[assets]'`；
模拟器需要宿主 C 编译器。包含示例源资源和编译器锁文件，原生模拟器构建以发布目录为工作目录。
固件开发另需 ESP-IDF，完整 OS 源码保留在本仓库。发布包不含私钥、Wi-Fi 密码和设备备份。

构建、宿主测试通过；真实 USB 刷写、应用/资源/恢复测试通过。
未验证：应用视觉/游玩、浏览器视觉、公网 WSS、编程期间物理断电。Settings 已加入签名双槽
网络更新器；宿主发布测试和固件构建通过，但真机 HTTP 下载/安装及断电回滚仍需按
[系统更新指南](system-updates.zh_CN.md)完成验收。
