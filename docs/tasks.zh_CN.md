[English](tasks.md)

# Passport OS 执行检查点

2026-10-25 更新。分支 `feature/passport-os`，官方 BSP 基线
`006c2279852e352a8433b2c21e5e09be2ca36f77`。BSP 未修改，IDF5.5.3/LVGL9.5.0。
用户已授权开发、提交、USB 刷写、本地网络。禁止整片擦除或修改 eFuse。

| 阶段 | 已验证进度 | 剩余验收 |
|---|---|---|
| M0 | 官方基线 1,527,616 字节；C3/8MB 审计；原 Flash 完整备份 | 审计无剩余项 |
| M1 | 独立 Launcher、生命周期、Wi-Fi 重连/重启 | 主界面/三实体键用户确认 PASS |
| M2 | 签名安装、发布者注册/所有权、代码数据分离、事务恢复 | 自动 LRU/存档哈希 PASS |
| M3 | 真 HTTPS 天气、WS 文本/二进制、白名单重定向；非法端点/循环/超限拒绝 | 公网 WSS、长时断网 |
| M4 | 受限 Lua、五种应用、500 真机会话；Tetris 约 18.2Hz | 实体游玩/视觉 QA |
| M5 | 授权、真实 RMS、退出 worker 回收 | 多环境声学校准 |
| M6 | SDK/协议/模拟器及双槽 OTA 接受/拒绝/待确认中断回滚 | 签名更新器、宿主发布测试及 LAN 真机 OTA 通过；断电回滚仍待验收 |
| I0–I4 | 注册表、编译器、流式签名 PAP2、原生 LVGL 加载器集成 | 最终应用 1,767,840 字节 |
| I5 | 500 滚动、500 实际加载绘制退出；像素/alpha 哈希与宿主一致 | 最终 100 次滚动/绘制回归 PASS |
| I6 | 清包重装、暂存恢复、重启和 USB 中断 PASS | 发布包和恢复指南已生成 |

实测：grid 滚动编码缓存峰值 14,880 字节，退出后为零；500 绘制会话堆漂移 -136 字节
（容差 ±512），空闲 82,144、最低 27,624、最大连续块 36,864 字节。
原生解码临时空间 I4 300/A1 684 字节，无全图 RGBA 缓冲。
证据见 `docs/test-reports/`、忽略的 `local/*result.txt` 和 `local/hardware-*.log`。

## 小智 MiniApp 与通用 WebSocket 音频后续

| 工作项 | 已验证进度 | 剩余验收 |
|---|---|---|
| OS 通用 API | 向 Lua 提供应用级 HTTPS 自定义请求头/正文、带请求头且受白名单限制的 WSS、有界 JSON 和公开设备身份 | 真机协议/TLS 互通 |
| 通用音频 | 16 kHz 单声道、60 ms Opus 采集/播放；有界队列、授权和应用退出清理；已锁定 ESP-IDF codec 版本 | 真机音频往返、任务栈与堆回收测量 |
| 小智 MiniApp | v0.2.3 增加 OS 网络 worker 暂忙重试；全屏双语 UI 与应用自有配色 | 真机检查配置请求重试；官方服务激活及音频 |
| 构建/测试/资源 | 静态验证通过；运行时 24/24；签名 PAP 校验通过；OS 1.0.20 镜像 2,280,576 字节，OTA 分区余量 13.0%；宿主 Lua 峰值深色 64,105 / 浅色 64,177 字节 | 真机运行堆峰值及退出后回收 |

证据见 `docs/developer/xiaozhi-miniapp.zh_CN.md` 和 `docs/test-reports/xiaozhi-miniapp.zh_CN.md`（另有英文版）。本机开发版 App Store 现在仅包含小智 AI；本项未刷写设备。

## 计划：OS 稳定后采用 GitHub 协作和静态发布

**状态：尚未开始；暂不开源仓库，也不启用公开发布。**先完成 Passport OS 后续实现和真机
验收门禁；准备公开源码前，再检查仓库中的凭据、私钥、设备日志及其他私有内容。

- 通过 GitHub PR 和 Actions 协作。建议将 OS 源码/构建/发布治理与 MiniApp 商店目录分开，
  可采用独立仓库，也可采用权限隔离的发布区域。
