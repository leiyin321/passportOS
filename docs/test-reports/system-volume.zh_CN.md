[English](system-volume.md)

# 系统音量验收 — 2026-10-29

Passport OS 1.0.46 在“设置 → 声音”提供独立媒体／闹钟音量。UP／DOWN 每次调节 5%，OK 切换项目，双击 OK 返回主页。范围 0–100%，到达边界后不再变化，0% 静音。默认仍为 45%／35%，两项保存在系统 NVS，不改变麦克风增益或应用权限。

## 验证

Build：PASS — ESP-IDF 5.5.3 完整 `./tools/validate.sh`、归档验证、签名设置 OTA、设备 ELF 身份匹配及 OTA 自检。

Host tests：PASS — 完整门禁，以及无符号边界／限幅／两项独立测试、播放任务重新初始化后的静音和指定音量、既有回收／失败路径及新增中文字形覆盖。

Device tests：以下有界检查 PASS：

- 两项均达到 0%／100%，额外按键保持边界，另一项不变。
- 媒体 50%／闹钟 40% 在真实重启后恢复，切换英文／中文页面时数值保持不变。
- 原生小智连接官方服务，解码播放 51 个回复包，实际音频工作任务应用 `volume=50`，替代原来固定的 45%。
- 播放退出后可执行池为零；脱敏退出计数：`METRIC exit free=104272 minimum=10448 largest=38912 internal_free=104272 internal_largest=38912 retention_free=96512 retention_largest=38912 stack=2212 lua=0 lua_peak=0`。
- 测试修改恢复为媒体 45%／闹钟 35% 和中文 UI，保留闹钟时间／开关。

Unverified：人耳确认响度／静音、中文视觉显示、不同音量下真实定时闹铃，以及长期音频和声学输出测量。软件／驱动计数不代表主观听感。

## 占用及行为

| 产物 | 字节 |
|---|---:|
| OTA 应用镜像 | 2,332,320 |
| 合并固件 | 2,463,392 |

设置状态使用两个原子百分比，不增加常驻任务或堆缓冲。编解码器音量调用由工作任务负责。无效存储值保留默认值；持久化失败时提示错误并保持上次生效百分比。不改变包 ABI、分区或 Lua 支持。

## 产物及恢复

`build/firmware/74ba34369c09345eb2243b93b1bf3be2791af4e730f63ae2cbfe16b159b0693f`

ELF SHA-256: `612fdc286b09af19736d318efef56b9821a4d9d6075fe2e011bb17f88508f20d`.

签名发布构建号 `2026102913`，本机系统更新服务提供 1.0.46。原始记录保留在忽略目录 `local/volume/`。详见[设置指南](../developer/system-settings.zh_CN.md)和 [`volume.h`](../../os/services/audio/volume.h)。未执行 Git 提交／推送。
