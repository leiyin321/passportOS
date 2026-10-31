[English](hardware-audit.md)

# M0 硬件与上游审计

官方 main：`006c2279852e352a8433b2c21e5e09be2ca36f77`，2026-10-25 克隆。
macOS arm64；ESP-IDF v5.5.3；esp-14.2.0_20251107 编译器。
`/dev/cu.usbmodem101`：ESP32-C3 QFN32 rev 1.1，XMC 内置 8MB，40MHz。
`idf.py -B build-baseline build` PASS；应用 1,527,616 字节；
SHA256 `c18ccbc3a2d759663d05380975dbe729335ce66418daaeba2a6a87898ca1a7f8`。
原始分区：NVS 0x9000/0x6000、PHY 0xf000/0x1000、factory 0x10000/0x7f0000。
未刷官方演示，不声称已测其运行堆或 CPU。

| 资源 | 上游代码审计 |
|---|---|
| LCD | SPI2 80MHz，ST7789P3 240x320；40 行单 RGB565 DMA 缓冲 19,200 字节 |
| LVGL | 24KiB 静态池；BSP 圆角 flush；不分配应用全屏缓冲 |
| 按键 | GPIO0 ADC1_CH0 电阻梯；只使用 BSP 事件 |
| I2C | BSP I2C0 SDA10/SCL7；CW2017 0x63 与 ES8311 0x18 共享 |
| 音频 | I2S0，6×240 帧 DMA；16kHz 单声道 API；读取阻塞 |
| USB | GPIO18/19 原生串口/JTAG，无 USB Host |
| 背光 | GPIO21 LEDC，不作为 UART TX |
| RAM | 无 PSRAM；METRIC 日志记录 free/minimum/largest/stack |

OS 关闭 BLE，给 TLS/Lua 保留内存。未修改 BSP 驱动。
运行指标必须来自串口日志；固件大小不代表 RAM 余量。
私有 Flash 备份位于忽略的 `local/`。