- OS 发布由 CI 构建并验证固件；经维护者批准的发布任务对更新清单签名，并将应用镜像和
  `latest.json` 发布到 GitHub Pages 或兼容的 HTTPS 静态托管。固件签名私钥不得进入源码，
  也不得暴露给不受信任的 PR 工作流。
- MiniApp 开发者通过 PR 提交源码、Manifest 和包内容。CI 校验 Manifest、权限、包格式、
  ID 和版本，自动生成商店静态索引，并部署索引、带版本的 `.pap` 包及图标；索引不手工维护。
- MiniApp 后端由应用开发者自行托管。Pages 只分发静态商店目录和应用包；应用网络服务由
  开发者独立提供，并由应用声明和申请相应权限。
- 实施前需确定社区应用发布者信任/签名策略、目录 schema 与兼容规则、审核责任、撤回/回滚
  行为，以及受保护的部署环境。
- 验收：公开干净检出可复现 OS 构建和宿主测试；获批准的 OS 发布可被设备 OTA 发现并验签；
  MiniApp PR 可生成通过校验的应用包和索引条目；设备能通过静态 HTTPS 浏览、下载和安装；
  无效或不受信内容会被拒绝；商店分发无需动态后端。

## 安全续接

1. 阅读本文件、`AGENTS.md`、规格和开发者文档，检查 Git 和串口占用。
2. 激活 `~/esp/esp-idf-v5.5.3/export.sh`。运行 `tools/passport/test.sh` 和 `tools/validate.sh`。
3. 仅在构建成功后，将已验证应用刷入活动 OTA0 `0x20000`。
   `/dev/cu.usbmodem101` 必须单进程独占。禁止再次刷初始化数据镜像。
4. 执行 `tests/passport/hardware_cache.py`、`hardware_faults.py` 和资源回归。
5. 完成双语报告/发布包、归档验证；停止自己启动的本地服务。

分区：OTA0 `0x20000/0x280000`，OTA1 `0x2a0000/0x280000`，SPIFFS 包
`0x520000/0x260000`，appdata NVS `0x780000/0x80000`。当前设备已初始化。
原备份 `local/original-flash.bin`、校验和及分块必须保留且不公开。
Wi-Fi 和私钥只保存在忽略的 `local/`，禁止复制进发布包。
本会话开发服务监听 8040/8041/8042，交付时停止。用户已确认主界面和实体按键正常；应用视觉和浏览器视觉 QA 待测，
不得宣称全部 V1 验收完成。

最终交付：`build/release/`，已验证归档位于 `build/firmware/55fd19bd2c3da64fa84a622a50688f43a986b3e0b493b88f8bebc97a196c8f73/`。最终应用 SHA256 `86fb164ec07e8e83f8c18ebd7b06ec538f735677272ff8123bf07456329fb67e`。最终 100 绘制循环 PASS，堆漂移 -112 字节；自动 LRU/异常隔离/Store 安装通过。开发服务已停止。参见 `developer/firmware-release.zh_CN.md`。

## 当前启动器修订

用户确认旧版文字启动器可用后要求图文主界面。现使用四张原生图标/文字卡片，参见
`developer/launcher-ui.zh_CN.md`。完整构建/宿主 Gate PASS；已刷应用 1,768,784 字节
（增加 944），SHA256 `1fa05a79d35dd30097d049b9fcc51d2cad845ada17a7e27664dff9f03a21754b`。
当前归档：`build/firmware/b20e17cdf2794c0192d91586361c409dfdd111496b2e63d78593ca2bf5dfe9fc/`。
此处替代上文旧发布路径/哈希；之前压力/OTA 数据保留为对应构建的证据，不表示在仅改 UI 的版本上重跑。

## App Store 服务恢复

用户反馈商店不可访问后，确认 8040 没有监听进程。现已重启 8040 目录服务和 8041 WS echo，
确认签名目录有七个应用；重新授权本次启动使用 `192.168.1.10:8040/8041`，真机成功读取并显示目录，
安装 `org.passport.appstore-grid`。最终联网验收又读取目录并安装了 Tetris；设备现已返回主界面。用户浏览商店期间需保持两个服务进程运行。

## D0–D6 显示布局、卡片列表和手机 Wi-Fi 配网（2026-10-25）

