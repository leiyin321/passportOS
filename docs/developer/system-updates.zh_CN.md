<p align="right">
  <strong>简体中文</strong> · <a href="system-updates.md">English</a>
</p>

# Passport OS 系统更新

当前公共更新源为 `https://leiyin321.github.io/passportOS/updates/latest.json`，商店位于同一 Pages 项目的 `/apps/index.json`。默认地址及 fork 公钥配置见[分发指南](../../distribution/README.zh_CN.md)，有效保存的地址覆盖项优先；HTTPS 和私有局域网 HTTP 均必须验签。

## 首次安装与后续升级的边界

首次安装 Passport OS 仍需通过 USB 刷入固件。设备运行带系统更新服务的
Passport OS 后，后续版本可在 **Settings → System Update** 检查、下载、校验和安装。
更新器只写入非活动 OTA 应用槽，不格式化应用包或应用数据分区。

## 发布服务器目录

在同一个公网 HTTPS 域名下托管 `latest.json` 和同目录的应用镜像：

```text
https://updates.example.com/passport/stable/latest.json
https://updates.example.com/passport/stable/passport-os-1.2.3.bin
```

设备使用 ESP-IDF 证书包验证 TLS 证书。设备必须配置该系统更新源，且每份固件清单
还必须通过 Passport OS 内置 RSA-2048 系统发布密钥验签。App Store 发布者密钥或 TLS
本身都不足以授权固件。

## 生成签名发布

使用已验证固件归档里的**仅应用镜像**，不要使用首次安装的合并镜像。私钥是系统内置
发布密钥，必须保密并离线保管；本仓库忽略的 `local/publisher-private.pem` 仅用于当前
开发设备。

为让 Settings → About 显示与清单相同的固件版本，构建签名发布时将版本传给
`PASSPORT_OS_VERSION`，并确保与 `--version` 相同：

```sh
PASSPORT_OS_VERSION=1.2.3 ./tools/validate.sh
```

```sh
passport firmware-release \
  build/firmware/<archive-sha>/FoloToy-AI-Passport.bin \
  --version 1.2.3 --build 2026102501 \
  --key local/publisher-private.pem \
  --public sdk/publisher-public.pem \
  --output build/system-updates/stable
```

`--build` 必须是严格递增整数（1..2,147,483,647），不能复用或降低。将生成的
`latest.json` 和 `.bin` 一起发布到同一个 HTTPS 目录。签名覆盖以下 UTF-8 字节串：

```text
passport-firmware/1
chip=esp32c3
version=1.2.3
build=2026102501
size=<应用镜像字节数>
sha256=<小写镜像 SHA-256>
image=passport-os-1.2.3.bin
```

清单格式为 `passport.firmware/1`，包含以上字段和 256 字节 RSA 签名（小写十六进制），
最大 4 KiB。镜像必须为 4 KiB–2.5 MiB；设备从清单所在目录获取镜像。更新器会拒绝
未知/重复字段、非递增版本号、错误签名、大小/哈希不匹配、无效 ESP 镜像和不可信域名。

## 配置并在设备上安装

通过受信 USB 开发通道配置一次 HTTPS 清单 URL：

```sh
passport device update-source \
  https://updates.example.com/passport/stable/latest.json
```

随后通过实体按键操作：

1. 打开 **Settings → System Update**，按 OK 检查更新。
2. 存在签名有效的新版本时再按 OK，将其下载到非活动槽；页面同时显示已下载/总字节数和 10 格百分比进度条。
3. RSA 签名、长度、SHA-256 和 ESP 镜像校验通过后，再按一次 OK 安装并重启。

首次启动时，Passport OS 会在候选镜像待确认状态下运行现有的初始化/存储自检。只有
自检成功后才记录已接受发布序号并标记 OTA 槽有效。自检失败或确认前复位时，ESP-IDF
Bootloader 会回到上一个槽。下载期间中断不会切换当前启动槽；从设置重试即可。

