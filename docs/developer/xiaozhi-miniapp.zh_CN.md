[English](xiaozhi-miniapp.md)

# 小智 AI MiniApp

## 已实现功能

`examples/xiaozhi/` 已实现双语小智 WebSocket MiniApp。应用读取只读公开设备身份，向兼容小智的 OTA/配置端点提交设备及应用信息，读取激活说明和 WebSocket 配置，并将应用自己的服务 URL/Token 保存在应用私有数据中。响应里的固件更新字段会被忽略。配置可用后，应用使用自身提供的 Authorization、Protocol-Version、Device-Id、Client-Id 请求头建立 WSS，发送协议 v1 `hello` 并检查服务端返回的传输及音频格式。

按住 OK 会发送 `listen/start`，请求 OS 的通用 16 kHz 单声道 Opus 音频流，并将每帧 Opus v1 音频作为 WebSocket 二进制消息发送。松开 OK 会发送 `listen/stop` 并请求停止采集。收到 `stt`、`llm`、`tts`、`alert`、`goodbye` JSON 消息后，应用更新语音及对话页面；收到的 Opus 二进制帧会排入 OS 通用解码/播放队列。应用忽略不支持的 `mcp`、旧版 `iot` 及 `system` 命令。双击 OK 由 OS 拦截并返回主页。

0.2.1 引入 Display API 的 240×320 全屏视口。0.2.2 读取 `passport.system.display_mode()` 并自行选择配色：浅色模式使用浅色面板和深色文字，深色模式使用深色面板和浅色文字。应用也通过 `passport.text(value, rgb24)` 指定文字颜色；应用负责自身颜色，可独立于系统主题定制。0.2.3 在 OS 网络 worker 暂时拒绝请求或返回可重试的 HTTP 错误时，会自动重试服务配置。请求提交被拒绝不再误报为 Wi-Fi 设置错误，应用会显示“等待网络…”并自动重试。Manifest 声明 `display.mode: fullscreen`，应用运行时由 OS 隐藏状态栏和导航栏。

小智专属配置字段、认证语义、协议消息和对话展示都保留在 Lua 应用中。OS 仅提供与服务商无关的 HTTPS/WSS、JSON、公开身份和 Opus API。应用无法访问 PCM、Socket 句柄、硬件密钥或编解码器句柄。

## 配置和兼容性

应用当前使用 `https://api.tenclass.net/xiaozhi/ota/`，并读取包含字符串 `url`、`token` 字段的 `websocket` 对象。要求 `wss://` URL 的主机与 Manifest 声明的精确域名一致。当前 Manifest 仅允许 `api.tenclass.net`；如果服务返回其他主机，必须先将该精确主机加入 Manifest 才能连接。应用会显示激活码/说明，供用户在账号中激活设备；不会执行固件 URL 或 OS 更新字段。

OS 通用音频 API 当前支持 16 kHz 单声道、60 ms 的 Opus v1 帧。应用通告该音频参数，并拒绝不兼容的服务端 hello。本应用声明 64 KiB Lua 堆，需要 Passport OS 1.0.17 或更新版本。当前不支持 MCP、AEC、MQTT、UDP、唤醒词及后台自动聆听。麦克风上传仅在用户按住 OK 时发生，松开或退出应用后不会继续录音。

## 验证和剩余真机工作

OS 网络层保留 TLS 校验、Manifest 精确域名检查、有界请求/响应/消息、保留请求头拒绝、WSS 重连策略、前台麦克风授权、有界音频队列和应用退出清理。音频工作任务会记录编解码前后堆、编解码器关闭后的空闲堆、最大连续空闲块和任务栈余量。固件和宿主资源测量见[小智测试报告](../test-reports/xiaozhi-miniapp.zh_CN.md)；真机堆和栈数据仍需设备验收。

MiniApp 已签名并通过 SDK 校验，静态/宿主测试及隔离 ESP32-C3 固件构建均通过。现在它是本机 Mac 开发版 App Store 目录中的唯一应用，目录地址为 `http://192.168.1.10:8040/index.json`；已从运行中的服务获取目录和包并验证通过。公开目录分发前仍需设备验收：官方服务配置/激活、真机中英显示、实际语音双向链路、运行时 RAM 峰值和退出后的堆回收。

## 协议参考