- 已实现标准/全屏视口管理、绘图裁切宿主、Manifest 校验、Lua `passport.display.get_viewport()`、声明式绑定及 UP/DOWN/OK 导航。Apps 和 App Store 列表采用 2×2 图标/标题卡片；缩短底栏提示以适配 224px 区域。
- `./tools/validate.sh` 通过（ESP-IDF 5.5.3）；Wi-Fi 表单解析器宿主用例及已有 C/Python 套件通过。
- Passport 真机矩阵：Weather/Chat 为 240×272 标准模式，Tetris/VoiceBird 为 240×320 全屏。VoiceBird 麦克风采集启动。Tetris/counter 交替绘制并退出 200 次，52.58 秒通过；堆从 75,476 变为 75,344 字节；每次退出后图像句柄为 0、模式回标准。
- 新增 Settings → 手机配网：启动临时 WPA2 AP 和本次随机密钥；本地页面 `http://192.168.4.1` 使用有界表单解码、保存 NVS，获取 IP 后关闭 AP。真机验证 Settings 页面和 AP 启动；解析器宿主用例通过。尚未验证手机关联及 HTTP 表单提交。
- 最近验证应用镜像 1,798,288 字节、合并镜像 1,929,360 字节，归档 `build/firmware/25188f317d2fd1827dc62363d466f3bec2d715731709d4b03bd405268236a54a/`。仅刷写 `0x20000` 应用分区，以保留 NVS 和应用数据。
- 最终保留数据刷写前的完整镜像刷写清除了 NVS；用户随后用主机 `passport wifi` 恢复网络。最终真机检查确认实时天气、WS 文本回显、七项 App Store 目录载入，以及商店下载/安装/启动 Tetris。8040/8041 本地服务仍运行。
- 重复 VoiceBird 会话测试发现编解码器休眠后未唤醒；`ps_audio_start` 现会调用 `bsp_audio_wake`。最终固件在上次会话已休眠后再次采集 209 帧，持续 4.22 秒。
- Tetris 棋盘绘制已改为扫描连续占用行，保持 Lua 回调预算。最终商店包哈希 `4bec4c5c717cce2417c434f34aceb91a2de1c1c443261362289a904e78e75bef`。
- 详细指标及限制见中英文显示测试报告。新卡片/Settings 人眼视觉检查、实体长按 OK 时长仍待验证。

## 系统日期与时间（2026-10-25）

- Wi-Fi 获取 IP 后已有 SNTP 客户端会自动向公开服务器 `pool.ntp.org` 同步；此次补充系统默认时区 `Asia/Shanghai`（UTC+08:00）、顶部 `HH:MM` 时钟和 Settings → Date & Time 详情页。
- 首次同步前状态栏显示 `--:--`，详情显示等待 Wi-Fi/同步状态；同一次开机内 Wi-Fi 断开时系统时钟继续走时，断电重启后需重新联网同步。该系统时间服务可作为后续闹钟的基础；重启后应用需等时间有效再安排闹钟。
- `./tools/validate.sh --static` 与 `./tools/validate.sh` PASS；ESP-IDF 5.5.3，应用镜像 1,827,520 字节（OTA 应用区 2,621,440 字节，剩余 30%），合并镜像 1,958,592 字节。归档 `build/firmware/80ba98ee0212a8cab76a74f2a2dd658e5af6bd46e5ec03e4911d3413fa981604/`，SHA256 `80ba98ee0212a8cab76a74f2a2dd658e5af6bd46e5ec03e4911d3413fa981604`，ELF SHA256 `b3ab2954895b179004d4912a36ada27d45b876576be0b1171cdef00532da1223`。时间显示仅增加一个 LVGL 状态栏标签，无新增任务或网络缓冲区；未单独测量堆差值。未刷机，真机 NTP 同步/时区/状态栏显示待验收。

## 双槽签名系统更新器（2026-10-25）

## 系统闹钟（2026-10-25）

## 设置一级菜单滚动及“闹”字覆盖（2026-10-25）

## 闹钟设置提示优化及启动器清理（2026-10-25）