更新源属于系统配置。MiniApp 无法配置更新源、请求 OTA 写入或选择启动槽。不要发布
使用不同分区表、芯片目标或不兼容 Bootloader 构建的镜像。从厂商固件首次迁移仍须
通过 USB 刷写，见[固件发布与恢复](firmware-release.zh_CN.md)。

## 在 macOS 本机提供测试更新

将签名镜像发布到 `build/system-updates/local-update/`，再分别在两个终端启动
本机 HTTP 服务和 HTTPS Quick Tunnel：

```sh
PYTHONPATH=sdk python3 -m passport firmware-release \
  build/firmware/<archive-sha>/FoloToy-AI-Passport.bin \
  --version 1.0.0 --build 2026102502 \
  --key local/publisher-private.pem --public sdk/publisher-public.pem \
  --output build/system-updates/local-update
python3 tools/serve_system_updates.py --port 8765
cloudflared tunnel --url http://127.0.0.1:8765 --no-autoupdate
```

使用 `passport device update-source` 配置生成的
`https://<random>.trycloudflare.com/latest.json`。测试期间需保持两个进程运行。
Tunnel 为设备提供受信任的公开 TLS 端点，更新文件仍由 Mac 提供；临时 URL 会在
Tunnel 重启后改变。该方式仅用于开发测试，不保证生产可用性。

若设备和 Mac 在同一可信 Wi-Fi 网络，优先直接走局域网，避免公网 Tunnel 数据流中断。
将测试服务器绑定到 Mac 的私有 LAN 地址，并把该地址/端口显式配置为更新源：

```sh
python3 tools/serve_system_updates.py --host 192.168.1.10 --port 8765
passport device update-source http://192.168.1.10:8765/latest.json
```

固件只对显式配置的 RFC1918 私有 IP 和相同端口放行此 HTTP 开发路径；清单和镜像仍必须
通过内置 RSA 签名及 SHA-256 校验。公网更新服务器仍须 HTTPS；LAN 路径仅用于本地开发/测试。

固件清单请求超时为 15 秒，镜像流式读取单次超时为 60 秒，以适应较慢 HTTPS 隧道；App
请求保持 3 秒。固件传输总时限仍为 15 分钟。

## 验收状态

宿主测试覆盖发布签名、清单篡改检测、重复字段拒绝、文件名与大小边界。1.0.5 版本的
完整 `./tools/validate.sh` PASS，SDK 发布清单/镜像签名往返验签 PASS。归档位于
`build/firmware/c970187ec42e4ea05f992bcb94d309018e877d62386e1cd08b1eaf77988b5626/`。
应用镜像 1,840,224 字节，OTA 槽剩余 781,216 字节（29.80%）；首次刷机合并镜像
1,971,296 字节。应用 SHA-256 `5f4b470eee767c6a796dbc986a89134fb22ed242ed1db5179998c376eba794b7`；
合并镜像/归档 SHA-256
`c970187ec42e4ea05f992bcb94d309018e877d62386e1cd08b1eaf77988b5626`。

2026-10-25，已先通过 USB 将已验证仅应用镜像写入 `0x20000`；分区表、NVS、packages 和
App Data 均未写入。随后 Mac 在 `http://192.168.1.10:8765` 通过局域网提供签名版本
1.0.5/build 2026102507。设备日志确认请求完整 1,840,224 字节、启动进入 `ota_1`，并通过
系统启动自检、确认槽有效（`OTA self-test valid rc=0`）；用户确认更新成功。Mac 服务仍在运行，
设备更新源已保存为该局域网地址。

