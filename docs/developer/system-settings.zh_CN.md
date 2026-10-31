<p align="right">
  <strong>简体中文</strong> · <a href="system-settings.md">English</a>
</p>

# Passport OS 系统设置

Settings 采用适配 240×320 屏幕、参考 iOS 的分组列表。从主界面用 UP/DOWN 选择
**Settings** 并按 OK 进入；一级菜单使用可滚动视口，UP/DOWN 浏览全部项目，右侧滚动条提示当前位置，OK 打开详情，双击 OK 返回主界面。配置项增加时继续向下滚动即可，不要求挤在一屏。单击 OK 确认当前操作；长按 OK 可供 MiniApp 使用。

## 当前项目

| 分组 | 项目 | 行为 |
| --- | --- | --- |
| 连接 | Wi-Fi | 显示 Station 状态并启动手机配网临时热点 |
| 连接 | Bluetooth | 显示蓝牙支持是否编入固件、配对服务是否可用 |
| 个性化 | Display | 以 10% 为步进调节背光（20–100%） |
| 个性化 | Appearance | 在深色和浅色系统界面间切换；保存到 NVS |
| 个性化 | Screen Timeout | 选择 15 秒、30 秒、1 分钟、2 分钟、5 分钟或永不 |
| 个性化 | Sounds | 分别调节媒体／闹钟音量，0–100%、每次 5%；OK 切换调节项目 |
| 隐私 | Permissions | 说明应用级麦克风授权、向白名单端点传输音频及 Manifest 控制的网络访问 |
| 系统 | Date & Time | 连接 Wi-Fi 后通过 `pool.ntp.org` 自动同步；显示日期、时间、同步等待状态及 Asia/Shanghai（UTC+08:00）时区 |
| 系统 | Alarm | 系统每日闹钟；配置时间和启用状态，时间有效后触发，按任意键停止 |
| 系统 | System Update | 使用签名 HTTPS 清单检查更新，下载至非活动 OTA 槽，校验后手动确认安装 |
| 系统 | Language | 选择 English 或简体中文；设置保存在系统 NVS，并应用于全部系统界面和提示 |
| 系统 | Storage | 显示应用包存储状态和已安装应用数量 |
| 系统 | About | 显示 Passport OS 固件语义版本、硬件目标、ESP-IDF 版本和 App API 版本 |

背光亮度和熄屏时间保存在 NVS 的 `osconfig` 命名空间，启动时恢复。熄屏计时到期后
关闭显示背光；第一个按键只唤醒屏幕，不触发当前选择。麦克风授权提示和手机 Wi-Fi
配网期间保持屏幕亮起。“永不”会关闭空闲计时器。

声明 `audio.stream` 的应用会看到单独的授权说明：采集期间，应用可将 Opus 音频发送到其获准的
网络服务。只有应用显式请求通用音频流 API 后才开始采集；应用退出或运行时故障时，OS 会停止音频任务并回收资源。

系统时间使用 SNTP 公共服务器 `pool.ntp.org`，设备连接互联网并获取 Wi-Fi 地址后自动开始同步。时钟显示在顶部状态栏；首次同步前显示 `--:--`。时区固定为 `Asia/Shanghai`（UTC+08:00），同一次开机内断网时系统时钟继续走时；断电重启后会等待网络重新同步。系统闹钟只在时间有效后调度，设置保存在 `osconfig` NVS 中。详见[系统闹钟规范](system-alarm.zh_CN.md)。

System Update 需要先运行 Passport OS，再通过 USB developer CLI 配置受信 HTTPS `latest.json`
清单地址。检查、下载和安装由用户在设置页逐步确认；只有签名与镜像校验成功才会切换到非活动 OTA 槽。首次安装/从厂商固件迁移仍需 USB 刷写，更新协议与发布命令见
[系统更新指南](system-updates.zh_CN.md)。

语言设置通过 UP/DOWN 在 English 与简体中文间切换并自动保存。MiniApp 可调用
`passport.system.language()` 读取 `en` 或 `zh-CN`，再自行选择应用内容的翻译；系统不会
自动翻译 MiniApp 自己提供的字符串。

语言选择覆盖系统导航、设置项名称/状态/详情、状态栏和按键提示、配网与权限对话框、
应用商店/安装错误、系统更新进度/错误及运行时错误提示。应用名称和内容仍由应用负责
翻译。系统中文使用生成的思源黑体字形子集并回退到 LVGL 内置 CJK 字库；来源、许可和
生成方法见[资源说明](../../assets/README.zh_CN.md)。

Appearance 通过只读接口 `passport.system.display_mode()` 向 MiniApp 提供系统模式
（`dark` 或 `light`）。应用自行选择跟随系统模式还是使用独立配色；可通过
`passport.text(value, rgb24)` 设置文字颜色，并用 `passport.rect` 绘制背景面板，OS 不会覆盖应用画布颜色。

蓝牙条目在配对服务接入前只显示说明状态。系统闹钟通过板载音频编解码器和扬声器发出提示音。
增加未来设置项时，应扩展有界设置
注册表，为每项提供真实状态和详情页，并只把实际生效的配置持久化到 `osconfig`。
按键处理不能执行阻塞操作。

## 声音音量

从 Passport OS 1.0.46 起提供。

进入 **设置 → 声音**，UP 增加、DOWN 降低当前选中音量，每次 5%；OK 切换媒体／闹钟音量，双击 OK 返回。两项均支持 0%（静音）到 100%，达到边界后不再变化，自动保存到 `osconfig` NVS（`media_volume`、`alarm_volume`）。默认媒体 45%、闹钟 35%；存储值无效时使用默认值。保存失败时显示错误并保持上次生效值。

两项由 OS 管理，Lua／Native 的 Opus 播放使用媒体音量，系统闹钟工作任务使用闹钟音量。开始播放不再覆盖系统设置。音频工作任务在自身上下文应用变化，编解码器唤醒／重新初始化后也重新应用。播放音量不改变麦克风采集增益，也不授予麦克风权限。实际声压、扬声器失真和主观响度仍需真机试听。

验证记录：[系统音量验收](../test-reports/system-volume.zh_CN.md)。

蓝牙状态与 iPhone 使用指南见[原生蓝牙外设](bluetooth-peripheral.zh_CN.md)。ESP32-C3 不支持经典蓝牙耳机音频／通话。