- 闹钟详情显示当前编辑项，并在小时、分钟或启用状态上明确标记选中项。闹钟关闭时进入详情会优先选中“启用”，此时按上下键即可开关；已开启时从小时开始编辑。中英文界面均说明短按 OK 切换字段和停止操作 1.5 秒后自动保存。
- 移除用户可见的“图标库”主页卡片、设置行及图标浏览页。系统内部 System Icon Registry 仍供系统界面和 MiniApp 使用。
- `PASSPORT_OS_VERSION=1.0.16 ./tools/validate.sh` 完整验证通过：仓库检查、全部宿主测试、ESP-IDF 5.5.3 构建、固件归档和合并镜像校验。应用镜像 2,067,840 / 2,621,440 字节（剩余 553,600 字节，21.12%）；合并镜像 2,198,912 字节。未连接硬件，因此没有设备 RAM 或实体交互数据。
- 签名版本 1.0.16 / build 2026102519 已由本机更新服务 `http://192.168.1.10:8765/latest.json` 提供。HTTP 清单和镜像获取 PASS；下载镜像 2,067,840 字节，SHA-256 与 `82bcd67fbeee4858dd69f92d31afed6870d7cca8478704a0bb2c764b0ea81fd6` 一致。
- 闹钟交互和启动器文档已同步更新中英文。硬件可通过 Settings → System Update 更新；本轮未使用 USB 刷写。

- Settings 一级菜单改为八行视口，UP/DOWN 浏览所有项目并以右侧滚动条提示位置，不再挤满单屏。纯视口定位逻辑加入宿主测试。
- 修复“闹”缺字：思源黑体子集从 132 扩展到 133 个码点（U+95F9）；滚动提示中的 U+6EDA“滚”在生成字体中，U+52A8“动”由 LVGL CJK 回退字体覆盖，字体测试检查两条覆盖路径。
- `PASSPORT_OS_VERSION=1.0.15 ./tools/validate.sh` 完整门禁 PASS。应用 2,067,776 字节，剩余 553,664 字节（21.12%）；合并镜像 2,198,848 字节。字体 Flash 16,932 字节，生成 C 源 113,325 字节；未增加页面级字体堆分配。
- 签名发布 1.0.15 / build 2026102518 已发布到本机更新服务；HTTP 下载镜像和签名/哈希校验 PASS。按用户要求未连接设备，真机滚动与中文显示仍待确认。报告：`docs/test-reports/settings-scroll.md`。

- Settings → Alarm 新增系统每日闹钟；支持调节小时/分钟、启停，使用系统语言显示。设置保存至 `osconfig`，停止修改 1.5 秒后写入，默认关闭、07:00。
- 复用已同步的上海本地系统时间；时钟无效时不触发，恢复有效后重新调度；每个本地日历日最多触发一次。触发时退出前台 MiniApp、点亮屏幕并通过 BSP 音频 codec/扬声器播放重复提示音，任意按键停止并返回主页。未开放 MiniApp 闹钟 API。
- 增加可宿主测试的纯时间判断/字段调整模块及测试。`PASSPORT_OS_VERSION=1.0.14 ./tools/validate.sh` 完整门禁通过；应用镜像 2,067,328 / 2,621,440 字节（剩余 554,112 字节，约 21.1%），合并刷机镜像 2,198,400 字节。构建包 `build/firmware/a8af1b4675f2264cc21c1824ce04595746c514b0f026e8230de6d1d359699a6c/`，合并镜像 SHA256 `a8af1b4675f2264cc21c1824ce04595746c514b0f026e8230de6d1d359699a6c`，应用镜像 SHA256 `515dccadc899adcd9f09952ad131f5b1fce013bd958347c71dc28bad01db93f0`。
- 已签名并发布到运行中的 Mac 更新服务 `http://192.168.1.10:8765/latest.json`，版本 `1.0.14` / build `2026102517`；清单验签通过，HTTP 获取镜像大小 2,067,328 字节且 SHA256 与归档一致。未连接/刷写设备；真机设置交互、定时触发、声音、内存峰值及停止后 codec 回收仍待设备安装后验收。当前设计不保证深睡眠唤醒。
- 中英规范见 `docs/developer/system-alarm.md`；构建/发布/资源和未验收项见 `docs/test-reports/system-alarm.md` 及配对中文报告。

- Settings → About 已增加固件语义版本显示；版本 1.0.7 / build 2026102509 已签名并发布到本机更新服务。用户确认系统 OTA 流程和 About 固件版本号均已在真机验证通过。构建、宿主测试、SDK updater 测试、清单及镜像验签通过。应用镜像 1,840,384 / 2,621,440 字节（剩余 781,056 字节，29.79%），合并镜像 1,971,456 字节；无新增持久 RAM。

