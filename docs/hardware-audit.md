[简体中文](hardware-audit.zh_CN.md)

# M0 hardware and upstream audit

Official main: `006c2279852e352a8433b2c21e5e09be2ca36f77`, cloned 2026-10-25.
Native macOS arm64; ESP-IDF v5.5.3; compiler esp-14.2.0_20251107.
USB `/dev/cu.usbmodem101`: ESP32-C3 QFN32 rev 1.1, XMC 8MB embedded flash, 40MHz.
Baseline `idf.py -B build-baseline build`: PASS. App 1,527,616 bytes;
SHA256 `c18ccbc3a2d759663d05380975dbe729335ce66418daaeba2a6a87898ca1a7f8`.
Original table: NVS 0x9000/0x6000, PHY 0xf000/0x1000, factory 0x10000/0x7f0000.
Baseline runtime not flashed; no baseline heap/CPU measurements claimed.

| Resource | Audited upstream implementation |
|---|---|
| LCD | SPI2 80MHz ST7789P3, 240x320; single 40-row RGB565 DMA buffer 19,200 B |
| LVGL | 24KiB static pool; BSP rounded flush; no app framebuffer |
| Keys | GPIO0 ADC1_CH0 shared ladder; BSP events only |
| I2C | BSP I2C0 SDA10/SCL7 shared CW2017 0x63 and ES8311 0x18 |
| Audio | I2S0 six x 240 frames DMA; 16kHz mono API; BSP read blocks |
| USB | GPIO18/19 native serial/JTAG, no USB host |
| Backlight | GPIO21, LEDC; never UART TX |
| RAM | No PSRAM; runtime metrics emitted as METRIC phase/free/minimum/largest/stack |

OS disables BLE to reserve RAM for TLS/Lua. No BSP driver changes.
Resource measurements must come from serial logs; build size is not RAM availability.
Original private Flash backup and restore commands reside in ignored `local/`.
