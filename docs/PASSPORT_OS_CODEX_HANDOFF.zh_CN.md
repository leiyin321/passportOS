[English](PASSPORT_OS_CODEX_HANDOFF.md)

最新应用检查点：Native 小智 1.0.5 增加中英文后台网址、二维码及激活引导。完整构建与主机测试通过；USB 未检测到，新页面扫码及实机资源测量待验证。OS 保持 1.0.75，后续系统与应用更新统一走线上 Pages 服务。


最新检查点：OS 1.0.75（签名构建 2026110102）已直接通过公共 Pages OTA 安装并验证启动。源码 https://github.com/leiyin321/passportOS 已公开，Pages https://leiyin321.github.io/passportOS 提供签名系统更新及三个 Native 应用。本地/GitHub Linux 主机及固件门禁、Pages 部署、全部公共产物签名/哈希、最新版检查、Native 商店强制重装均 PASS。公共 OTA 210.83 秒，蓝牙包重装 4.23 秒。应用 2,423,200 字节，距 2.5 MB 余 76,800，见[公共验收](test-reports/public-distribution.zh_CN.md)和[分发指南](../distribution/README.zh_CN.md)。Release 提供 8 MiB 首次安装镜像及 SDK wheel；备用板覆盖式首次安装和社区文章发布不在本次已验收范围。原工作区/上游 remote 保留，独立发布仓库在 `local/open-source/public-repository`。无 USB Flash 写入/擦除，串口关闭。

商店检查点 — 2026-10-31：按用户要求下架旧 Lua 小智（`org.passport.xiaozhi`，0.2.8），从实时索引和下载目录移除。保留 Native 小智 1.0.4、蓝牙外设 0.1.4、城市电台 0.1.2 三项。HTTP 索引、旧下载 404、其余包大小/哈希检查 PASS。保留源码示例、设备已安装应用和持久数据。
# Passport OS — Codex 开发交接与验收任务书

> 版本：v1.0 · 2026-10-25  
> 目标硬件：FoloToy AI Passport / ESP32-C3 / 8 MB Flash / 无 PSRAM / 240×320 LCD / 三键 / ES8311 音频  
> 定位：ESP-IDF/FreeRTOS 之上的轻量应用操作系统；**交付实际代码、可刷写固件、开发者工具、示例应用和实测报告**。  
> 本文是交给 Codex 的执行规范。未被硬件实测证实的指标都属于待验证目标，不得写成已完成事实。

## 当前检查点 — 2026-10-29（后续用户指令）

设备 OS 为 1.0.50，通过设置 OTA 安装。本机商店提供原生蓝牙外设 0.1.3、原生小智 1.0.4 和 Lua 小智 0.2.8。后续用户授权和 [Native 计划](PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.zh_CN.md)替代下文最初的仅 Lua 限制。可信 Native ABI 1／2 与 Lua 共存；ABI 2 增加可选 96 字节 HID 宿主表，前 80 字节核心保持不变。保留用户数据及 OTA／数据分区，“设置 → 声音”继续提供持久化媒体／闹钟音量。

用户已确认蓝牙自定义手势验收：iPhone 12 Pro Max／iOS 27、OS 1.0.50／原生 0.1.3 上，UP／DOWN 可上下滑动短视频，OK 可暂停／继续。推荐在 iPhone 辅助触控设备设置中，将鼠标按钮 2／3／1 分别映射到已保存的上划／下划手势及单点。此前直接拖动／滚轮未通过，不能由此次成功推定其通过；其他设备／应用、Consumer 媒体控制及长期重连仍待验证。C3 硬件不支持经典蓝牙耳机音频／通话。保持手机唤醒：当前配置不宣称远程唤醒，宿主挂起时停止报告。

最新完整 OS 构建及应用主机门禁通过。历史 OS 1.0.48 完成十轮蓝牙生命周期及原生语音播放／录音、ABI 1／Lua 启动回归；不能将其资源测量当作新版本全部真机验收。恢复时核对[蓝牙检查点](test-reports/bluetooth-peripheral.zh_CN.md)、[iPhone／SDK 指南](developer/bluetooth-peripheral.zh_CN.md)、[原生报告](test-reports/xiaozhi-native.zh_CN.md)、[显示报告](test-reports/display-performance.zh_CN.md)及[音量报告](test-reports/system-volume.zh_CN.md)中的精确版本、产物及未完成项。原始日志和密钥留在本机，未执行 Git 提交／推送。

## 0. 立即给 Codex 的指令