- Settings 新增 System Update，分步确认检查/下载/安装并显示字节进度。清单使用内置 RSA-2048 系统密钥验签；镜像精确长度流式下载到非活动 OTA 槽，安装前验证 SHA-256 与 ESP 镜像头。开机后通过现有 OS/存储自检才确认候选槽；ESP-IDF 回滚支持保持开启。更新器不写应用包和 App Data 分区。
- 增加 USB developer CLI 配置/清除可信 HTTPS 清单地址，以及 `passport firmware-release` SDK 工具签名发布 `latest.json` 和仅应用镜像。首次安装仍需 USB 刷固件；系统更新用于之后升级。
- `./tools/validate.sh --static` 和完整隔离 `./tools/validate.sh` 均 PASS（ESP-IDF 5.5.3）。更新工具宿主测试 PASS（3 个用例：签名发布/篡改拒绝、重复字段、文件名/大小边界）；SDK `firmware-release` CLI 也已用归档应用镜像完成签名及清单/镜像往返验签。
- 已验证归档：`build/firmware/14a8e835a508436325c77238e26052fae9181e163bd7d73bbbad1d75bf2ecfd2/`。应用镜像 1,838,960 / 2,621,440 字节（余 782,480 字节，29.85%）；首次刷机合并镜像 1,970,032 字节。应用 SHA-256 `90d9ae429c47f8b17e82ba81d342299daf6a912e1142c194ccbef1f86c4834ac`；合并镜像及归档 SHA-256 `14a8e835a508436325c77238e26052fae9181e163bd7d73bbbad1d75bf2ecfd2`；ELF SHA-256 `e2ecf0de9eb6c855d239e5b354239715c98c92fdfe9ba88cb5e99ffd669e8069`。
- macOS 本机发布服务器和 Cloudflare Quick Tunnel 已运行，远程 HTTPS 清单及 1,838,960 字节镜像均返回成功；签名版本为 `1.0.0` / build `2026102502`。设备 `/dev/cu.usbmodem101` 已在 `0x20000` 写入已验证仅应用镜像（哈希校验通过；NVS/package/App Data 区未写入），HTTPS 更新源已配置成功。实体按键触发的 Settings OTA 检查/下载/安装及启动后数据/回滚验收进行中。RAM 峰值尚未测量；网络流块为 512 字节，另复用现有 TLS/网络栈。
- 最终直连 LAN 验收（2026-10-25）：发布 `1.0.5` / build `2026102507` 已签名并由本机 `http://192.168.1.10:8765/latest.json` 提供。设备检查清单、完整下载 1,840,224 字节、安装到 OTA1 并重启；串口记录 `OTA self-test valid rc=0`，用户确认成功。服务器对清单和镜像均记录 HTTP 200。此前 Quick Tunnel 在 223,312 字节处触发 mbedTLS 读取错误 `-0x7100`；切换直连 LAN 后完成。引导恢复时只写了 `0x20000` 应用区；OTA 只写非活动 OTA 槽，不触碰 packages/appdata。启动 Wi-Fi 指标：空闲 86,728 字节、最低 86,672、最大连续块 77,824；未捕获 OTA 期间 RAM 峰值。断电回滚注入和应用私有数据逐项比对仍未验证。本机签名更新服务继续运行。私有 LAN HTTP 仅用于开发验收，签名、精确大小、哈希及 ESP 镜像检查仍强制执行。

## MiniApp 开发标准与工具审查（2026-10-25）

- 新增源码/打包态 Manifest、声明式 UI/1、商店目录 v1 机器可读 schema，以及配套开发规范和 CLI 命令参考。Manifest 校验现覆盖实际源码清单和编译后 PAP2 资源字段。
- `passport pack` / `passport validate` 会预检声明式 UI 节点/操作、解析与资源限额、资源引用、Manifest 权限及 HTTPS 数据源白名单。已从 SDK 与设备运行时移除原本不执行页面导航的空操作 `navigate`。
- `passport publish-index` 支持多个发布者公钥，逐包验签，拒绝重复 ID 并遵守设备当前目录上限（8 项）。商店分页、`passport validate` 中 Lua 执行/静态检查、真机商店验收和社区密钥治理仍待完成。报告：`docs/test-reports/miniapp-sdk-audit.zh_CN.md`。
- `passport serve` 默认仅绑定 localhost；开放局域网需显式指定。MiniApp 宿主测试现纳入 `./tools/validate.sh --static`。
- 静态门禁 PASS，含 12 项包测试、5 项声明式测试和完整 `tools/passport/test.sh`。固件构建/分区校验 PASS（ESP-IDF 5.5.3）；应用镜像 2,067,776 / 2,621,440 字节（剩余 553,664 字节，21.12%）；合并镜像 2,198,848 字节。未连接设备，也未测量真机 RAM。

