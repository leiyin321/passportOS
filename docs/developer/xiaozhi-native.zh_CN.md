[English](xiaozhi-native.md)

# 小智 AI Native

源码：[`examples/xiaozhi_native`](../../examples/xiaozhi_native/main.c)。应用 ID `org.passport.xiaozhi-native`，版本 1.0.4，`native-elf`、ABI 2。**下载前先升级 Passport OS 1.0.45；1.0.34 仅实现 Native ABI 1。** Lua 版保留独立应用 ID 和私有数据。

应用实现服务配置／激活、WebSocket 认证、hello 协商、按住说话、Opus 包传输、STT 识别与 TTS 回复显示。不使用 Lua 解释器，也不使用 OS 小智 provider。系统 Wi-Fi、HTTPS/WSS 传输／策略／TLS、授权、Opus 编解码任务、托管显示及退出回收交由 OS 的[通用 ABI 2 服务](native-services.zh_CN.md)。小智地址、认证头、JSON 消息及对话状态由应用源码维护。

协议参考[官方 WebSocket 规范](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)，[FoloToy Passport 适配](https://github.com/FoloToy/folo-ai-passport-xiaozhi)用于硬件／应用背景。MiniApp 不安装服务端返回的固件或执行其 OTA 命令；系统更新仍由 Passport 设置管理。

## 使用

1. 在设置 → 系统更新升级 OS，再从本机 App Store 下载 **XiaoZhi AI Native**。
2. 打开后单击 OK 授予麦克风权限，UP／DOWN 拒绝。重新打开应用可再次申请授权。
3. 读取 OS 实际 Wi-Fi 状态，向官方服务获取配置、建立认证 WSS 并交换 hello。服务提供激活码时会显示，供在 xiaozhi.me 绑定；激活属于应用及服务，不属于 OS 设置。
4. 语音页按住 OK 说话，松开结束，回复由系统音频任务播放。UP／DOWN 切换语音、对话记录及服务页；双击 OK 返回主页并回收资源。
5. 出错时在语音页或服务页按 OK 重试。服务超时不会无限重连，HTTP／WSS 错误保留数字状态码。语音缓冲有上限，屏幕展示识别／回复摘要。

UI 跟随系统中／英文，应用自行绘制高对比度深／浅色界面。OS 字库补充语音、激活及页面所缺字符，并验证覆盖。远端任意对话文本中超出 OS 子集的字符仍可能缺字。

录音为单声道 16 kHz、60 ms Opus。服务端 hello 必须选择 WebSocket、Opus、单声道、支持的 Opus 采样率（8／12／16／24／48 kHz），提供帧时长时仅接受 20／40／60 ms。Opus 播放解码为系统单声道 16 kHz。不支持的格式、超长凭据、设备不可用和拒绝授权会明确提示；公开日志不包含凭据和激活码。

## 编译与发布

先启用 ESP-IDF 5.5.3：

```sh
PYTHONPATH=sdk python -m passport native build examples/xiaozhi_native
PYTHONPATH=sdk python -m passport pack examples/xiaozhi_native \
  --key /path/to/os-publisher-private.pem --public sdk/publisher-public.pem \
  -o dist/xiaozhi-native.pap
PYTHONPATH=sdk python -m passport validate dist/xiaozhi-native.pap --public sdk/publisher-public.pem
PYTHONPATH=sdk python -m passport publish-index dist --public sdk/publisher-public.pem \
  --base-url http://YOUR_MAC_IP:8040 -o dist/index.json
PYTHONPATH=sdk python -m passport serve dist --host 0.0.0.0 --port 8040
```

HTTP 仅作为本地开发服务，需要设备已有局域网授权；包仍执行 RSA 签名及 SHA-256 校验，私钥不得分发。Native 安装仅接受 OS 内置发布者。目录的 `native-elf/2` 能力标记用于说明，旧固件会拒绝不支持的 Manifest ABI。

验证和真机验收状态见[测试报告](../test-reports/xiaozhi-native.zh_CN.md)。宿主 mock 证明协议和回收请求路径，不代表远端激活或实际可听语音通过。

在对话记录页单击 OK 可向小智发送问候（官方 `listen/detect` 消息），用于不按住麦克风时确认服务回复和扬声器播放。配置接口遇到暂时错误最多重试三次，之后等待用户重试。商店发布应将每个应用的当前包放在独立索引输入目录，生成索引后保留已发布的版本文件，避免旧下载失效。