你是 Passport OS 的首席实现工程师。请基于 FoloToy/ai-passport 官方 `main` 分支建立独立 `feature/passport-os` 分支，先阅读 `AGENTS.md`、硬件指南、BSP 公共接口、`bsp_pins.h`、`partitions.csv`、`sdkconfig.defaults` 及实际源码。**禁止照搬官方硬件测试演示菜单作为最终 OS UI；复用 BSP，不重写已工作的硬件驱动。** 按本文里程碑持续实现、测试、记录并交付，不要仅生成规划文档。每个阶段都要有自动化测试和可验证的运行结果；没有真机时完成可运行的宿主模拟器与构建，并明确标记真机测试为待执行，绝不伪造结果。所有阶段均保存可追踪的提交/任务状态。

### 执行优先级
1. 先保证可构建、可显示、可操作、可联网、可稳定恢复。
2. 再完成动态应用、数据保护、网络权限与缓存。
3. 再完成脚本小游戏、音频能力、App Store/SDK。
4. 最后按真机余量决定 Mesh、蓝牙、蜂窝扩展和更高级执行模式是否进入当前版本。

### 硬性成功定义（Definition of Done）
- 设备开机进入 Passport OS Launcher，不依赖 PC 常驻进程。
- Wi-Fi STA 能配网、连接手机热点/路由器，能重新连接并有可见状态。
- 用户可以从应用目录打开**此前未烧录在固件中的第三方应用**；运行、退出、重新启动，无需刷机。
- 支持 **声明式静态应用**和 **本地脚本应用**，并统一使用受控 Capability API。
- 应用代码缓存与 App Data 分离；清除代码后再次下载，保留并恢复游戏进度。
- 小程序可发起授权范围内的公网 HTTPS/WSS 以及获准本地 HTTP/WS；不可获得底层 Socket / 扫描 / 原始 Flash 权限。
- 至少完整测试：天气、计数器、俄罗斯方块、声控小游戏、WebSocket 聊天/实时数据（五类）。
- 有模拟器测试、错误/安全测试、真机实测计划和记录；OTA 方案通过断电/回滚设计验证后才允许作为正式更新手段。

## 1. 参考项目与技术选型

**先读后做：**
- 官方硬件：<https://github.com/FoloToy/ai-passport>，尤其 `AGENTS.md`、`components/bsp/`、硬件开发指南。
- 应用生态/Loader：<https://github.com/espruino/BangleApps>、<https://github.com/espruino/EspruinoAppLoaderCore>。
- Script VM 备选：<https://github.com/Moddable-OpenSource/moddable>（XS/Mods 支持 ESP32-C3，但与现有 LVGL 集成和 RAM 余量未验证）。
- Lua 微控制器经验：<https://github.com/nodemcu/nodemcu-firmware>。
- LVGL + 应用管理模式：<https://github.com/ByteWelder/Tactility>。**不要照搬其原生 ELF 动态执行。**
- ESP-IDF 官方：<https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/>（细节以所用 IDF 版本文档为准）。
- ESP-IDF WebSocket 组件：`espressif/esp_websocket_client`；HTTP 客户端：`esp_http_client`。

**暂定：** ESP-IDF 5.5.3（以官方工程要求为准）、FreeRTOS、官方 BSP、LVGL、受限声明式解释器、裁剪 Lua VM、LittleFS/NVS 或经实测确认的存储方案、系统托管网络客户端。不得同时引入 Lua/XS/WASM 三套 VM。M0 结束后对 Lua 做小型资源 PoC；若资源指标不达标，比较 XS 或更小 VM，并形成 ADR 决策记录。**API 不与 VM 绑定。**

## 2. 明确硬件约束