### 系统更新发布

已将验证通过的小智版本发布为 `1.0.17` / build `2026102520`，写入正在运行的 Mac 更新服务目录（`http://192.168.1.10:8765/latest.json`）。应用镜像 2,278,640 字节；SHA-256 `799c6daacfa47a5197d4ed9422052b76783fff2e06294a1076af186d8bbffe8e`。实际服务端清单和完整镜像均成功获取，签名和哈希验证通过。本次未触发设备安装。详见 `docs/developer/system-updates.zh_CN.md`。

### 本机 MiniApp 商店目录发布

查明 App Store 为空的原因：8040 端口服务正常，但 `dist/index.json` 的 `apps` 数组为空。现在仅将小智 AI v0.2.0 加入目录，保留用户此前删除旧应用的决定；PAP 签名验证通过，使用 `passport publish-index` 重新生成目录，并从实际局域网服务获取目录和应用包验证成功。设备端刷新/安装待验证。

### 小智安装失败诊断

App Store 已正确提供签名包，但设备在 Manifest 限额阶段拒绝（`PAP rejected: limits`）。设备启动 ELF `1478e2d69...` 与 OS 1.0.16 归档镜像匹配（`4d56f53e7d7defb77e48802e8b86190b3c8f8dff48548b0cc78437de42352bb7`）；小智需要 64 KiB Lua 堆，已发布的 1.0.17/build `2026102520` 支持该上限。下一步：通过 Settings → System Update 安装 1.0.17，About 确认版本后重试小智。本次未刷写固件或擦除数据。

### OTA 1.0.17 启动回滚与修复

