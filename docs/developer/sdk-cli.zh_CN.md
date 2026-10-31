[English](sdk-cli.md)

# Passport MiniApp SDK 与 CLI

在仓库根目录安装可编辑 SDK；USB 命令需要 `pyserial`。RSA 签名/验签依赖 OpenSSL。图像工具为可选依赖，通过 `.[assets]` 安装。

```sh
python3 -m venv .venv
.venv/bin/pip install -e ./sdk
.venv/bin/pip install -e './sdk[assets]'  # 仅在编译 PNG/SVG 资源时需要
passport --help
passport api > local/api-v1.json
```

此开发环境中的 USB 命令默认使用 `/dev/cu.usbmodem101`。其他端口或操作系统请对 `install`、`trust`、`wifi` 或 `device` 显式传入 `--port PORT`。每条命令会独占串口执行；不要并发运行 USB 命令。

## 命令参考

| 命令 | 用途 |
| --- | --- |
| `passport init DIR --id org.example.app [--runtime lua|declarative|native-elf]` | 创建 Manifest 和入口模板 |
| `passport keygen --private FILE --public FILE` | 创建 RSA-2048 发布者密钥对；拒绝覆盖已有文件 |
| `passport pack DIR --key PRIVATE --public PUBLIC -o FILE` | 校验源码、编译已声明图像资源、签名并验签 PAP1/PAP2 |
| `passport validate FILE --public PUBLIC` | 校验包长度、签名、密钥身份、入口摘要、Manifest 和声明式 UI 预检 |
| `passport publish-index DIR --public KEY [--public KEY ...] --base-url HTTPS_URL -o INDEX` | 验证包并生成静态目录索引 |
| `passport serve DIR [--host 127.0.0.1] [--port 8040]` | 通过无身份验证的 HTTP 提供开发目录 |
| `passport assets build DIR` / `inspect FILE` / `report FILE` | 编译和检查 PAI1 图像及 PAP2 资源预算，见[资源指南](assets.zh_CN.md) |
| `passport trust --public PUBLIC [--port PORT]` | 发送发布者公钥，并在设备上确认密钥 ID |
| `passport install PACKAGE --public PUBLIC [--port PORT]` | 使用有界 USB READY/ACK 协议发送已验证应用包 |
| `passport device 操作 ... [--port PORT]` | 调用系统开发命令，见下方列表 |
| `passport wifi --ssid SSID [--port PORT]` | 通过 USB 发送 Wi-Fi 凭据，密码使用隐藏输入 |

`passport device` 操作包括：`list`、`metrics`、`home`、`launch ID`、`offload ID`、`pin ID on|off`、`remove ID`、`view`、`reconnect`、`service IP HTTP_PORT WS_PORT`、`catalog URL`、`update-source URL`、`screen on|off`、`icons`、`cancel_cycle` 和 `data_hash ID KEY`。`remove` 必须输入 `DELETE` 确认；它会同时删除应用私有数据和包。USB 是受信开发通道，MiniApp 无法访问。

## 发布者身份与应用生命周期

为应用身份生成并长期使用一个发布者密钥，将私钥放在 Git 忽略的 `local/` 等目录中。包身份和私有数据归属由 `(publisher_key_id, app id)` 共同决定。更新应用时保留原密钥；新发布者不能接管已被其他密钥注册的 ID，包括已 Offload 的应用。设备内置一个发布者，最多还可注册四个额外 RSA-2048 发布者。`passport trust` 会显示密钥 ID，但仍须由用户在设备上确认。

输入不变时，`passport pack` 生成确定性签名包。PAP1 携带单个文本入口；PAP2 携带入口和编译后的资源。签名覆盖完整包内容。`passport validate` 不会执行 Lua，也不保证真机视觉正确；发布前还应运行模拟器/运行时测试，并在目标硬件上验收。

## 静态商店与本地开发服务器