- ESP32-C3，单核，约 400KB 片上 SRAM，**无 PSRAM**，8MB Flash。
- 240×320 RGB565 LCD，驱动 ST7789P3，显示经官方 BSP/LVGL 接入。单份全屏 RGB565 缓冲约 150KiB，**不可为每个应用单独分配全屏缓冲**。
- UP/DOWN/OK 为同一 ADC 电阻梯输入，禁止另起一套 ADC 驱动抢占。
- ES8311 I²S0 全双工音频，录/放走 BSP worker；不要在 UI 线程阻塞录音。
- CW2017 电量表与 ES8311 共享 BSP I²C0，禁止重复初始化总线。
- 原生 USB Serial/JTAG 占 GPIO18/19；不能按常规 USB Host 设计插入 USB 4G Modem。GPIO21 为背光，勿当默认 UART TX。
- 官方默认只有 Factory App、NVS、PHY，**没有 OTA 分区**；重新分区是破坏性变更，刷写前必须明确备份/恢复说明。
- 官方 demo 不等于已有 Wi-Fi 配网、长期网络服务、BLE Mesh、ESP-NOW 多跳或 OTA 系统。
- Wi-Fi 与 ESP-NOW 在 ESP32-C3 上受同一个 2.4GHz 射频/信道限制。**第一版不承诺多跳语音 Mesh 可用。**
- 资源测量至少记录：boot 后、Wi-Fi 后、TLS 后、WebSocket 后、LVGL 后、Lua 后、运行音频后：free heap、minimum free heap、largest free block、各任务 stack high watermark、CPU/帧率、Flash 镜像大小。

## 3. 产品边界和非目标

### V1 必须具备
系统 Launcher、设置、Wi-Fi STA、App Catalog、应用加载、受限 UI、Lua 游戏、应用数据、网络能力、安全权限、资源回收、可维护的 OTA 方案、开发者工具和验证应用。

### V1 可保留抽象但不要阻塞主线
Remote UI 动态后端应用、先进 CAS 去重、模块级流式代码装载、复杂账号云同步、原生 ELF 动态加载、JS/XS/WASM 同时支持、多人常驻应用、多任务应用后台、ESP-NOW 多跳语音、Bluetooth Mesh Provisioning、Cat-1 扩展和 BLE 网关。若工作量/内存允许，可将 Remote UI 作为扩展里程碑实现。

### 安全禁令
禁止第三方 App 获取原始 Flash、任意 GPIO、FreeRTOS Task 创建权、系统 OTA、底层 TCP/UDP Socket、LAN 端口扫描、任意 Wi-Fi 配置、原始 ESP-NOW、原始 BLE Mesh 网络密钥或配置权。第三方不可执行不可信原生 C/ELF，不可调用 Lua `io`/`os`/`debug` 或不受控 native module。

## 4. 推荐系统架构

```text
Apps:      Weather / Counter / Tetris / VoiceBird / WS Chat
             │
Runtime:   Declarative Runtime  │  Script VM (Lua)  │ Remote UI [later]
             └─────────────┬──────────────────────┘
Capability: UI | Canvas | Input | Timer | Device | Storage | HTTP/WS | Audio | Channel
             └─────────────┬──────────────────────┘
Services:  App/Package | Permissions | Resource | Display | Audio | Network | Storage
           Power/OTA | Mesh Manager [system-only extension]
             └─────────────┬──────────────────────┘
Hardware:  Official BSP + ESP-IDF / FreeRTOS
```

设计不变量：
- 只有一个前台第三方 App Instance；系统 Launcher/Network/Storage/OTA 为系统服务。
- App 不能直接持有硬件；获得系统发出的有作用域的 opaque handle。
- 任何 handle、timer、LVGL object、音频订阅、HTTP/WS 都必须挂到 `AppSession`，退出可一次性取消、回收。
- UI 仅由 UI Task 统一更新，网络和音频回调不得直接调用 LVGL；遵循 BSP LVGL 锁。
- 后台 Mesh 中继（若未来实现）是系统服务，不依赖对讲应用是否运行。
- 系统应用（Launcher/Settings/Store）为信任域代码；不是靠 Manifest 标注 `system:true` 获得权限。

### 建议目录
```text
passport-os/
  components/bsp/                 # upstream BSP; 尽量不改
  main/                           # OS bootstrap
  os/core/                        # app lifecycle, event, resource, errors
  os/services/{display,input,audio,network,storage,power,ota,mesh}/
  os/security/{identity,permission,endpoint_policy}/
  os/package/{manifest,verify,store,eviction}/
  os/runtime/{declarative,lua}/
  os/system_apps/{launcher,settings,store}/
  protocol/                       # versioned C structs, JSON/CBOR schema
  sdk/                             # lua binding, declarative builder, CLI
  simulator/                       # native host/browser simulator
  examples/{weather,counter,tetris,voicebird,ws-chat}/
  server/dev/                      # local static package & WS test server
  tests/{unit,integration,security,hardware}/
  docs/{architecture,adr,api,package,storage,security,test-reports}/
```
目录按实际 ESP-IDF CMake 规则调整；保留官方源码授权及来源信息，检查第三方依赖许可证。

## 5. Application Identity / Package / Instance / Data 四实体

