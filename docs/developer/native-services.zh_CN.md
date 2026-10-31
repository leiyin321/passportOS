[English](native-services.md)

# Native ABI 2 服务

需要 Passport OS 1.0.35 及以上，并使用可信 Native Manifest 的 `abi:2`。兼容 ABI 1 和 Lua。头文件为 [`passport_native_v2.h`](../../sdk/native/include/passport_native_v2.h)，机器可读规范为 [`abi-v2.json`](../../sdk/native/abi-v2.json)。沿用[原生开发](native-apps.zh_CN.md)中的 SDK 编译、打包、验证、安装及目录生成工具。

入口仍接收 ABI 1 前缀指针；先检查 `host->abi==2` 及 `host->size>=sizeof(passport_native_host_v2)` 再转换。返回 `app.base.abi=2`、`app.base.size=sizeof(passport_native_app_v2)`、同步 `event`/`stop` 及可选 `data(kind,status,bytes,length)` 回调。ILP32 宿主／应用表分别为 80／20 字节。OS 检查回调地址，不导出 LVGL、驱动、堆或 RTOS。

| 方法 | 约定 |
|---|---|
| `network_ready()` | 读取 OS 实际 Wi-Fi 状态，需要 fetch 或 websocket 权限 |
| `http_request(method,url,headers,body,length)` | 异步 HTTPS GET/POST，正文至多 2048 字节，头至多 1024 字节／8 项；需要 `network.fetch` |
| `ws_open(url,headers)` | 异步 WSS 握手，至多 4 项受限头；需要 `network.websocket` |
| `ws_send(data,length,binary)` | 拷贝／排队至多 2048 字节，需要相同 websocket 权限 |
| `ws_connected()` / `ws_close()` | 查询／关闭会话所属连接 |
| `json_get(json,length,path,out,capacity)` | 点分对象路径，拷贝解码后的字符串、数字或布尔值；非负返回值为不含 NUL 的长度，-1 无效或缺失，-2 容量不足，不部分输出。JSON 至多 16 KiB、路径 127 字节、输出缓冲区 4096 字节 |
| `identity(device,device_cap,client,client_cap)` | 需要 `device.identity`，设备缓冲区至少 18、应用稳定客户端 ID 至少 37 字节；与 Lua 使用同一身份方案 |
| `storage_get(key,out,capacity)` / `storage_set(key,value)` | 沿用应用私有存储和 Manifest 配额，需要 `storage.private`，每次回调至多写入 4 次 |
| `audio_mode(1/0/-1)` | 申请录音、仅播放、关闭会话；需要 `audio.stream` 及 OS 麦克风授权 |
| `audio_play_opus(data,length)` | 将至多 1024 字节包拷贝到有界播放队列，需要相同权限和授权 |

除 `json_get` 使用上述返回值、关闭方法无返回值外，服务操作接受返回 1，拒绝返回 0。音频请求修改托管视图状态，OS 前台循环异步启动工作任务；模式 0 不开启麦克风采集，模式 -1 结束托管音频会话，由 OS 循环回收任务。应用必须及时返回。私有存储沿用现有同步、有界存储实现。

HTTP 正文、头及 WS 消息在调用返回前拷贝到 OS 任务／队列。DNS、地址、端口、域名、TLS 证书、白名单内重定向及消息配额沿用已有网络服务。这些 Native 宿主方法不包含小智地址、认证头格式、凭据或业务协议。

异步回调在应用前台上下文派发：

- 事件 7：麦克风授权，1 同意／0 拒绝；OS 先设置授权再通知应用，拒绝授权与任务启动失败分别处理。
- 事件 8：音频启动错误，值沿用 Runtime 音频错误码。
- 数据类型 1：HTTP 完成，包含 WSS 升级成功的 101、失败的负状态；旧会话 generation 的 HTTP 结果被拒绝。
- 数据类型 2／3：WebSocket 文本／二进制，status 为 opcode。
- 数据类型 4：采集的单声道 16 kHz Opus，通常每包 60 ms；停止采集后不再交付。

数据指针仅在本次回调内有效，保留字符串必须拷贝，禁止保存帧指针或把 Native 回调交给工作任务。stop 在代码仍加载时执行；随后托管会话取消任务、清空消息、回收音频和网络，再释放包。ELF 卸载后不再调用应用回调。

