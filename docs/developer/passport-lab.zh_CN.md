<p align="right">
  <strong>简体中文</strong> · <a href="passport-lab.md">English</a>
</p>

# Passport Lab MiniApp

`org.passport.device-lab` 是可从 App Store 安装的 MiniApp，参考了官方 AI Passport
出厂硬件演示的用途。它只使用 Passport OS 已公开的 MiniApp API，不复用出厂测试菜单界面。

应用提供屏幕色块测试、按键按下/抬起/UP-DOWN 长按事件测试、经用户授权的本地麦克风 RMS
音量表、应用私有持久存储测试，并说明哪些硬件测试由系统保留。标准模式下系统状态栏仍
可见，包括电池百分比；背光和熄屏时间在 Settings 中管理。

MiniApp 不能扫描 Wi-Fi、启动 BLE 广播、读取原始按键 ADC、播放原始音频或进入浅/深睡眠；
这些能力根据系统 Capability 策略仍由 OS 独占。Audio 页面只接收 RMS 特征，不暴露、保存
或发送原始音频。单击 OK 确认，双击 OK 返回主页；长按 OK 可由应用处理。

已签名 PAP 位于 `examples/packages/passport-lab.pap`，静态商店目录在 `dist/`。使用项目发布
者密钥重新打包的命令：

```sh
PYTHONPATH=sdk python3 - <<'PY'
from pathlib import Path
from passport import pack
Path('examples/packages/passport-lab.pap').write_bytes(
    pack('examples/passport_lab', 'local/publisher-private.pem',
         'sdk/publisher-public.pem'))
PY
cp examples/packages/passport-lab.pap dist/passport-lab.pap
passport publish-index dist --public sdk/publisher-public.pem \
  --base-url http://192.168.1.10:8040 -o dist/index.json
```

发布者私钥保存在被 Git 忽略的本机目录，不能复制到源码仓库或 MiniApp 包中。
