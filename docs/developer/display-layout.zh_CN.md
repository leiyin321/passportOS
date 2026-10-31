[English](display-layout.md)

# Display 布局与 API v1

屏幕为 240×320 纵向。Manifest 的 `display` 可省略，默认标准模式：

```json
"display": { "mode": "standard", "orientation": "portrait", "navigation": "select" }
```

`mode` 只接受 `standard`/`fullscreen`；方向只支持 `portrait`，否则包校验失败。
`navigation` 是受限提示意图：`buttons`、`scroll`、`select`、`game`，OS 自己选择本地化底栏。
`design_width`/`design_height` 仅为建议尺寸，应用不能动态切换模式。

| 模式 | 状态栏 | 应用视口 | 导航栏 |
| --- | --- | --- | --- |
| standard | y=0..23 | (0,24)，240×272 | y=296..319 |
| fullscreen | 隐藏 | (0,0)，240×320 | 隐藏 |

safeInsets 全为 0；主题建议的 8px padding 单独计算。Lua 使用：

```lua
local vp = passport.display.get_viewport() -- getViewport 为别名
-- vp.width、vp.height、vp.mode、vp.safeInsets.left/top/right/bottom
```

视口只读。`passport.rect`、系统图标、包内 sprite、声明式 image 节点采用视口坐标。
矩形会裁切到视口；非法原点、负尺寸、不支持的图片和越界坐标会安全失败。应用画布和文字
只能挂在 OS 拥有并裁切的 LVGL AppViewportHost 中。不得访问系统对象树、驱动、LCD 句柄或帧缓冲。

声明式 `ui.json` 可绑定 `display.width`、`display.height`、`display.mode` 和
`display.safeInsets.*`。保留的运行时 `display` 对象不会写入应用存档。
标准模式的底栏提示通过 `navigation` 意图设置；按键继续使用稳定的
`on_button(0 UP / 1 DOWN / 2 OK)`，Lua 会收到即时按下/松开边沿和单击/长按回调。
UP/DOWN 每次松开立即产生单击，连续点按不会合并为双击。OK 单击确认；OK 等待 180 ms 判断是否双击，
双击返回 Home 并抑制两次单击回调；长按 OK 派发给应用。BSP 长按阈值为 1,800 ms。

OS 状态栏管理应用名、网络和电量，底栏管理导航提示。标准应用得到 240×272 裁切区；全屏隐藏两栏，
提供 240×320。麦克风权限提示期间临时恢复系统栏。应用退出或异常时恢复系统界面，释放图像、音频、
网络和 VM，保留安装包及私有存档。

## 手机 Wi-Fi 配网

在主界面打开 **Settings** 并按 OK。Passport 会启动临时 WPA2 热点，设备屏幕显示本次会话随机密钥。
手机连接该热点后访问 `http://192.168.4.1`，输入目标 2.4 GHz Wi-Fi 名称和密码。配网页面由设备本地提供；
凭据由 ESP-IDF Wi-Fi 配置写入 NVS，不输出到串口日志。Station 获取 IP 后，配网服务和热点自动关闭。
双击 OK 或执行 `home` 可以取消；网络拒绝凭据时热点保留，允许重试。当前版本需要手动输入 URL，尚未实现
基于 DNS 的强制门户。

表单限制为一个 SSID（1–32 字节）和一个密码（开放网络可留空，或 8–63 字节），请求体最多 512 字节；
解码 URL 表单转义，拒绝重复/未知字段、控制字符、无效转义和截断数据。

Weather/Chat 为标准模式，Tetris/VoiceBird 为全屏。宿主、真机和视觉检查分别记录在[测试报告](../test-reports/display-layout.zh_CN.md)。
API 不分配整屏帧缓冲。