Quick Tunnel 下载在 223,312 字节时出现 ESP32-C3 mbedTLS 读取错误 `-0x7100`，尽管 Mac
源站已向 Tunnel 发送完整文件，因此本次改用局域网路径。此开发 HTTP 路径仅允许显式配置的
RFC1918 IP/端口；RSA 签名、精确长度、SHA-256 与 ESP 镜像检查仍强制。Wi-Fi 启动时空闲堆
86,728 字节、历史最小空闲堆 86,672 字节；OTA 下载期间峰值 RAM 尚未测量。传输中断恢复、
强制回滚和 App 私有数据内容仍待单独验收。


## 1.0.6 发布记录（2026-10-25）

- System Update 检查到当前已安装 build 或更旧的签名 build 时显示“Passport OS 已是最新版本”，不进入下载/安装流程。
- 下载页保留已下载/总字节数，同时显示 10 格百分比进度条和百分比数值。
- 发布版本 `1.0.6` / build `2026102508`，应用镜像 1,840,320 字节，SHA-256 `3ebcd4fca52e5d5fd248cd30c2a56393532fd65a48cbb501f35944365913dd76`。已通过本机签名和清单/镜像验签，更新服务 URL 为 `http://192.168.1.10:8765/latest.json`；设备安装验收待完成。


## 1.0.7 发布记录（2026-10-25）

- Settings → About 显示镜像内固件版本。签名清单版本 `1.0.7` 与构建时 `PASSPORT_OS_VERSION=1.0.7` 一致。
- 发布 build `2026102509`，应用镜像 1,840,384 字节，SHA-256 `2da846bd6315eafa67748db4e0eecc1c59d1e291e21475bd6584e07924c22cd7`。本机服务已提供 `http://192.168.1.10:8765/latest.json`，清单和镜像验证通过；用户确认设备系统 OTA 流程和 About 固件版本号显示均已验证通过。


## 1.0.8 语言版本发布记录（2026-10-25）

- 新增 English/简体中文系统语言选择，设置保存在 NVS；MiniApp 可通过只读 `passport.system.language()` 获取当前语言。
- 已本地化系统导航、设置项名称/状态/详情、状态栏/底部按键提示、配网与权限提示、商店/安装/更新通知及运行时错误。应用自己提供的名称和内容由应用负责翻译。
- 增加采用许可分发的思源黑体 SC 16 px 字形子集，生成的 C 源码为 110,986 字节，链接后的字体数据占 16,579 字节 Flash；缺字回退到 LVGL 内置 CJK 字库。
- build `2026102510`，ESP-IDF 5.5.3；应用镜像 2,061,808 字节，OTA 分区 2,621,440 字节，余量 559,632 字节（21.34%）。首次刷机合并镜像 2,192,880 字节。签名清单和镜像校验通过，SHA-256：`04a0334c46e55d69171939f9e98d3578e98220fcd79a59e1502527d6aa4f0cfe`。
- 已发布到 `http://192.168.1.10:8765/latest.json`；清单和镜像均返回 HTTP 200。设备 OTA 安装和实际屏幕语言切换验收仍待执行。
- `./tools/validate.sh --static` 与 `./tools/passport/test.sh` 通过，包括 18 项 Lua runtime 测试和 MiniApp 语言 API 测试；ESP-IDF 5.5.3 构建、合并镜像与固件布局校验通过。尚未测量设备中文绘制期间的 RAM 峰值。


## 1.0.9 OK 按键修复（2026-10-25）

- OK 按下时不再立即执行页面动作。确认短按/双击后执行一次；长按只执行长按动作。UP/DOWN 仍即时响应。MiniApp 的 OK 按下/抬起事件在短按确认后成对派发；长按 OK 返回主页，不会先收到 OK 短按回调。
- 新增 OK 按下/单击/双击/长按/抬起分类的主机回归测试。`./tools/passport/test.sh`、ESP-IDF 5.5.3 构建及固件布局验证通过。
- 已签名 build `2026102511` / version `1.0.9`；应用镜像 2,061,872 字节，OTA 分区余量 559,568 字节（21.35%）；合并镜像 2,192,944 字节。SHA-256：`4ab16642c806849d205caa5d4d9be440c349a5cb9c7de845b8f22ff28bc3043d`。已发布到本机更新服务；设备端按键与 OTA 验收待执行。


