[English](xiaozhi-native.md)

# 原生小智验收 — 2026-10-29

已交付原生小智 1.0.4（`org.passport.xiaozhi-native`）和 Passport OS 1.0.45。设备通过设置 OTA 安装 OS，未刷合并镜像、未擦除数据；真机执行商店目录及包下载、签名／哈希校验及安装。Lua 版保留独立 ID 和数据。

## 测量

下表单位均为字节，两种固件镜像都小于十进制 2.5 MB。

| 产物 | 字节 |
|---|---:|
| OTA | 2,331,440 |
| Merged | 2,462,512 |
| Native ELF | 12,420 |
| Native loaded span | 13,060 |
| Executable pool | 16,384 |
| Streaming task stack | 24,576 |
| Package | 13,404 |

Native 没有私有堆，代码／数据／BSS 使用永久预留池；网络、编解码和工作区另使用 OS 托管堆。退出后清空并归还池内空间，池本身仍保留。通用堆计数包含并发 Wi-Fi 和系统缓存，不能视为 Native 单独分配量。

## 真机语音结果

官方配置返回 HTTPS 200，证书校验成功，WSS／hello 到达 Ready；按下／松开交付麦克风包，执行服务端文本／TTS 回复，扬声器驱动消费了解码包。以下计数证明软件／驱动路径，不代表人耳清晰度验收：

- frames=0, Opus TX=0, RX=88, unused stack=13804 bytes, max read=0 us
- frames=50, Opus TX=49, RX=0, unused stack=3304 bytes, max read=45187 us
- frames=51, Opus TX=50, RX=107, unused stack=3300 bytes, max read=45224 us

最终测试未出现 panic、失败分配或接收队列超时，退出后 Native 池占用为零。脱敏指标 (`xiaozhi-native-results.txt`; 本地诊断，不公开)；原始日志、识别文本、激活信息及令牌只保存在忽略目录 `local/native_xiaozhi/`。

## 架构及修复

- ELF 内实现小智配置、认证、hello、listen、STT/TTS 状态机。OS 只提供通用 ABI 2 网络、JSON、身份、存储和经授权的 Opus 服务，不嵌入小智 provider。
- 需要连续 24,524 字节的编码器分配曾失败；调整分配顺序、选择 Native 半双工编解码器所有权并在退出释放 I2S／DMA，保留共享 I2C。
- 匹配 ELF 定位 SILK／CELT 栈保护故障，确立实际栈需求。流式音频使用 24 KiB 栈及堆上 PCM 工作区；通用低延迟 20 ms CBR 帧聚合为标准 60 ms Opus 包，线上协议不变。真实 libopus 编解码测试验证每包 16 kHz／960 采样点。
- Espressif 要求内部帧输出容量 160 字节，不能只按压缩后长度估算。OS 查询要求，提供有界 256 字节帧缓冲。
- 两条消息的 WS 接收队列曾被 TTS 突发填满；增加有界一秒背压、批量消息交付和合并绘制，接收消息之间不等待 LVGL。Native 音频提交至多等待 80 ms，BSP 按键回调仍非阻塞。
- 完整 TLS 证书链验证保持开启；仅释放握手后不用的对端证书副本。LVGL 池 16 KiB、DMA 四行、按实测调整 HTTP／WS 栈，回收 RAM；保留字库、Lua、断言和信任校验。
- 商店请求在前台校验安装结束前保持互斥，防止快速重复 OK 覆盖共享下载临时文件；提交失败时清空等待状态。

## 验证

Build：PASS — ESP-IDF 5.5.3 完整门禁及匹配归档验证。

Host tests：PASS — Lua／声明式／包／网络／音频回归、Native 解析器变异和 ASan/UBSan、有界无 NUL 结尾 JSON、真实 Opus 聚合解码、接收背压及错误、中英文原生协议状态机。

Device tests：上述 OTA、商店、Native 启动、官方网络／语音包链路和退出指标 PASS。真人语音清晰度及远端任意中文字符仍需人工验收，内置 UI 标签字形覆盖已检查。

Unverified：人耳清晰度、长时间语音压力测试、电流功耗、全新未绑定设备的远端激活；仍必须获得用户麦克风授权。

## 产物与恢复

归档：`build/firmware/529fc4652bdf03357206f07a9b0ebbd2e1c0188700a223b3ad02f55bd677c720`。

- OTA SHA-256：`fd5c16c32c4cce3e3bade457a038d3cba1151dc3c6cb9d6ce76717c9b4b9fb41`
- 合并镜像 SHA-256：`529fc4652bdf03357206f07a9b0ebbd2e1c0188700a223b3ad02f55bd677c720`
- 匹配 ELF SHA-256：`c304c627ecc229d90390aa4fed9dc3916cf0898027599e58aad7c42b9d69f1d0`
- 应用包 SHA-256：`5820c20afbddc406e50cd20ea4c41faeff08e05f2c53bdae5426dbc8dca9b686`

本机系统更新／商店服务为 8765／8040 端口。[原生小智源码及流程](../developer/xiaozhi-native.zh_CN.md)、[ABI 2](../developer/native-services.zh_CN.md)。恢复时先核对这些版本、归档和脱敏指标；保留 Lua／ABI 1 应用和数据。

## 字形、延迟及内存

实际应用正文控件字体探针的检查／缺失计数为：voice: 59/0, greeting: 105/0, repeat: 78/0，均无缺字；当时系统 UI 为中文。主机覆盖测试另验证了控件字体选择不依赖系统语言，以及完整 GB2312 一级字集。2,919 个补充字形增加 202,625 字节 Flash，位图不由堆持有。字集以外的生僻字、表情仍需应用提供合适资源。描述符探针不能代替人工确认裁剪／基线效果。

启动后空闲 8 位堆为 109,640 字节，实测会话最低空闲 8,604 字节，相对空闲状态减少 101,036 字节。该计数包含 OS 网络／音频分配和 Wi-Fi 活动，不是应用独立堆。原生可执行池峰值 13,060 字节，退出后归零。各轮退出（空闲／最大连续块，字节）：[('voice', 103840, 77824), ('greeting', 103312, 77824), ('repeat', 104268, 65536)]。

混合测试先播放问候，再按下／松开打断播放并采集，最后退出。历史 OS 1.0.43 在该路径出现 Wi-Fi 接收分配警告及语音发送失败；最终构建精简有界缓冲，并启用固定 SDK 官方的 WebSocket 独立发送锁。[显示延迟报告](display-performance.zh_CN.md)提供主页对比、探针限制及物理验收项。用户已确认早期构建能够听到回复；最终中文字形、按键手感和真人语音识别仍需人工验收。

## 兼容性及生命周期回归

OS 1.0.45 上，ABI 1 Native Hello 启动、处理注入的 OK 单击并退出；Lua 小智启动并退出，解释器回收（退出后 `lua=0`）。该检查验证启动／生命周期，不代表 Lua 远端语音服务验收。随后原生小智 100 次启动／退出全部通过，耗时 8,930,072 微秒，报告堆差值 36 字节，退出后可执行池为零。有界循环测试不等于长期无泄漏证明。