**Identity**：规范化 App ID、可信 Publisher Key/证书、版本、收藏入口、授权记录；应用所有权由签名和信任链判定，而不是 App ID 字符串。  
**Package**：可重新获取的脚本/UI/图片/音效/元数据；可缓存、更新、淘汰，完整性校验。  
**Instance**：RAM 中的 UI、脚本状态、请求、任务与资源作用域；退出时完全销毁。  
**Persistent Data**：与 Package 独立，游戏进度、设置、用户私有 KV；用户明确彻底卸载才删除，版本升级需要迁移。

用户操作：Exit（只释放实例）、Offload（删除可恢复代码/资源，保留数据及入口）、Remove & Delete Data（明确确认后删除包、数据、入口）、Pin Offline（尽力固定缓存，不承诺离线网络功能）、Reinstall（下载验证后恢复旧数据）。缓存空间不足时只可自动 Offload，绝不可自动删 App Data。

数据 schema migration 需原子化：旧数据保持可恢复，新版本迁移成功后才确认切换。紧急缺空间时提示用户而不是破坏数据。

## 6. Manifest / Package v1 规范

开发者输入格式为目录下 JSON + Lua，打包为 `.pap`（Passport Application Package）。初期格式可采用简单归档/长度前缀 + 哈希清单，避免嵌入式设备必须解压复杂 ZIP；包具体二进制规格须配有宿主编码器、解析器、向后兼容测试和防畸形 fuzz 测试。

Manifest 示例（示意，不代表已实现）：
```json
{
  "format": "passport.app/1",
  "id": "org.example.tetris",
  "publisher_key_id": "example-key-1",
  "version": "1.0.0",
  "runtime": "lua",
  "entry": "main.lua",
  "min_os_api": 1,
  "data_schema": 1,
  "permissions": ["ui.canvas", "storage.private"],
  "limits": {"script_heap_kib": 24, "fps": 20, "persistent_kib": 32},
  "assets": [{"path":"sprites.bin","sha256":"...","size":4096}],
  "signature": "..."
}
```

需要独立版本：Package Format、OS Firmware、Capability API、App Version、Data Schema。Package Downloader 支持静态 HTTPS URL、大小限制、SHA-256 校验、签名身份验证、原子写入切换、断电恢复、旧版本回退；拒绝路径穿越/超长字段/重复键/超限资源。**不能以网络 TLS 本身代替包签名与发布者身份。** 开发模式可接受一次性用户确认的本地未签名包，但不得继承生产应用数据命名空间或高权限。

## 7. Capability API v1（严格白名单）

| API | 范围/规则 | 可用应用 |
|---|---|---|
| `ui.*` | 文本、按钮、列表、布局、焦点、Dialog | Public |
| `graphics.*` | 填充、线段、sprite、文本；裁剪、dirty rect；限制帧率/对象量 | Public |
| `input.*` | 按键按下/释放/长按事件 | Public |
| `timer.*` | 限制 timer 数量、最小周期、执行预算 | Public |
| `device.*` | 电量、型号、网络状态，不暴露永久设备识别信息 | Public |
| `storage.*` | 本 App 持久 KV / 文件；独立配额与原子提交 | Public |
| `cache.*` | 本 App 临时存储；系统可回收 | Public |
| `network.fetch` | HTTPS，Manifest 域名白名单；本地 HTTP 仅单服务授权 | Public + policy |
| `network.websocket` | WSS，Manifest 域名白名单；本地 WS 仅单服务授权 | Public + policy |
| `audio.play` | 系统音效/音频，音量上限 | Public / budget |
| `audio.features` | RMS / activity / pitch（按实现），用户知晓麦克风使用 | Restricted |
| `audio.record` | 原始音频独立授权，仅在前台持续有效 | Restricted |
| `channel.*`, `voice.*` | 已授权逻辑房间/PTT；不开放 Mesh 原始接口 | Restricted / later |
| `bluetooth.gatt` | 未来仅已配对/选定服务；不开放任意扫描 | Restricted / later |
| Wi-Fi admin / ESP-NOW / BLE Mesh raw / TCP UDP raw / OTA / Flash | System Only | MiniApp 禁止 |