## 1.0.10 按键事件时序（2026-10-25）

- BSP 现在在按下时立即派发 `PRESS`；松开时若此前未派发长按则派发 `CLICK`，随后派发 `RELEASE`。按住达到 1,800 ms 门限时派发 `LONG`，之后松开仍派发 `RELEASE`，但不再产生 `CLICK`。快速连续按键各自判为短按，不等待驱动的双击合并窗口。
- 系统页面的 OK 动作在松开确认短按时执行；UP/DOWN 仍即时响应。MiniApp 的 OK 短按在松开时成对收到按下/抬起回调；长按 OK 会先退出应用，不触发短按回调。SDK 文档和 BSP 回归测试已更新。
- 已签名 build `2026102512` / version `1.0.10`；应用镜像 2,062,128 字节，OTA 分区余量 559,312 字节（21.33%），合并镜像 2,193,200 字节。SHA-256：`889c3256966b7efa46dc7adacb04ecf64ae56f792f2ad1340a28a1b743aeed0a`。清单和镜像已发布并通过本机更新服务校验；设备端长短按行为和 OTA 安装仍待硬件确认。

## 1.0.11 按键手势与商店版本（2026-10-25）

- 三个按键都提供即时按下/松开边沿，以及分类后的单击、双击、长按。单击 OK 确认；双击 OK 返回主页，并抑制组成双击的两次单击回调；长按 OK 派发给 MiniApp，可用于按下开始、松开停止的按住说话。
- App Store 和 Apps 库卡片显示应用版本。选择商店应用后会比较设备已安装版本与目录版本：未安装或较旧版本才下载/更新；已安装版本相同或更新时提示已是最新，再按 OK 打开应用。安装完成提示后按 OK 也会直接打开新应用，不会重复下载。
- 主机回归测试覆盖手势分类、MiniApp 回调、按住说话和严格语义版本比较。完整工程验证通过。
- 版本界面使用 320 字节静态缓存/状态，不分配堆内存；版本比较使用 24 字节栈空间保存数字分量。ELF 的 `.dram0.bss` 为 61,440 字节。尚未在真机测量运行时峰值空闲堆。
- 已签名 build `2026102514` / version `1.0.11`；应用镜像 2,063,888 字节，2,621,440 字节 OTA 槽余量 557,552 字节（21.27%）；合并镜像 2,194,960 字节。应用 SHA-256：`5c7d704229781e6bddad69269035215e47aa14097bfdc91da89cdacf509961ba`；合并镜像 SHA-256：`b21d6936204ac2a6b7e503e2303e90681454cd6ba29b106a331e9b9188eb6f54`。
- 已发布到本机更新服务 `http://192.168.1.10:8765/latest.json`；签名清单和镜像验证通过，两个服务端点均返回 HTTP 200。设备 OTA 启动及 App Store/Apps 卡片目视验收仍待完成。

## 1.0.12 中文“双击”提示字形修复（2026-10-25）

- 将系统 CJK 字体子集中缺失的“双击”字形补齐，确保返回主页提示能正常显示。
- 新增主机字形覆盖测试，同时检查 Unicode 映射和非空字形位图。
- 字体子集现含 132 个系统码点；生成 C 源文件 112,548 字节，链接字体数据占 16,817 字节 Flash，比上一版增加 238 字节。
- 已签名 build `2026102515` / version `1.0.12`；应用镜像 2,064,128 字节，OTA 槽余量 557,312 字节（21.26%）；合并镜像 2,195,200 字节。应用 SHA-256：`5589d4f8369c38c6c0d4ad872eaa29745d26b49de3cda152cb0652c2124c4be2`；合并镜像 SHA-256：`cb78fd52ed60d4ab2302fc3737c3cbc54c61a8aba69439bbf58835115adbd340`。
- 已发布到本机更新服务并通过签名/哈希验证。设备 OTA 和屏幕字形目视确认仍待完成。

