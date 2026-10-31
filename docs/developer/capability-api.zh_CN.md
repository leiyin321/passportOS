[English](capability-api.md)

# Capability API 1 与应用生命周期

机器可读规范：[api-v1.json](../../sdk/passport/api-v1.json)，也可执行 `passport api`。
Manifest Schema：[manifest-v1.schema.json](../../protocol/manifest-v1.schema.json)。
本文明确当前实现，不把交接文档中的未来接口当成既有能力。

仅一个前台应用，签名发布者+ID 决定私有数据身份。应用拿不到 LVGL 指针、文件描述符、任务、GPIO、Flash、
原始无线或 Socket；调用使用系统持有的当前会话。退出时停止回调、取消/等待 HTTP、关闭 WS、等待音频停止确认、
释放 VM/代码/UI 状态、使作用域句柄失效。新的受信发布者也不能冒领已有或 Offload 后保留的其他发布者 ID。

```lua
function on_start(_) passport.text("Hello Passport", 0xf7fbff) end
function on_button(key) -- 短按；0 UP，1 DOWN，2 OK
  passport.text("Button " .. key)
end
function on_button_down(key) end -- 按下沿
function on_button_up(key) end   -- 松开沿
function on_button_short(key) end
function on_button_double(key) end
function on_button_long(key) end -- 按住达到 1,800 ms
function on_tick(dt_ms) end
function on_stop(_) end
```

三个按键在按下时都会立即派发 `on_button_down(key)`，物理松开时立即派发 `on_button_up(key)`。
UP/DOWN 每次松开都会立即派发一次 `on_button_short(key)` 和兼容旧应用的 `on_button(key)`；快速连续点按不会识别为双击。
OK 则等待 180 ms 双击判定窗口后再派发单击。双击 OK 返回主页，并抑制组成双击的两次单击事件。
`on_button_double` 保留但不会派发给 MiniApp；若应用需要方向键双击操作，可自行判断连续单击回调的时间间隔。
按住达到 1,800 ms 时派发 `on_button_long(key)`（及兼容旧应用的 `on_long(key)`），不再派发短按事件；
松开时仍派发 `on_button_up(key)`。长按 OK 留给应用使用。
按下/松开回调即时到达，因此语音应用可在按下 OK 时开始采集，松开时停止：

```lua
local microphone_allowed = false
function on_permission(allowed) microphone_allowed = allowed == 1 end
function on_button_down(key)
  if key == 2 and microphone_allowed then passport.microphone(true) end
end
function on_button_up(key)
  if key == 2 then passport.microphone(false) end
end
function on_stop(_) passport.microphone(false) end
```

应用需要声明 `audio.features`，并等待系统授权后才可采集音频。

双击 OK 返回 Home。`on_stop` 有预算限制，脚本错误后不保证执行；应在关键事件保存。
每次 KV 写入是原子的，多次 KV 写入不构成同一事务。Tetris 将完整进度存为一个受限值。
Offload 删除当前及历史代码，保留身份、入口和数据；彻底删除是单独的可信系统动作，缓存回收不得调用。

Lua5.4.8 使用 32 位整数/浮点，默认 Lua 堆 32KiB，Manifest 最大可设 64KiB；每回调 40,000 指令，解释代码目标 12ms。
原生服务调用可能延长实际耗时，须实测；每事件最多 4 次持久写入。画布最多 192 个矩形，严格约束
216×180 坐标，不分配应用全屏缓冲。Tick 按 Manifest 限制 1..20Hz。`text(value, color)` 更新有界 MiniApp 文本，可选 RGB24 前景色（`0x000000`–`0xFFFFFF`）。省略时 OS 使用易读的浅色 `#E9F0FA`；应用应根据自己的背景面板选择足够对比度的颜色。`clear/rect` 在文字后方绘制画布。