可执行池仍为 16 KiB，文件上限仍为 32 KiB。此高权限 API 依赖协作，并非沙箱；额外 Lua 发布者不能安装 Native 应用。应用认证和状态机放在应用中，见[原生小智](xiaozhi-native.zh_CN.md)。

真机自动化可用 `passport device button INDEX EVENT --port PORT`，索引 0／1／2，支持 press／release／click／long，double 仅允许 OK。注入与 BSP 相同的队列事件，press 不自动合成 click。实体按键时序及麦克风声学仍需硬件验收。

OS 1.0.36 为无 PSRAM C3 增加 Native 半双工策略：录音和播放交替持有编码器／解码器，播放前切换 `audio_mode(0)`；录音中直接播放返回 0。此策略由 Native 调用选择，Lua 原有双工行为保持兼容。仅播放启动不创建编码器。

C3 语音验证基线为 OS 1.0.45：PCM 工作区由堆持有，工作任务栈预算为 24 KiB，通用编码器以低复杂度 CELT 编码 20 ms 帧，再按 RFC 6716 聚合为 60 ms CBR 包。内部调度变化不改变 ABI 或线上包时长，Lua 和 Native 均保持 60 ms。Native 在编码器分配后才初始化 I2S，退出时释放 DMA／通道，保留共享 I2C。

编码输出缓冲区按 `esp_opus_enc_get_frame_info_by_cfg` 的要求检查，不能仅按压缩码率推算大小；当前配置报告 640 字节 PCM 输入及至少 160 字节输出容量。工作区、编解码器和 I2S DMA 的全部回收由 OS 负责。

Native 播放提交最多等待 80 ms，WS 工作任务交付最多等待前台 1000 ms，之后报告队列阻塞。以有界 TCP 背压承接语音突发，不增大缓冲；BSP 按键回调仍只入队。HTTP／WS 工作栈按实测余量设为 6／4 KiB，LVGL 池 16 KiB、显示 DMA 4 行。保留完整 TLS 链验证，仅释放握手后不再使用的对端证书副本。

WebSocket 在一个前台循环内最多交付四条消息，消息处理不直接等待 LVGL；按 Manifest FPS 和视图变化合并绘制。连接状态查询经源码核对为无锁，接收背压用于处理突发，不依赖更换该查询。

OS 1.0.45 增加独立 3 KiB WS 发送任务；前台排队已拷贝消息（八槽、待发载荷总计至多 4096 字节），不再与 SDK 接收锁同步竞争。退出等待任务结束并释放待发消息。英文系统下应用正文也固定使用多语言字体链，紧凑补充字库覆盖 GB2312 一级汉字及常用聊天标点。`font-probe` 检查实际控件字体，`display-metrics` 测量输入排队到刷新就绪延迟。

固定版本的 Espressif WebSocket 客户端启用官方独立发送锁（`CONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK=y`），避免接收语音流持有发送音频／控制所需的锁。音频输入／输出队列各容纳两包（每包 60 毫秒时共 120 毫秒），保持有界背压，退出时清空。

## 可选 BLE HID 扩展

OS 1.0.47 增加独立的可选 96 字节 HID 宿主表，前 80 字节保持核心 ABI 2 不变，已有二进制继续兼容。可信 Native 应用声明 `bluetooth.hid`，检查更大的宿主表长度后使用 [`passport_native_hid.h`](../../sdk/native/include/passport_native_hid.h)。扩展支持绑定的 BLE 指针／Consumer Control 报告，不提供 A2DP／HFP 音频或电话功能。API／生命周期和 iPhone 设置见[蓝牙外设](bluetooth-peripheral.zh_CN.md)。Lua 不能申请此首版扩展。

## 可选网络媒体扩展

OS 1.0.51 新增 120 字节的可选 Native 媒体表，提供 HTTPS MP3 播放、停止／状态、系统媒体音量、只读时钟与电池信息。保留 80／96 字节前缀，需 `audio.playback` 与 `network.fetch`，不需要麦克风授权。所有者、限制与迁移说明见[城市电台及通用媒体 API](city-radio.zh_CN.md)。

OS 1.0.56 新增公开广播专用 `audio.http`，要求可信 Native ABI 2、播放和获取权限、精确域名白名单及公网 IPv4 80 端口。禁止凭据和查询字符串，普通 fetch/WS 安全策略不变。建议使用 1.0.59 及后续版本，支持压缩字节拼块、自适应缓冲和仅输出播放。