## 1.0.13 UP/DOWN 连续单击（2026-10-25）

- UP/DOWN 不再进入双击判定。每次短按松开立即产生一个 `CLICK`，快速连续点按会逐次移动。OK 仍保留 180 ms 单击/双击判定；双击 OK 继续返回主页并抑制组成它的两次单击。
- MiniApp 会逐次收到 UP/DOWN 短按回调。`on_button_double` 保留兼容但不再派发；应用若需要方向键双击操作，可自行计时连续单击回调。
- BSP 回归测试覆盖 UP 和 DOWN 快速连按、忽略驱动双击、OK 双击抑制、长按和物理松开。完整工程验证与 ESP-IDF 固件构建通过。
- 已签名 build `2026102516` / version `1.0.13`；应用镜像 2,064,176 字节，2,621,440 字节 OTA 槽余量 557,264 字节（21.25%）；合并镜像 2,195,248 字节。应用 SHA-256：`8c30456a4f96ed26dc3c67277b6e31c3e455ca50f3a196fa19b58651444d7ebb`；合并镜像 SHA-256：`ef48b92b11f95289497582282f3ae2939ae1344109b2e478ae4bf80e8a66de56`。
- 已发布到本机更新服务并通过签名清单/镜像校验。设备 OTA 和实体按键连续导航手感仍待确认。

## 1.0.17 小智 MiniApp 版本（2026-10-25）

- 使用 `PASSPORT_OS_VERSION=1.0.17` 构建包含小智 MiniApp 及 OS 通用 HTTPS/WSS/Opus 改造的固件；固件构建和分区校验通过。
- 已签名并发布 build `2026102520` 到运行中的 Mac 更新服务：`http://192.168.1.10:8765/latest.json`。
- 应用镜像 2,278,640 / 2,621,440 字节（剩余 342,800 字节，13.1%）；SHA-256 `799c6daacfa47a5197d4ed9422052b76783fff2e06294a1076af186d8bbffe8e`。
- 首次安装合并镜像 2,409,712 字节；SHA-256 `5021f4fb6a7a6b04ce1682c8d7213b6754e9f3b0ac10c713c356e6ce61cbb43a`。
- 实际更新服务器的签名清单和全部 2,278,640 字节镜像均 HTTP 200；SDK RSA 验签、精确长度和 SHA-256 校验通过。本次没有触发设备检查或安装更新。

## 1.0.18 OTA 启动栈溢出修复（2026-10-25）

- 修复 1.0.17 启动时 `passport_http` worker 栈溢出导致回滚的问题。大型网络 job 改为堆所有权，FreeRTOS 队列仅传指针。
- 完整宿主及 ESP-IDF 5.5.3 验证通过。应用镜像 2,278,416 字节；2.5 MiB OTA 槽余 343,024 字节（13.08%）。合并镜像 2,409,488 字节；SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`。
- 已签名发布 `1.0.18` / build `2026102521` 到运行中的本机 Mac 服务 `http://192.168.1.10:8765/latest.json`。应用 SHA-256：`ecd9e121907b68b187deb1eeda97d5d28d5c7f602272793ef3fcbbb64d4cf99e`。已验证在线清单及完整镜像大小/哈希。
- 设备证据及匹配 ELF 的 panic 符号化记录见 `docs/test-reports/ota-1.0.17-rollback.zh_CN.md`。1.0.17 自动回滚到 1.0.16 已通过验证。随后用户安装了 1.0.18：`ota_1` 启动、存储自检通过（`OTA self-test valid rc=0`）、Wi-Fi 成功联网，About 显示 1.0.18。启动时最低空闲堆为 85,264 字节，网络任务栈高水位为 3,884 字节。

## 1.0.19 应用库卡片栈溢出修复（2026-10-25）