网络访问：公网默认仅 HTTPS/WSS；局域网允许经用户确认的指定 HTTP/WS/HTTPS/WSS 服务，供 Mac 开发、树莓派、本地 API。通过系统 `selectLocalService()` 获得 opaque service handle；开发模式可临时授权固定 IP:port；生产避免单独依赖会漂移的 IP，优先可信服务身份/配对。DNS、IP、端口、每次重连、重定向均重新检查，禁止 127.0.0.0/8、链路本地、无授权私网目标、设备系统自身服务；避免 DNS rebinding。限制并发连接数（初始每前台 App 1）、响应体、消息大小、发送速率和总流量，支持 backpressure。WS binary 与 text 分离；原始音频流走 Audio Service，不宜通过脚本逐帧拷贝 16k PCM。**HTTP/WS 本身可探测服务，所以只能访问明确批准的端点，不能通过对内网任意 IP 发请求实现扫描。**

## 8. 运行时设计

### Declarative
定义 `Manifest + State + UI Tree + DataSource + Actions`，支持 `text/row/column/button/list/progress/select/dialog` 最小组件；支持 `fetch/set/navigate/conditional/timer` 有限 Action；`DataSource` 可直接请求 Open-Meteo 一类公开 HTTPS JSON 接口。数据绑定仅有限字段路径（字段名、数组下标），**不实现任意表达式 eval**。静态包完全无需开发者运营服务。UI 限制节点数、文本长度、请求体和 JSON 解析深度。增量 patch 只在需要时实现。

### Script VM
优先受限 Lua。只有系统显式注入的 `passport.*` API；不加载 `io/os/debug/package.loadlib` 等危险接口；限制堆、Lua instruction count、单个事件最大用时；异步能力采用 event/callback/future 适配，禁止在 UI 线程阻塞。`on_start/on_button/on_tick/on_audio/on_stop/on_draw` 等回调作为 SDK，而非系统把应用源代码编进固件。脚本执行或错误后可以彻底重建 VM state。

### Graphics/Game
在 LVGL 之上提供精简 Game Canvas：rect/line/text/sprite/clear/clip；尽量 dirty rect、局部 DMA 绘图，游戏初始目标 15–20 FPS；不向 Lua 暴露 LVGL 指针，不分配整屏应用帧缓冲。Game Tick 使用时间步长和队列，对 FPS 及脚本预算限流。

### Audio
BSP 音频独占归 Audio Manager；C 原生处理 RMS（16kHz 采样，约 20–40ms 帧），提供平滑音量/活动状态；游戏只收到特征值，不得默认读取原始语音。开始采集需要显著麦克风指示、系统授权、自动暂停与回收。播放和录音同时使用时，通过统一仲裁。

## 9. 存储及 Flash

必须先编译 OS 镜像，再决定分区表。初始研究方向：双 OTA app slot + NVS/otadata/PHY + App Package Cache + App Data，全部按实际偏移及 64K app 对齐、文件系统开销核算。**不要照抄 2×2.5MiB 的估算当成最终可用配置。** 若固件太大，提出 ADR：裁剪功能、改变 OTA 布局或允许 Flash 硬件升级；不得悄悄去掉安全回滚。

**Package Cache**：V1 实现完整包 LRU + Pin + 校验 + 版本切换，保存访问时间/计数时合并写入，限制 Flash 擦写频率。禁止删正在运行、更新中的唯一有效包。V2 再做资源级 CAS/去重/流式关卡，不要阻塞 MVP。

**App Data**：独立逻辑命名空间和保护策略，支持 KV/小文件、按实际占用配额、CBOR 存档与原子写、重启恢复、Schema Migration。不保证无备份情况下抵抗整片 Flash 损坏或重刷；提供导出/恢复工具。缓存格式化不能影响数据区。

**OTA**：系统 OTA 由可信组件管理，验证固件签名/哈希、开机自检、回滚；测试 OTA 时不擦 App Data。若未能实现安全 OTA，则 V1 发布必须明确禁止无人值守 OTA，不得宣称完成。

## 10. 交付的五类样例应用（必须通过实际加载器进入）

### A1 天气（Declarative / 公网 HTTPS）
- 静态 `.pap`，无需开发者后端；数据直接从 Open-Meteo 公开 API 取得（使用条款与网络限制另查）。
- LCD 显示城市、温度、天气、更新时间，三键操作刷新/切城市；状态、超时、离线反馈。
- 验证 Manifest 权限、JSON field extraction、UI Binding、HTTP/TLS、加载/Offload/重新下载；可以保存城市设置。天气数据需标注缓存新鲜度。

### A2 计数器/待办（Declarative / Local Storage）
- 上下调整、OK 确认；持久计数或待办项；断网完全可用。
- 验证 App Data 配额、重启恢复、Offload 后数据保留、彻底卸载后数据删除、断电恢复。

