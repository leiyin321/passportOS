[English](community-post.md)

# Passport OS 1.0.75：让 AI Passport 成为可安装 Native 应用的小系统

AI Passport 玩法社区发布草稿，作者：`leiyin321`。
基于 FoloToy 官方 BSP 的社区 OS，不是厂家出厂固件。
目标：ESP32-C3 AI Passport、8 MiB Flash、无 PSRAM、三键。

## 功能

- 桌面及独立下载的 Native 应用，保留 Lua 兼容。
- Native 小智 AI 对话、城市电台、iPhone 蓝牙手势控制器。
- 手机二维码/登录页配网、多网络保存、同一手机热点配网。
- 中英双语、深浅色、音量、时钟/闹钟、熄屏时间和存储详情。
- GitHub Pages 静态托管的签名系统更新和应用商店。

## 首次安装

从[发布页](https://github.com/leiyin321/passportOS/releases/tag/v1.0.75)下载
`PassportOS-first-install-1.0.75.bin` 及 `SHA256SUMS`，核对哈希。
使用 USB 数据线连接，以 **`0x0` 地址刷入 8 MiB 完整首次安装镜像**。
该操作替换现有固件、Wi-Fi、设置、应用及应用数据，不是保留数据的普通升级。
已有 Passport OS 请使用设置 → 系统更新。不能将 OTA 应用文件刷到 `0x0`。
首次安装不要求先回读原固件。

上下键选择，短按 OK 确认，双击 OK 返回主页。在设置 → Wi-Fi 中使用手机二维码/
本地页面配置 2.4 GHz 网络，然后进入应用商店。
若使用同一手机热点，勾选热点模式并先保存，待 Passport 关闭配网 AP 后开启手机热点；
支持的 iPhone 开启“允许其他人加入”和“最大兼容性”。

## 更新、应用和源码

- [分发站点](https://leiyin321.github.io/passportOS/)
- [源码及 SDK](https://github.com/leiyin321/passportOS)
- [本地/fork 开发和协作指南](README.zh_CN.md)
- [实际验收及范围](../docs/test-reports/public-distribution.zh_CN.md)

真机已通过公共 HTTPS OTA、最新版检查、Native 商店浏览及强制下载安装。
完整首次安装镜像已生成并校验，但尚未在备用板进行覆盖式刷入实测。
音频/AI 内容服务由各应用及其后端提供者维护，不由 Pages 提供。
Native 是由 OS 维护者审核签名的受信代码，不是隔离进程。
开发者可 fork 单仓库，使用自己的密钥和本地静态服务，成熟源码可提交 PR 审核合并。