- 应用库卡片绘制时不再为获取版本号而验签/解析每个已安装包。列表刷新阶段预先读取版本，渲染器只查缓存。
- 完整宿主及 ESP-IDF 5.5.3 验证通过。应用镜像 2,278,416 字节；OTA 槽余 343,024 字节（13.08%）。合并镜像 SHA-256：`c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`。
- 已签名发布 `1.0.19` / build `2026102522` 到运行中的本机更新服务 `http://192.168.1.10:8765/latest.json`；清单和完整镜像的签名/大小/哈希均已验证。应用 SHA-256：`2b340c94f700ac2691ad03f1a5df61a62d1dc0c55749d61999e6358f84262f2e`。
- 匹配固件 panic 证据及复现/修复报告见 `docs/test-reports/app-library-card-crash.zh_CN.md`。安装 1.0.19 后的真机复测待完成。

## 1.0.20 系统外观与 MiniApp API（2026-10-26）

- Settings 新增持久化的深色/浅色外观选项。MiniApp 可通过只读 `passport.system.display_mode()` 获取所选模式；OS 不强制应用使用系统配色。`passport.text(value, rgb24)` 支持应用设置自己的文字颜色。小智 0.2.2 使用自有调色板，浅色模式为白底深色字。
- 签名 build `2026102601`、版本 `1.0.20` 已发布到 Mac 本地更新服务：`http://192.168.1.10:8765/latest.json`。应用镜像 2,280,576 字节；OTA 分区余量 340,864 字节（13.0%）；合并镜像 2,411,648 字节。
- 应用镜像 SHA-256：`d33d67b9d8bd80ce0c9ae9a61f838d2addbcf97f8f351795b820349f564e0998`；合并镜像 SHA-256：`91253dd4ebdb7250316b488babf975346ab8b1a91444d61b9fa2c853df928a45`；ELF SHA-256：`987927e90863dc8990fe05b64acb9909fbd900211cfb979534653fe3c82fba81`。
- ESP-IDF 5.5.3 完整固件构建和固件校验通过。修改后的静态/宿主门禁通过：运行时 23 项、包测试 13 项、编译器 12 项，以及仓库、BSP、显示、字体、OTA 和 API 检查。已从运行中的本机服务验证 OTA 清单/镜像签名和哈希。真机外观切换待验收。

## 1.0.21 MiniApp HTTPS 诊断（2026-10-26）

- 增加脱敏的 MiniApp HTTPS 提交及 worker 故障诊断日志。日志可区分 Wi-Fi/WebSocket/worker 队列拒绝，以及策略、内存分配、TLS/open、请求体发送、响应头和响应读取失败。不会记录 URL、请求体、设备身份或认证请求头。
- 签名 build `2026102602`、版本 `1.0.21` 已发布到本机更新服务。应用镜像 2,281,344 字节；OTA 槽余量 340,096 字节（13.0%）；合并镜像 2,412,416 字节。
- 应用镜像 SHA-256：`1d281b70a4ecebf63b0a036d3238561a9954fc7a9935837a7379e9c4b498da15`；合并镜像 SHA-256：`fabcb93fb2040fc7c47b52d58dca9a955f9823d21818c5217678223f82722bc6`；ELF SHA-256：`a11f1b8872cf127a0675edb771104cfa742896240c19b7c602f47b7c24e14d6f`。
- ESP-IDF 5.5.3 完整门禁通过（运行时 24/24）。已从本机更新服务获取清单和镜像，并通过 RSA 签名/大小/哈希验证。安装 OTA 后重新触发小智服务检查并捕获 `passport_http` 日志，以定位剩余原因。

## 1.0.22 审查与可靠性优化