只开放部分 base/table/string/math 库。不开放 io/os/debug/package/coroutine、模块加载、动态代码加载、
pcall/xpcall（防止吞掉预算异常）、元表创建和脚本 GC 调整。移除模式匹配、string.dump/rep/format/pack/unpack。
`string.find(s,needle,start,true)` 必须为纯文本查找。错误、OOM、超时会停应用并显示可恢复错误，重新启动会创建全新 VM。

网络：

```lua
-- Manifest: network.fetch + domains:["api.example.com"]
function on_start(_)
  passport.http.request("POST", "https://api.example.com/session",
    { ["Content-Type"]="application/json" }, '{"hello":true}')
end
function on_http(status, body) end

-- Manifest: network.websocket + 精确域名白名单
local queued = passport.ws_connect("wss://api.example.com/socket", {
  ["Authorization"]="Bearer app-owned-token",
  ["Protocol-Version"]="1"
})
function on_http(status, body) -- WSS 客户端启动回调为 101；HTTP 请求返回服务端状态
end
function on_tick(_)
  if passport.ws_connected() then passport.ws_send("hello", false) end
end
function on_message(bytes, opcode) -- 1 文本，2 二进制，保留 NUL 字节
end
```

`http.request` 支持 HTTPS GET/POST，最多 8 个自定义请求头、2 KiB 请求体和 8 KiB 响应体。`on_http(status, body)` 返回服务端状态及有界响应体。TLS 证书必须有效；必须使用 Manifest 精确域名及 443 端口。GET 重定向逐跳重新授权；带自定义请求头时限制为同源。POST 不跟随重定向。拒绝传输层保留请求头及 CR/LF。`passport.json.decode/encode` 上限为 8 KiB、深度 8 和 256 个值。

`ws_connect` 仅接受 WSS，最多 4 个自定义请求头。排队完成后触发 `on_http(101, "")`；实际握手成功后还应使用 `ws_connected()` 判断，再发送数据。接收消息最多重组至 2048 字节，文本/二进制 opcode 保持区分，并采用有界队列；发送上限为每秒 50 条、16 KiB。仍执行 TLS、DNS/IP 和精确域名检查，关闭自动重连。仅声明 `device.identity` 时，`system.public_device_identity()` 才返回 Wi-Fi MAC 和稳定的应用级 UUID；不会暴露硬件密钥。

麦克风和通用 Opus 流：

```lua
function on_permission(allowed)
  if allowed == 1 then passport.audio.stream_start() end
end
function on_audio_frame(opus_bytes)
  -- 由应用决定是否以及发送到哪里
  passport.ws_send(opus_bytes, true)
end
function on_message(bytes, opcode)
  if opcode == 2 then passport.audio.play_opus(bytes) end
end
function on_stop(_) passport.audio.stream_stop() end
```

声明 `audio.stream`；若还需本地 RMS 特征，另声明 `audio.features`。前台授权提示会说明流式应用可把音频发往获准端点。当前流接口采集并编码为 16 kHz 单声道、60 ms Opus 帧，不向 Lua 暴露 PCM。播放接口接收 raw Opus v1 数据包并排队，按 16 kHz 单声道解码播放。采集/播放队列有界；应用退出或脚本错误时 OS 会停止任务、关闭编解码资源。`audio.stream_stop()` 不会阻塞 Lua 回调，只发出停止请求。`passport.microphone(true/false)` 仍是只返回本地 RMS 的接口，不上传音频。

MiniApp 可无权限读取用户设置的系统语言，并据此选择自身文本：

```lua
local language = passport.system.language() -- "en" 或 "zh-CN"
if language == "zh-CN" then
  passport.text("你好")
else
  passport.text("Hello")
end
```

`Settings → Language` 的值为 `en`（English）或 `zh-CN`（简体中文），接口只读。
MiniApp 负责翻译自身名称、内容和提示；系统语言不会自动改写应用提供的字符串。

系统图标及包内精灵已通过 `passport.ui.icon` / `passport.graphics.sprite` 实现，详见[资源API](assets.zh_CN.md)。

声明式应用使用独立且有界的 `ui.json` 文档，而不是 Lua 回调；支持的节点、操作和限额见[声明式 UI/1 规范](declarative-ui.zh_CN.md)。