用户通过 Settings 安装 1.0.17。设备进入 `ota_1` 后，`passport_http` worker 触发栈保护异常，ESP-IDF 自动回滚到 1.0.16。匹配 ELF 回溯确认 worker 栈上的 9,308 字节 `job` 描述符超过 8,192 字节任务栈（采集 SP 低于栈底 2,980 字节）。1.0.18 改为堆分配单份 job、队列传指针，并新增回归测试。`PASSPORT_OS_VERSION=1.0.18 ./tools/validate.sh` 完整门禁通过。应用镜像 2,278,416 字节，OTA 槽余 343,024 字节；合并镜像 SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`。签名 `1.0.18` / build `2026102521` 已发布到本机更新服务，在线清单及镜像大小/哈希已验证。用户已安装 1.0.18；串口确认 `ota_1` 启动、存储自检通过、Wi-Fi 联网且无新 panic。启动时最低空闲堆 85,264 字节，网络任务栈高水位 3,884 字节。用户确认 About 显示 1.0.18。详见 `docs/test-reports/ota-1.0.17-rollback.zh_CN.md`。

### 应用库卡片栈异常与修复

1.0.18 上进入“应用”卡片会导致 main 任务栈保护异常。匹配 ELF 显示格式化输出期间 SP 低于栈底 132 字节。卡片绘制为取得应用版本号同步加载并验签 PAP 包，导致包解析嵌套在较大的 `draw()` 栈帧下。1.0.19 改为列表刷新时预读版本，绘制时只读缓存；新增源码契约回归测试。`PASSPORT_OS_VERSION=1.0.19 ./tools/validate.sh` 完整门禁通过。应用镜像 2,278,416 字节，OTA 槽余 343,024 字节；合并镜像 SHA-256 `c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`。签名 `1.0.19` / build `2026102522` 已发布到本机更新服务，清单/镜像已验证。OTA 安装后的真机复测待完成；详见 `docs/test-reports/app-library-card-crash.zh_CN.md`。

小智 0.2.1 修复用户反馈的启动界面问题：Manifest 现请求全屏，语音/对话/连接设置页使用完整 240×320 视口、深色背景和更明亮的高对比卡片。静态宿主验证通过（运行时 20/20）；本机 App Store 的 `dist/` 目录和包已重新生成并验签。真机视觉验收待完成，详见 `docs/test-reports/xiaozhi-miniapp.zh_CN.md`。

OS 1.0.20 新增持久化深色/浅色外观和只读 `passport.system.display_mode()`。应用自行决定是否跟随系统，并可通过 `passport.text(value, rgb24)` 设置文字颜色。小智 0.2.2 根据只读系统模式绘制自己的高对比调色板。静态验证通过，运行时 24/24；签名系统固件和应用包已发布到本机更新与应用商店服务。小智 0.2.3 网络 worker 忙碌重试已通过模拟器验证并发布到本机 App Store，待真机验证；用户已确认系统外观切换通过。详见 `docs/test-reports/xiaozhi-miniapp.zh_CN.md`。

## 系统审查与优化 1.0.23

架构/源码审查及已实施的内存、HTTP 分帧、音频、JSON、闹钟、SDK、原子发布优化见[审查报告](development/passport-os-review.zh_CN.md)。ESP-IDF 和宿主完整门禁通过，OTA 1.0.23 build 2026102604 已发布，在线签名/大小/哈希验证通过。设备最近一次观测仍为 1.0.21，需要从设置安装新版；升级后的资源/语音/网络验收待完成。

- 2026-10-26：通过 USB 复现小智 TLS RSA/MPI 分配失败；发布 OS 1.0.24、小智 0.2.4，加入动态 TLS 缓冲、连接前 Lua 回收、有界重试及激活提示。构建和主机测试通过，等待 OTA 后设备验证。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。

- 2026-10-26：TLS 验证成功后，将 WSS HTTP 502 定位为 Host 错写 IP。OS 1.0.26 保留域名请求头及已验证 IP 连接；构建、主机测试、在线包校验通过。设备 hello/音频等待 OTA 验证。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。

- 2026-10-26：修复小智把音频启动失败误报为用户拒绝授权。流启动异常保留授权并通知 `on_audio_error(1)`。已发布 OS 1.0.28、小智 0.2.7；设备音频验收等待 OTA 后进行。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。
- 2026-10-26：修复 WSS 堆碎片下小智音频任务无法创建的问题：先分配 9 KiB 任务栈，再分配流队列；增加阶段/堆状态日志及各分配阶段回滚测试。OS 1.0.29/build 2026102610 已发布到 Mac OTA 服务。完整构建和宿主测试通过；真机按住 OK 通话、编解码栈高水位、RAM 峰值和退出资源回收仍待 OTA 后验收。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。
- 2026-10-26：设备日志确认 OS 1.0.29 在 WSS 后无法创建 9,216 字节音频任务栈（最大块 7,680 B）；小智还会在 OK 保持逻辑按住时每个 tick 重试。OS 1.0.30 在应用启动网络前预留不采集音频的工作线程，并在多次按住通话间复用；小智 0.2.8 在出错后清除按住状态。完整门禁通过；OS 1.0.30/build 2026102611 和小智 0.2.8 已发布并核验。语音、内存峰值/栈高水位及退出回收需更新后复测。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。
- 2026-10-26：OS 1.0.31 保留预留音频流任务，并兼容声明流能力的应用使用普通麦克风采集。完整门禁和 33 项运行时测试通过；build 2026102612 已在 OTA 服务取代 1.0.30。App Store 小智 0.2.8 仍为最新版。真机语音及资源验收待安装后继续。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。
- 2026-10-26：复现 1.0.31 WSS 证书校验失败（`0x4290`，之后 TLS `-0x3000`），原因为预留音频栈占用普通堆。OS 1.0.32 将 9 KiB 工作线程栈放入 ESP32-C3 Retention RAM，并使用 capability 感知的任务删除 API。完整门禁及 33 项运行时测试通过；签名 build 2026102613 已上线。真机 WSS/PTT 和堆回收仍待 OTA。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)。
- 2026-10-26：用户在 1.0.32 上再次复现 WSS 故障。Retention 分配仍与 TLS capability 分配共享内部 RAM。OS 1.0.33 延迟到实际开始采集时再分配工作线程栈；应用启动和授权不再预留。堆指标已区分 Internal/Retention 空闲量及最大连续块。完整主机测试和固件构建/布局验证通过。见[报告](test-reports/xiaozhi-miniapp.zh_CN.md)；真机 OTA 验收待完成。