签名 build `2026102603` 已由 Mac 服务提供。应用镜像 2,283,280 B，OTA 余量
338,160 B（12.9%），合并镜像 2,414,352 B。应用 SHA-256：
`fb59175bb4f6c07fe0cfd52d11864e150dba5a94cda78c1725d71a75021e2d8d`。完整构建/宿主门禁及归档验证通过，直连局域网获取
在线清单及完整镜像，签名/大小/哈希通过。设备安装/验收待完成。网络内存、HTTP 分帧、
音频、JSON、闹钟及 SDK 改动详见[审查报告](../development/passport-os-review.zh_CN.md)。

发布改为原子替换文件，最后提交 latest.json。已有版本镜像内容不可改写，请使用
新版本号及递增 build。不要对同一发布目录并发运行多个发布进程。

## 1.0.23 最终审查发布

签名 build `2026102604` 替代已暂存发布的 1.0.22。最终复查修复 GET 重定向的
缓冲所有权，禁止空请求体 POST 跟随重定向。真实网络 worker 测试新增受限分配、
重定向响应、分块尾部、短写、错误清理、500 次请求回收；启用 AddressSanitizer/UBSan，
socket/RTOS 行为仍由测试桩提供。

应用镜像 2,283,296 B，OTA 余量 338,144 B（12.9%），合并镜像 2,414,368 B。
应用 SHA-256：`7e6adede041783bcc37924e56201bc0c8756a335f0143aab6b86e6a065750f86`。
完整固件门禁/归档验证和在线局域网签名/大小/哈希通过。从设置安装 1.0.23 后进行
硬件验收，完整产物身份见[审查报告](../development/passport-os-review.zh_CN.md)。

## 1.0.29 小智音频任务分配修复

签名 build `2026102610` 已发布到运行中的 Mac 更新服务
（`http://192.168.1.10:8765/latest.json`）。现在音频任务先申请 9,216 字节任务栈，再
分配流队列，避免 WSS 建立后最大连续块减小导致任务创建失败。ESP-IDF 5.5.3 完整门禁和
宿主测试均通过。

应用镜像 2,289,120 B；OTA 余量 332,320 B（12.68%）；合并镜像 2,420,192 B。应用
SHA-256：`630446dbeb6bdb0d186a3aed53418386ee00f5bf0d5fe0379a7e346a2b178a63`；完整
镜像/归档 SHA-256：`4a758ee3d9e6b8e11c430cdfba3d2552b7ff2f261ca5cecae11a36e9a6f61944`；
ELF SHA-256：`a80313c29c73b5e5bcd67b2b200b3d71439840761c6d9a771135dcfe050518ad`。已从服务
下载并验证签名清单和完整镜像。编解码任务栈高水位、真机 RAM 峰值、成功按住 OK 通话和
退出清理需用户在设置中安装更新后测量。

## 1.0.30 小智音频预分配

1.0.29 设备复现了 WSS 建立后的任务栈分配失败：空闲堆 22,428 B，最大连续块 7,680 B，
而音频任务申请 9,216 B。1.0.30 打开声明 `audio.stream` 的应用时，在网络回调前创建
不采集音频的等待态任务。该任务在多次按住通话间保留，并于应用退出时回收。签名 OS 和
小智 0.2.8 已发布到本机；安装后需继续实测编解码内存、任务栈高水位、语音及退出清理。
签名版本 build `2026102611` 已上线：`http://192.168.1.10:8765/latest.json`。应用镜像
2,290,000 B；OTA 余量 331,440 B（12.64%）；合并镜像 2,421,072 B。应用 SHA-256：
`0b8af29458b0e8c8f9e33066c21a05b7461cd348f87525690f8218394bb4921a`；完整镜像/归档
SHA-256：`c1ad1929d76b4fbae290ec0ae679cc0386b8c8f00beed7cf65f06c32f6d8e90c`；ELF SHA-256：
`4943a6aa92fc263e9116b0a6b67a783fe727de0c0a8be480ce8fa07787b0b43d`。重新下载的 OTA 清单
和镜像通过签名/大小/哈希验证。App Store 已提供小智 0.2.8，线上包 SHA-256：
`0a05b02fb0d6b07b13ec452a531d5ab4320ddc79a5af749eef0466281ed3bef2`。