### A3 俄罗斯方块（Lua / Graphics / Input）
- 方块下降、移动、旋转、碰撞、消行、得分、结束/重开；按键映射考虑仅 3 键（如左右移动、OK 旋转，长按 OK 快速下降，确保可用性）。
- 游戏逻辑完全在 Lua；底层只提供通用 Canvas/Input/Timer/Storage，不得将俄罗斯方块规则硬编码为 OS 服务。
- 保存最高分及可选择的完整棋盘进度；清包、重装恢复；断网可玩。测 FPS、CPU、最大帧耗时、Lua heap。

### A4 声控飞行/跳跃（Lua / Audio Features）
- 录音权限弹窗；读取 OS 提取的 RMS/Activity 特征；按音量控制角色上升/跳跃；具备噪声校准和阈值调整；结束释放麦克风。
- 拒绝权限时提供按键替代玩法或明确提示；测试并发音频资源冲突、应用异常退出后麦克风是否关闭。
- **默认不得发送语音到网络。**

### A5 WebSocket 聊天/实时行情模拟（Script / WSS + LAN WS）
- 提供项目内独立 Python/Node 本地回显/聊天室服务器，支持本地 `ws://<LAN_IP>:port`；另用自建测试 WSS 或可配置的服务验证公网 WSS。禁止测试代码依赖不可信第三方实时开放回显网站。
- 验证连接/关闭、消息顺序、文本与二进制、断线重试、流控、授权固定局域网服务、未经授权 LAN 拒绝、会话销毁后连接数恢复。

可选 A6：远程 UI 语音助手（基于自建服务端）；可选 A7：逻辑频道 Demo（模拟 Mesh Transport，不必真无线组网）。

## 11. App Store / 开发工具

- V1 Store 可采用 HTTPS 静态索引 + CDN/开发者托管；目录应包含 app ID、可信发布者 key、版本、Manifest/PAP URL、hash、兼容性与必要权限。
- 设备可浏览、选择、首次获取、离线启动已缓存包、更新检查、Offload、Pin、删除应用及数据；操作成功/失败有明确状态。
- 开发工具 CLI：`passport pack`, `passport validate`, `passport serve`, `passport publish-index`（可按实际实现命名）；生成可重复的包和 manifest；能为开发者输出签名。
- Desktop/Browser Simulator：240×320 三键模拟，UI/事件/Lua mock 音频 API、模拟网络/存储；必须标注其与真机行为差异。测试协议和包格式与设备端使用同一套 fixtures。
- App Store 只负责发现和分发，不强制开发者拥有自建后端。允许 GitHub Pages 等静态发布。

### GitHub 协作与静态发布（历史计划，现已实施）

实施结果：用户批准 Native 优先的 `leiyin321/passportOS` 单仓库、本机签名及手动控制 Pages 发布。[分发指南](../distribution/README.zh_CN.md)与[验收报告](test-reports/public-distribution.zh_CN.md)替代下述历史计划的实施安排。

原前置条件：在 Passport OS 完成计划内的实现和真机验收门禁前，不公开源码仓库，也不启用公开发布。
开源前检查 Git 历史和发布产物，清理凭据、私有签名密钥、原始设备日志及其他私有数据。

- OS 与 MiniApp 均通过 GitHub PR 和 Actions 协作。优先拆分 OS 和商店目录仓库；如使用
  单仓库，则需实现等价的权限隔离和受保护发布区域。
- OS CI 构建和测试固件；经维护者批准的受保护任务对 OTA 清单签名，并将清单和应用镜像
  发布到 GitHub Pages 等 HTTPS 静态托管。固件签名私钥不得暴露给不受信任的 PR 工作流。
- MiniApp CI 校验源码、Manifest、权限声明、ID、版本和包格式，再生成并部署商店索引、带
  版本的 `.pap` 包和图标。索引由通过校验的贡献生成，不手工维护。
- MiniApp 所需动态后端由各应用开发者自行托管；Pages 只提供静态分发。实施前需确定社区
  发布者信任与签名、审核、兼容性、撤回/回滚及部署权限规则。

验收：公开干净检出可复现 OS 构建和宿主测试；获批准的 OS 发布可被设备 OTA 发现并验签；
MiniApp 贡献可生成通过校验的包和索引条目，并能通过静态 HTTPS 浏览、下载和安装；错误或
不受信内容会被拒绝。商店静态分发不依赖动态后端。该历史计划现已按上述指南实现，当前验收状态以报告为准。