尚未实现：独立焦点树的任意 UI 控件、line/画布文字、原始 PCM 访问、device/cache/channel/BLE、公共 OTA 传输、应用后台并行。这些必须以版本化 API 增加，Manifest 不能自行开启。

小智示例 MiniApp 使用上述通用能力，由应用自行完成配置请求、认证请求头、JSON hello/会话状态、Opus 封装及对话界面。小智专属服务端规则、账户 Token 和协议解释都留在应用中，详见[小智 MiniApp](xiaozhi-miniapp.zh_CN.md)。

`passport.system.display_mode()` 只读取 Settings 中选择的系统外观，返回 `dark` 或 `light`。应用可以跟随这个值选择配色，也可以独立定义背景和前景颜色；OS 不会把系统主题强加给应用画布。`passport.text(value, color)` 支持可选 RGB24 文字颜色，应用也可用 `passport.rect` 绘制自己的背景面板。

## 审查增量：OS 1.0.22

HTTP 响应支持有界分块/EOF 和 Content-Length。响应头读取失败按传输错误处理，
截断或超限响应不会作为成功交付；OTA 仍要求签名中的精确大小。请求头键值拒绝
NUL、CR/LF 和保留字段。响应上限不代表提前预分配 8 KiB：TLS 握手后随数据到达增长。

JSON 在递归 C 解析前检查深度和值数量；Lua 转换内存不足时也释放临时 JSON 分配。
嵌入 NUL 的键和字符串直接拒绝，不再静默截断。

运行时编译/解析后会消费并释放已加载包源码，C 调用者再次启动前须重新加载包；
MiniApp API 不变。Lua 配额与系统可用堆独立，请求/codec 仍可能因系统内存不足失败。
资源证据及生命周期/整机内存后续工作见[系统审查](../development/passport-os-review.zh_CN.md)。

### TLS 内存行为（OS 1.0.24）

HTTP 和 WSS 保留证书、主机名验证以及域名策略。OS 在自定义 HTTP/WSS 提交前回收 Lua 不可达对象，使用 ESP-IDF 动态 TLS 记录缓冲降低堆峰值。这是通用基础能力，不是专用服务 provider。提交返回 false 可能是忙碌、资源或策略问题；异步 HTTP -1 可能是传输/TLS 失败，两者均不等于 Wi-Fi 断开。应用应限制重试次数并保留错误反馈。

按键应用收到 101 后必须继续保留“连接中”标志，直到 `ws_connected()` 成功或明确失败/超时才清除。一次按下后产生的单击事件不能重启握手。

OS 1.0.26 在 WebSocket Host/SNI/证书验证中保留批准的域名，同时将 TCP 连接固定到网络策略已检查的 IP；socket 连接阶段不再次解析域名。ESP-IDF 5.5.3 专用 connect 适配器隔离在 `os/services/network/ws_transport.c`，升级 IDF 时须重新验证其私有 transport ABI。

Opus 播放输出为 16 kHz 单声道 PCM。不能仅因为发送端使用其他合法 Opus 采样率就拒绝兼容码流；解码器决定输出采样率。当前播放缓冲最多容纳 16 kHz 下每包 60 ms。OS 1.0.27 为音频任务配置 10 KiB 栈，实机栈/堆余量仍需验收。

`audio.stream_start()` 在线程无法创建时可能返回 false。后台线程随后启动失败时，OS 会清除当前采集状态，但保留用户授权，并调用 `on_audio_error(code)`；代码 1 表示音频流启动失败。应用应停止并清理音频状态，允许稍后重试。只有用户明确拒绝时才调用 `on_permission(0)`。

Native OS 1.0.51 新增 `audio.playback` 权限，仅限可信 Native ABI 2 应用；结合 `network.fetch` 使用可选 HTTPS MP3 媒体扩展，不需要麦克风授权。必须显式声明台源及重定向域名。参见[媒体所有者、API 与限制](city-radio.zh_CN.md)。