## 1.0.31 音频 API 兼容更新

保留网络请求前预留音频任务的行为，并使等待态任务同时兼容普通麦克风采集和 Opus 流。
签名 build `2026102612` 已在本机更新服务取代 1.0.30。应用镜像 2,289,984 B；OTA 余量
331,456 B（12.64%）；合并镜像 2,421,056 B。应用 SHA-256：
`eca67a0597bf1011cf4cece4d7949176e2bd8024a29bfd4414592297a5b6c277`；完整镜像/归档
SHA-256：`efa58799b17ab088d892c749af76dfba4050bfc6d7a9c29358268f9485e300d6`；ELF SHA-256：
`f525b78ef221570dff2543e35da44216b64cc825af62c5014076ec16c6dc6d15`。已从在线服务验证 OTA
清单和镜像的签名/大小/哈希。

## 1.0.32 不占用 TLS 普通堆的音频任务栈

1.0.31 设备的 WSS 证书签名验证报 mbedTLS `0x4290`，随后握手返回 `-0x3000`。预留的
9 KiB 任务栈占用了 RSA 验签所需的普通堆。1.0.32 使用 `xTaskCreateWithCaps` 将任务栈
放入 ESP32-C3 Retention RAM，并以 `vTaskDeleteWithCaps` 回收；TCB 仍在内部 RAM。ESP-IDF
和宿主完整门禁通过。签名 build `2026102613` 已发布到本机 OTA 服务。

应用镜像 2,290,112 B；OTA 余量 331,328 B（12.64%）；合并镜像 2,421,184 B。应用 SHA-256：
`e50eb345f1fbabda1fcba303d52aa4892584757ad3202b4f51e50b977efe244b`；完整镜像/归档 SHA-256：
`f365f8c0dbf45b616200c332d0acd8c49cab850b1a0881e36cc20da8b84b7b6d`；ELF SHA-256：
`0ce5b949821e3e74cb3f3dc07ef89d921f17cb206298b16ff7f3cd93d27136ae`。在线签名清单和镜像
下载及哈希核验通过。App Store 中的小智 0.2.8 继续有效；真机 WSS/音频验收待完成。

## 1.0.33 延迟音频栈分配

用户在 1.0.32 上再次复现 WSS 错误。虽然音频栈位于 Retention RAM，该区域仍可被
mbedTLS 的 `MALLOC_CAP_INTERNAL` 请求使用，因此和 TLS 共用内部 RAM。OS 1.0.33 不再于应用
启动或麦克风授权时创建工作线程；只有实际开始采集时才创建带 Retention capability 的
9,216 字节工作栈。运行时堆指标已分别记录 Internal 与 Retention 的空闲总量和最大连续块。

ESP-IDF 5.5.3 全量验证通过：仓库/主机测试（运行时 33/33、包测试 13/13、资源编译器 12/12，
以及 BSP/网络/OTA 检查）、固件分区及合并镜像检查均通过。应用镜像 2,290,064 B，占
2,621,440 B OTA 槽；余量 331,376 B（12.64%）。合并首刷镜像 2,421,136 B。应用 SHA-256：
`94335b5472da45f44963fc179520718a8d911666cc6935514f20948cb7dc10f6`；完整镜像/归档 SHA-256：
`80e51f89fee7b5506682f9c33bd8ed7d7f345a54e46608139a889154eb47f48f`；ELF SHA-256：
`3b13c485917542593e48ead7f68eaf6ac83fcf0319d23c0b50fb747dfd0a2e38`。签名 build
`2026102614` 已发布到 `http://192.168.1.10:8765/latest.json`；已下载线上清单和完整镜像并验证
RSA 签名、大小及 SHA-256。仍需真机安装并验收 WSS hello、PTT/TTS、RAM 峰值和退出回收。
