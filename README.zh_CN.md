[English](README.md) · **简体中文**

# Passport OS

面向 FoloToy AI Passport 的小型操作系统：ESP32-C3、8 MiB Flash、无 PSRAM、
彩屏及三键。Native ELF 是主要应用方向，保留现有 Lua 应用兼容能力。

## 普通玩家

从 AI Passport 玩法社区安装审核后的首次安装固件。在设置中连接 Wi-Fi，
通过应用商店下载应用，后续从设置 → 系统更新升级 OS。系统和应用独立更新。

公共分发：<https://leiyin321.github.io/passportOS/>。
[安装及分发指南](distribution/README.zh_CN.md)说明首次安装、恢复、本地服务和发布流程。

## 开发者

使用 ESP-IDF 5.5.3，按[环境指南](docs/development/engineering/environment-setup.zh_CN.md)
准备后运行 `./tools/validate.sh`。源码与工具采用单仓库：

| 目录 | 用途 |
| --- | --- |
| `main`、`os`、`components` | 桌面、系统服务、Native/Lua 运行时及 BSP |
| `sdk`、`protocol` | Native C ABI、打包/签名/设备工具及协议 |
| `examples` | Native 小智、城市电台、蓝牙外设及示例 |
| `distribution`、`tools/passport` | 精选索引、静态站点构建及本地发布工具 |
| `.github/workflows` | PR 验证及手动控制的 Pages 发布 |

从 [Native 开发](docs/developer/native-apps.zh_CN.md)开始。
`python3 -m pip install ./sdk` 安装 SDK，`passport init` 默认创建 Native 应用；
Lua 兼容项目使用 `--runtime lua`。二次开发可使用自己的 RSA 公钥和本地更新服务。
签名私钥不得放入源码、固件、Pages、PR 或公共产物。

## 协作及发布

通过 PR 提交源码，附主机测试、ABI/权限要求和设备验收。维护者审核、构建并签名，
PR 工作流不持有签名密钥或部署权限。Native 为受信代码，需要 OS 发布者签名；
授权 Lua 发布者不等于授权任意 Native 代码。

参阅[贡献规则](.github/CONTRIBUTING.zh_CN.md)、[分发指南](distribution/README.zh_CN.md)、
[SDK](docs/developer/sdk-cli.zh_CN.md)及[硬件文档](docs/README.zh_CN.md)。
项目使用 MIT 许可，保留上游版权及各[第三方许可证](distribution/THIRD_PARTY.zh_CN.md)。