当前设备最多接受 `passport.catalog/1` 的八个条目。`passport publish-index` 从 `.pap` 文件生成索引；CLI 会用重复传入的 `--public` 发布者公钥验证每个包，并拒绝重复 App ID。生成目录的规范是 [`catalog-v1.schema.json`](../../protocol/catalog-v1.schema.json)。若要扩大公开商店，需先为设备 App Store 增加分页，再提升当前八项限制。

索引本身没有独立签名。它是由配置的 HTTPS 源提供的发现元数据；下载的每个包仍会分别校验目录 SHA-256 和发布者签名。主机 CLI 收到的公钥仅用于生成索引时验包，不会把发布者注册为设备受信密钥。设备信任仍需用户单独确认。

`passport serve` 默认仅绑定本机。若要让已授权的测试设备从可信局域网访问，可运行 `passport serve dist --host 0.0.0.0 --port 8040`；这会在无身份验证的情况下开放 `dist` 中所有文件。私钥和其他本地私有文件必须放在该目录之外。生产发布应将索引、应用包和图标部署到 HTTPS 静态托管。MiniApp 需要的动态后端由各应用开发者自行维护。

## 校验边界

- `manifest-v1.schema.json` 描述源码编辑态和打包后两种字段形态。SDK 打包时还会执行跨字段、文件路径等严格检查；固件仍是最终权威。
- `ui-v1.schema.json` 和 `passport.declarative.validate_ui()` 校验当前支持的声明式结构和资源限额；实际绘制效果仍需设备验收。
- Lua 文件会检查 UTF-8、NUL 和大小并参与签名，但打包器不会执行代码。模拟器用于检查宿主包校验无法覆盖的运行行为。
- `passport publish-index` 会验证包内容和签名，但不会替设备注册发布者或执行人工审核。
- `passport serve` 是本地开发服务，不是互联网服务或生产部署工具。

另见[快速开始](quickstart.zh_CN.md)、[包协议](package-protocol.zh_CN.md)、[Capability API](capability-api.zh_CN.md)、[声明式 UI/1](declarative-ui.zh_CN.md)、[资源指南](assets.zh_CN.md)和[显示布局](display-layout.zh_CN.md)。

## 可靠的串口诊断与发布

OS 1.0.22 实现 `pin ID on|off`，旧的 `pin ID` 表示置顶；`data_hash` 需要 `ID KEY`。
用同一个连接等待命令完成并采集后续 worker 日志，例如：

```sh
passport device --port /dev/cu.usbmodem1101 --verbose --observe 20 launch org.passport.xiaozhi
```

`--observe` 限制为 0..60 秒；`--verbose` 打印全部串口行，否则保留原有 Passport/metric
过滤。没有收到完成响应或收到明确 FAIL 时 CLI 返回非零。不要同时运行另一串口监视器。
分享前检查日志：应用文字及平台日志可能包含私人数据。

打包与目录发布原子替换输出文件。固件发布先提交完整镜像，再原子切换 latest.json，
拒绝倒退 build，拒绝把不同内容写入已有版本镜像。使用单发布进程；这些原子写入
不提供分布式发布锁，也不等价于公共托管平台的部署事务。

## Native ELF

固件 1.0.34 起可通过 `passport init DIR --id org.example.native --runtime native-elf` 创建项目，使用 `passport native build DIR` 编译、`passport native inspect DIR/app.app.elf` 检查。签名、安装与启动沿用现有工具；原生应用只允许内置 OS 发布者。完整 ABI、限制和示例见[原生应用开发](native-apps.zh_CN.md)。

Native ABI 2 从 OS 1.0.35 起提供通用网络、身份、存储及音频服务。Manifest 使用 `abi:2`，按权限和域名声明能力；ABI 1 保持原限制。详见 [ABI 2 服务](native-services.zh_CN.md)及[原生小智](xiaozhi-native.zh_CN.md)。
