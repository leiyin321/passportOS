[English](bluetooth-peripheral.md)

# 原生蓝牙外设

源码：[`examples/bluetooth_peripheral`](../../examples/bluetooth_peripheral/main.c)。应用 ID `org.passport.bluetooth-peripheral`，版本 0.1.4，Native ABI 2 的可选 HID 扩展。安装前先升级 OS 1.0.50。既有 ABI 1、核心 ABI 2 和 Lua 应用契约保持不变。

## 硬件范围

ESP32-C3 仅有 BLE，不支持经典蓝牙。本机无线硬件无法实现标准 A2DP 耳机音频、AVRCP 或 HFP 通话。本应用不将手机音频传给扬声器，也不接听电话。参见[乐鑫蓝牙能力表](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/bt-architecture/overview.html)。

## iPhone 设置

### 推荐：自定义手势按钮映射

用户已在 iPhone 12 Pro Max／iOS 27、OS 1.0.50／应用 0.1.3 上确认：映射自定义手势后，上下滑动和 OK 暂停／继续均成功。此方案为该实测配置的推荐路径，其他设备／应用仍需分别验收。

应用 0.1.4 默认选中 **iPhone 手势按钮映射**。按 OK 进入，配对 Passport Remote 后，在 iPhone 打开“设置 → 辅助功能 → 触控 → 辅助触控 → 设备 → Passport Remote → 自定其他按钮”。这是已连接设备的按钮设置，不是辅助触控悬浮菜单的单点／双击设置。

- 配置时短按实体 UP：将 **按钮 2** 绑定到已保存的“上划”手势。
- 短按实体 DOWN：将 **按钮 3** 绑定到“下划”手势。
- 将 **按钮 1** 设为“单点”，实体 OK 发送该按钮。

OS 只发送标准按钮按下／松开报告，无法通过 BLE 按名称选择 iOS 保存的手势；映射在 iPhone 设置并保存。部分自定义手势先进入位置选择状态；出现手势标记时，可尝试按 OK 确认指针位置，或在手机上选位置。目标 iOS／应用实测通过前不能保证一次按键自动执行。此模式不会自动追加确认点击，避免意外暂停视频。长按 OK 打开应用菜单，双击 OK 退出。

### 首页与配对

0.1.4 首页只保留 **iPhone 手势按钮映射** 和 **清除配对** 两项。UP／DOWN 选择，OK 打开。已移除直接拖动、滚轮、指针校准、媒体遥控和蓝牙耳机说明入口。

选择手势按钮映射后，以 **Passport Remote** 名称广播至多三分钟。在 iPhone 启用辅助触控，通过蓝牙设置或“辅助触控 → 设备 → 蓝牙设备”配对。更新应用保留已有配对及手机上的按钮映射。使用时保持 iPhone 唤醒。参见 [Apple 指针设备指南](https://support.apple.com/en-gb/111775)。

**清除配对**需要再次按 OK 确认，清除 OS 保存的 HID 配对；也应在 iPhone 忽略 Passport Remote 后重新配对。配对信息由整机 Native HID 应用共享，不属于应用私有存储。最多保存三个配对，同时连接一个设备。

## 通用 Native API

引入 [`passport_native_hid.h`](../../sdk/native/include/passport_native_hid.h)，声明 `bluetooth.hid` 权限。此首版扩展仅开放给可信 Native ABI 2 应用，Lua 不能申请该权限。OS 提供通用指针／Consumer 报告，不包含 iPhone 或短视频服务商逻辑。

C3 上核心 ABI 2 仍为 80 字节，可选扩展后为 96 字节。转换指针前检查 `host->abi==2` 和 `host->size>=sizeof(passport_native_host_hid_v2)`；应用回调表仍为 20 字节。按原 80 字节核心表编译的应用继续兼容更大的宿主表。

| 函数 | 契约 |
|---|---|
| `hid_start(flags)` | 异步启动；0 保留配对，1 明确清除后重新配对；拒绝其他标志和重叠启动 |
| `hid_state()` | 0 关闭、1 启动中、2 广播中、3 已连接／配对中、4 加密鼠标订阅就绪、5 错误、6 停止中、7 不可用 |
| `hid_report(1,data,4)` | 三个按钮位、相对 X／Y／滚轮有符号值，各轴 -127…127 |
| `hid_report(2,data,2)` | 小端 usage：0 松开、B5 下一首、B6 上一首、CD 播放暂停、E2 静音、E9 增大音量、EA 降低音量 |
| `hid_stop()` | 异步停止；新启动等待回收，不保留应用内存或回调 |

报告使用八槽拷贝队列，每秒最多 40 条。返回 1 表示入队成功，不表示手机确认完成操作。未知报告、缺少加密／订阅、队列忙、超配额均返回 0；已入队报告发送失败会断开连接，避免输入长期保持按下。每个点击／媒体操作都必须松开，双击不能提前发送组成它的单击。

仅前台会话所有者启动无线协议栈。NimBLE 管理 GATT、加密、配对及通知，初始化／回收由有界工作任务执行，不在按键回调中阻塞。首次使用后保留一个小型可复用 OS 邮箱，其他任务、连接和协议分配均应回收。协议栈无法安全退出时拒绝重启，持续错误／停止状态需重启 OS。配对使用绑定、加密的 Just Works，不宣称带显示 PIN 或中间人防护。

## 构建及发布

沿用 `passport native build`、`pack`、`validate`、`install` 和 `publish-index`。与其他 Native 应用相同，需要系统内置信任发布者。不得将发布私钥或原始配对／连接日志放入包或仓库。见 [Native 应用](native-apps.zh_CN.md)、[Native 服务](native-services.zh_CN.md)及[包协议](package-protocol.zh_CN.md)。

原型 DIS PnP 标识使用 Bluetooth 来源的内部／测试厂商 `0xFFFF`、产品 `0x0001`、版本 `0x0100`，不宣称第三方分配的产品身份。挂起期间停止报告，恢复时松开已按下输入。

配置时保持 iPhone 唤醒。当前 HID Information 不宣称远程唤醒，宿主挂起后停止报告，直到宿主退出挂起。USB `ble_debug` 可区分挂起与未加密／未订阅。验收状态见[蓝牙检查点](../test-reports/bluetooth-peripheral.zh_CN.md)。