## 12. 明确开发里程碑与 Gate

### M0 — Upstream Baseline & Resource Audit
输出：`docs/hardware-audit.md`、可重现 build、原始分区与固件大小、资源测量工具、`docs/adr/0001-runtime-choice.md` 初稿；给出最小 Launcher 可行性。Gate：官方 main 可构建，资源、引脚、当前版本有证据；无法接触真机时标记待测。

### M1 — OS Boot / Launcher / Wi-Fi / Lifecycle
输出：启动界面、三键 Launcher/设置、Wi-Fi STA 配置及自动重连、基本电量/网络状态、AppSession/ResourceScope、任务间事件队列。Gate：连续 100 次 App mock 打开退出无句柄泄漏；Wi-Fi 重连；关键信号线程安全。

### M2 — Storage / Package / Security Kernel
输出：可验证 `.pap` 包、App Registry/Downloader/Cache/Offload、独立 App Data、权限/发布者身份、版本迁移基础、原子更新。Gate：A2 应用通过 Loader 动态获取、反复启动、手工/自动 Offload、重新获取后数据存在；权限拒绝用例通过。

### M3 — Declarative UI & Network
输出：UI schema、Action/DataSource、HTTP(S)、WSS/WS、LAN 授权、网络请求资源限额、A1/A5 原型。Gate：无需开发者后端的天气应用能联网显示；本地 echo 服务需用户授权；越权地址/端口/DNS 重绑定测试通过。

### M4 — Lua VM / 2D Game
输出：VM 限额、bindings、Game Canvas、Tick/Input、A3 可玩，运行时 crash/timeout 可安全恢复。Gate：离线俄罗斯方块正常运行、退出回收、进度保留，真机简易画面帧率达到可玩标准（记录实测，不达标优化/ADR）。

### M5 — Audio / Voice-Control Game
输出：Audio Manager、音量特征订阅与权限、A4 可玩。Gate：麦克风从打开到退出完整回收；特征延迟与 CPU 可接受；拒绝权限不启动采集。

### M6 — Store / Tools / OTA / Release
输出：静态应用商店、CLI、模拟器、A1–A5 全部可在设备端安装/缓存/运行/卸载，OTA 方案与恢复说明、完整测试报告、发布固件、开发指南。Gate：系统端到端测试通过；OTA 仅在回滚实测通过后开启；给出最终资源预算和已知限制。

**每阶段执行协议：** 实现 → 单测 → 集成/模拟器 → 真机测试（有硬件时）→ 记录指标 → 更新 docs/tasks.md → Commit → 下一阶段。阻塞项必须形成可复现日志与明确替代方案，不能无限扩 scope。

## 13. 必须测试的矩阵

1. **功能**：首次/再次启动、离线启动、三键输入、屏幕 UI、天气数据、游戏、麦克风、WS、存储。
2. **生命周期**：加载失败、脚本 throw/死循环、OOM、长按退出、网络断连、应用切换、硬件 API 半途取消。
3. **存储**：缓存接近满、掉电时写包/存档、校验失败、重复包、应用升级/schema 迁移、App ID 冒名、存储配额超限。
4. **网络安全**：未授权 LAN IP:port、localhost、私网重定向、DNS rebinding、WSS 无效证书、超大 JSON/WS 帧、连接泛滥、关闭后重连竞态、无网络长等待。
5. **VM 安全**：无限循环、递归爆栈、大数组、非法绘图坐标、Lua 危险模块、滥用定时器、访问其他应用数据。
6. **资源**：free/largest heap、任务栈 high watermark、Flash 余量、LVGL 刷新时延、音频 DMA 丢帧、Wi-Fi+TLS+Lua 同时运行峰值。
7. **可恢复性**：重启后 Registry/数据一致性、OTA 成功/失败/回滚、错误固件、串口恢复与备份流程。

建议自动化用例编号：`P00–P09` 基线、`K00–K19` 核心权限生命周期、`S00–S19` 存储、`N00–N19` 网络、`D00–D09` 声明式、`G00–G19` 游戏、`A00–A09` 音频、`R00–R09` 发布恢复。每个 case 必须能产生 pass/fail 和复现实验参数。