- [小智官方平台](https://xiaozhi.me/)
- [小智 WebSocket 协议](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)
- [FoloToy Passport 小智参考实现](https://github.com/FoloToy/folo-ai-passport-xiaozhi)
- [FoloToy OTA/配置参考代码](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/main/main/ota.cc)
- [Passport Capability API](capability-api.zh_CN.md)

## 0.2.4 的连接恢复

使用 OS 1.0.24 或更新版本，以启用动态 TLS 缓冲及网络提交前的 Lua 回收。通用 HTTPS 失败不代表 Wi-Fi 断开。应用最多自动尝试三次配置请求，保留失败状态，在连接页按 OK 可重新开始。收到激活码后自动打开连接页；在官方账号完成激活后按 OK 刷新配置。允许空 token，此时不发送 Authorization。小智服务返回的固件信息不会更新 Passport OS。实测和发布信息见[测试报告](../test-reports/xiaozhi-miniapp.zh_CN.md)。

## 0.2.7 音频启动恢复

麦克风授权与 Opus/I2S 音频线程启动相互独立。后台线程启动失败时，OS 清除当前采集状态并发送通用回调 `on_audio_error(1)`，保留授权决定。小智对同步和异步启动失败均提示音频暂不可用，可松开按键后重试。只有本地采集启动成功后才向服务器发送 `listen/start`；“权限被拒绝”仅用于用户明确拒绝授权。

## OS 1.0.29 音频任务分配顺序

升级 1.0.28/小智 0.2.7 后，授权状态已保留，但音频仍无法启动。服务先分配两条流队列，再分配 10 KiB 任务栈。WSS 连接后观测到队列分配前最大连续空闲块约 9.7 KiB，队列分配后约 7.7 KiB，因此无法分配连续任务栈。现在先以 9 KiB 栈创建音频任务，让它等待启动信号；随后分配队列并唤醒任务。启动日志会指出信号量、任务栈或队列分配阶段，并记录空闲堆和最大连续块。宿主生命周期测试逐项注入分配失败并检查资源清理。

OS 1.0.29 / build 2026102610 已发布到 Mac 系统更新服务。固件构建和宿主测试通过。9 KiB 栈相对已测得的 4,752 字节采集函数栈帧仍留有余量；完整编解码调用链的栈高水位和真机按住 OK 通话需在 OTA 后继续测量。

## OS 1.0.30 / 小智 0.2.8：网络前预留资源，失败后停止按住重试

用户安装 1.0.29 后的串口日志确认每次都在 `stage=task-stack bytes=9216` 失败：空闲堆
22.4 KiB，但最大连续块只有 7,680 字节。仅调整队列顺序无法在 TLS/WSS 碎片化后得到该
任务栈。OS 现在打开声明 `audio.stream` 的应用时，在其网络回调启动前创建等待态音频任务。
该任务等待启动信号；在应用获得权限并启动流之前，不初始化或读取麦克风。等待态任务会
在多次按住通话之间复用，并在应用退出或权限拒绝时回收。这样保留了此前栈溢出调查证明
需要的 9 KiB 任务栈。

小智 0.2.8 在同步或异步音频失败后清除“按住说话”状态。此前 OK 仍被逻辑视为按住，`on_tick`
会重复尝试启动并覆盖错误提示。现在必须先松开 OK 才可重试。宿主测试确认两类失败后后续
tick 都不会重复调用流启动。

OS 1.0.31 已在更新服务取代 1.0.30，保留网络前预留任务，并兼容同时声明流权限的应用使用
普通麦克风采集。小智 0.2.8 已发布到本机 App Store。

随后从 1.0.31 设备日志确认，WSS TLS 证书签名验证报 mbedTLS `0x4290`，接着握手返回
`-0x3000`。预留任务栈和 RSA 验签竞争普通堆。OS 1.0.32 通过 FreeRTOS 带 capability
任务 API，将同样的 9 KiB 栈放入 ESP32-C3 Retention RAM；任务控制块仍位于内部 RAM，从而
保留普通堆的大块连续内存供 TLS 使用。该版本已发布，OTA 后仍需重测 WSS 和按住通话。

用户在 1.0.32 上再次复现 WSS 错误。Retention 栈仍会减少要求 `MALLOC_CAP_INTERNAL` 的
分配可用 RAM；Retention 与普通 DRAM 对这类请求并不是独立内存池。OS 1.0.33 因此延迟到
TLS/hello 后、MiniApp 实际调用 `audio.stream_start()` 时再创建音频 worker；麦克风授权不再
分配任务栈。堆指标现在区分 Internal 与 Retention RAM 的空闲总量及最大连续块。设备 TLS、
音频、峰值内存和资源回收仍待安装 OTA 后验收。