**量化验收目标（需真机确认，不是既有能力）：**
- 应用独占前台 1 个；WS 至少 1 条连接稳定。
- 简单游戏 15–20 FPS 或提供实际性能与优化结论。
- 小程序私有 heap 默认目标 <=32KiB；如无法满足，记录数据并调整 SDK/VM，而不是隐瞒。
- 500 次启动/退出资源稳定（需定义 heap 漂移容差、日志及工具）；异常退出后音频和网络句柄全部释放。
- 包 Offload/重下载后数据校验一致；用户未要求时 App Data 不被自动删除。
- Launcher 从已缓存应用启动体验可接受，测量真实 P50/P95 启动延迟。

## 14. 固件刷写与硬件安全

- 不得未经确认在用户设备上直接执行全片擦除、eFuse 写入、Secure Boot 永久使能、Flash Encryption 永久配置或覆盖无法备份的厂商数据。
- 新分区表与 bootloader 变更前需生成备份命令、恢复步骤、当前 Flash 清单；默认以可恢复开发板或经过许可的 AI Passport 测试。
- 区分 `idf.py flash` 与 merged image 0x0 刷写；官方文档警告合并镜像可能覆盖 NVS 空隙。开发过程优先分段刷写。
- 串口日志不打印 Wi-Fi 密码、App token、API secrets、存档隐私；Crash log 脱敏。
- 所有外部组件遵守许可证与版本锁定；禁止改 `managed_components` 的生成物。

## 15. 交付清单（Codex 必须提交）

- 可构建源码仓库及固定依赖、`sdkconfig.defaults`、自定义 `partitions.csv`（实测/校验版本）。
- 可刷写的 `.bin`（bootloader/partition/app 或可靠 merged 方式）及 SHA-256、刷写恢复指南。
- App Manifest/Package 协议文档与官方 Capability API 文档。
- 开发者 CLI、Simulator、静态 App Catalog 及本地 Demo Server。
- 5 个应用的源文件和 `.pap`、测试 fixtures。
- 自动化测试脚本、真机测试记录、性能表、已知限制与安全审计清单。
- `docs/adr/` 记录重大取舍（Lua vs XS，双 OTA vs 缓存容量，LittleFS vs NVS，网络授权）。
- `docs/tasks.md` 完成状态与可复现命令；完成阶段不得只说“已实现”而无日志/测试证据。

## 16. 首轮具体执行任务（从这里开始，不要停止在规划）

```text
1. 克隆/更新 FoloToy/ai-passport，阅读 AGENTS.md 与官方硬件指南。
2. git switch -c feature/passport-os；记录 upstream commit hash 和工作区状态。
3. 使用兼容的 ESP-IDF 5.5.3 构建官方 baseline，输出 bin 大小和分区表。
4. 分析 BSP API、GPIO、任务、LVGL pool、I2S buffers，制作内存表。
5. 基于官方 BSP 做一个最简 Passport OS Launcher（非硬件 demo UI）。
6. 实现 AppSession / ResourceScope / EventBus 的首批接口及宿主单测。
7. 增加 Wi-Fi STA 连接管理与设置界面，跑 HTTPS 请求与内存测量。
8. 写 M0/M1 的 PASS/FAIL 报告，标出无法硬测的项目。
9. 继续 M2；在全部里程碑完成前不要把项目标为已交付。
```

## 17. 决策记录和验收报告格式

每个里程碑必须有：目标、实现文件、构建命令、测试命令、硬件环境、固件 hash、剩余 free heap/largest block、应用 heap、耗时指标、测试结果、已知故障、下一步。采用如下状态：`PASS (hardware) / PASS (simulator) / FAIL / BLOCKED / NOT RUN`，严禁以模拟器 PASS 替代真机 PASS。

当不可实现某功能时：提供实测证据、最小变通设计与受影响 API，说明该功能是否从 V1 降级，并继续完成其他已定义的可交付任务。

---

## 最终验收场景

用户拿到刷好 Passport OS 的 AI Passport：连接手机热点 → 在 App Store 下载天气 → 读取公共 API 显示天气 → 切换计数器并保存数据 → 运行俄罗斯方块并退出 → 系统空间不足自动释放某个应用包而保留存档 → 重新下载俄罗斯方块恢复进度 → 运行声控游戏并明确授权麦克风 → 打开 WS 聊天连接到用户明确批准的 Mac 局域网服务 → 断开 Wi-Fi，已缓存离线游戏仍可运行 → 稳定退出回 Launcher。整个过程中**没有重新刷机，没有第三方应用取得底层硬件/局域网扫描权限，且重要用户数据不会被缓存清理误删**。

完成这条路径，并提供能重复验证的源码、构建产物、测试、日志与文档，才算 Passport OS V1 成功。
