<p align="right">
  <strong>简体中文</strong> · <a href="wifi-provisioning.md">English</a>
</p>

# 通过蓝牙配网连接 Wi-Fi

当用户需要设备通过 Wi-Fi 连接网络时，可以参考
[`demo/blufi-provisioning` 分支](https://github.com/FoloToy/ai-passport/tree/demo/blufi-provisioning)
的代码实现蓝牙配网。手机通过 BLE 上的 BLUFI 协议发送 Wi-Fi 名称（SSID）和
密码，设备以 Wi-Fi STA 模式连接网络并回报连接状态。蓝牙用于传递配网信息，
不是承载应用的互联网流量。

这是可选的应用参考。当前 `main` 的 Wi-Fi demo 仅扫描网络，并未实现联网或
蓝牙配网；没有联网需求的应用不必启用网络功能。

<a id="mini-program-name"></a>

## 小程序名称

配套小程序的准确名称为：**蓝牙配网-FoloToy AI PASSPORT**。

向用户说明或搜索小程序时，请使用上述完整名称，不要自行翻译或改写。
这是小程序名称，不是设备的蓝牙广播名；参考固件的广播名为
`BLUFI_FoloPassport`。

预期流程：设备进入配网状态，手机打开蓝牙并授予所需权限，打开该小程序，
选择目标设备，提供 2.4 GHz Wi-Fi 的名称和密码。确认设备取得 IP 地址后，
再验证应用实际需要的网络请求；仅蓝牙连接成功不能证明能够访问互联网。

## 代码参考入口

- [`main/demo_blufi.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_blufi.c)：BLUFI 回调、Wi-Fi 扫描与连接、凭据处理、状态回报和生命周期。
- [`main/demo_blufi_security.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_blufi_security.c) 及其头文件：BLUFI 安全协商回调，提取示例时不要丢弃这些逻辑。
- [`main/demo_radio.c`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/demo_radio.c)：NVS、`esp_netif` 和默认事件循环的共享初始化。
- [`main/CMakeLists.txt`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/main/CMakeLists.txt) 和 [`sdkconfig.defaults`](https://github.com/FoloToy/ai-passport/blob/demo/blufi-provisioning/sdkconfig.defaults)：源文件注册、依赖、NimBLE/BLUFI 和加密配置。不能只复制单个 C 文件就认为完成接入。

## 接入与验收

从应用当前基线出发，记录参考提交，仅移植适用的联网逻辑。不要直接合并整个
demo，也不要用该分支的旧版 BSP、分区表或配置覆盖当前版本。重新实现应用
自己的界面，将配网状态、任务及 Wi-Fi/BLE 服务放在应用层。

明确配网入口与退出、超时、重试上限、凭据持久化及主动清除凭据的方式。
禁止记录或提交 Wi-Fi 密码。保留 LVGL 锁、非阻塞回调、任务与事件处理器清理
机制，并评估 Wi-Fi、BLE 和 UI 同时运行的内部 RAM 占用。示例不等于完整的
生产环境授权或安全设计。

运行[验证门禁](build-and-test.zh_CN.md)，再遵循
[真机测试交接流程](../ai-guide.zh_CN.md#主动询问真机测试)。使用上述小程序
验证：发现设备、成功配网、密码错误与网络不可用、重连与重启、清除凭据、
反复进入退出配网，以及应用实际需要的网络请求。编译成功不能证明小程序
兼容性或实机联网正常；未执行的检查列入 `Unverified`。

## Passport OS 手机本地配网页

“设置 → Wi-Fi”创建临时加密热点，在 `http://192.168.4.1/` 提供自包含页面。由 ESP32-C3 扫描附近 2.4 GHz 网络，不依赖手机浏览器扫描能力。最多读取信号最强的 16 条记录，合并同名 SSID，隐藏网络可手动输入。失败时可重新扫描或手动填写。扫描运行在 HTTP 服务任务，离线时暂缓自动重连，扫描后恢复；释放驱动结果和临时 JSON 内存。

页面支持中/英、列表选网、显示密码、手动输入，以及标准 `WIFI:T:WPA;S:...;P:...;;` / `nopass` 分享文本导入，包含分隔符转义。导入只填表，须核对后提交。不能枚举手机保存的网络/密码、识别二维码图片或调用 Apple 附近密码共享协议。iPhone 用户可从“密码 → Wi-Fi”复制已保存密码后粘贴（[Apple 指南](https://support.apple.com/en-euro/guide/iphone/iph96e6aa9bb/ios)）。不需要外部资源、记录凭据、上传云端或剪贴板权限，保留现有加密热点及 POST 提交流程。

主机回归使用 Node.js 执行实际页面脚本（`tests/test_wifi_setup_page.py`），验证分享文本转义、重复/不支持字段、UTF-8 字节上限及使用 `textContent` 显示 SSID。手机连接热点、扫描结果、保存重连仍需实机验收，主机测试不能代替。

## 多个已保存网络（OS 1.0.63）

最多保存 8 个不同 SSID，使用 `appdata/wifi_profiles/saved` 下带版本的 780 字节 NVS blob；列表和扫描响应从不返回密码。仅当该键不存在时迁移原有驱动单网络配置，合法空列表表示用户已忘记全部网络，不重新迁移。损坏/不可读数据报错，不静默恢复已忘记项。相同 SSID 更新密码，新 SSID 追加；满额拒绝新增，不自动淘汰。NVS 操作失败保留内存中的旧列表及连接，断电一致性由 NVS 保证。本功能未开启 NVS 加密。

独立 4 KiB 工作任务负责 NVS 和连接/重试，事件回调只更新联网标志。开机或断线先尝试最近成功网络；单次尝试 12 秒后扫描，优先未试过且可见的最近成功网络，否则按信号选择其他已保存网络。定向连接兼容已知隐藏 SSID。全部尝试后退避 30 秒再循环；已连接时不主动漫游或扫描。成功连接仅在位置变化时将网络移到首位并保存。手动/自动扫描及结果读取释放由同一无线锁串行化。

手机页展示保存的名称，提供忘记确认、刷新及满额/存储错误提示。`GET /saved` 只返回名称、状态和容量；`POST /wifi`、`POST /forget` 接受有界 URL 编码名称/密码，并要求 `X-Passport-Setup: 1` 请求头（同源 JavaScript，不允许跨源 CORS 预检）。忘记请求使用空密码，以 SSID 匹配而非易变的列表下标。POST 接受表示已排队，不代表联网成功；状态 0=正常、1=满额/无效、2=存储失败、3=处理中、4=指定网络失败已回退其他保存网络。仅指定网络成功才自动关闭配网页，回退时保留供修改。取消只停网页/热点，保留保存网络及自动重连。回滚旧 OS 可能仍使用驱动原单网络凭据；不修改布局或擦除。

最终版本采用 ESP-IDF `WIFI_ALL_CHANNEL_SCAN`、按 RSSI 选接入点，以及每个接入点两次驱动重试，在切换保存网络前容忍认证/关联瞬时失败。1.0.61 是中间测试版本：数据持久化正确，但单次启动连接偶有认证超时。断线诊断只记录原因码。修改 `os/services/network/wifi_setup_page.html` 后执行 `python3 tools/passport/build_wifi_setup_page.py` 生成静态 gzip 资源；主机测试验证解压内容与 HTML 完全一致，浏览器自动解压 HTTP 响应。最终实机检查点见[手机配网报告](../../test-reports/wifi-phone-setup.zh_CN.md)。

OS 1.0.63 修复忘记失败目标网络后意外关闭配网页的问题：忘记操作在排队前清除待连接意图，HTTP 与主任务间使用原子标志。只有后续明确发起且成功的连接才自动关闭；单纯管理列表不会关闭页面。

## 扫码加入配网热点（OS 1.0.65）

“设置 → Wi-Fi → 手机配网”分三步。第一步显示临时 WPA2 热点的标准 `WIFI:T:WPA;S:...;P:...;;` 二维码，手机相机可提示加入，不必输入随机密码。按 OK 显示包含 `http://192.168.4.1/` 的网页二维码，再按 OK 显示手动名称/密码/地址备用页，再按返回第一步。UP/DOWN 切换步骤，双击 OK 取消。加入和打开网页仍需手机用户确认，不是自动读取手机保存的密码。

保留随机十二位密钥、两客户端上限、已保存网络、扫描/列表/忘记接口及同源 POST 检查。三步使用同一组热点凭据，新会话重新生成，取消/成功后清空显示密钥缓冲并释放二维码资源；服务已运行时再次读取真实 AP 配置，不返回占位名称。普通日志不输出二维码内容/密钥。

复用 LVGL 9.5 二维码组件和编码器，显式开启 I1 渲染及 Canvas/QR，仍关闭 RGB888/XRGB8888/L8/AL88。176×176 I1 像素缓冲通过复制的绘图分配器使用系统堆，元数据及临时编码缓冲由 LVGL 管理；替换时释放构造器旧缓冲，删除组件时同时释放元数据和像素。黑白图加十二像素白色外框，保持主题无关的对比度及留白。图像位于 x=32、y=82，下方文字 y=272，退出恢复普通正文位置；仅步骤变化重新编码，不随时钟刷新反复生成。

`ps_wifi_setup_qr_payload` 不依赖 ESP-IDF/LVGL，限制并转义 Wi-Fi 字段，回归覆盖 UTF-8、分隔符/反斜杠、开放/WPA 表达、输入/输出上限和非法控制字符。`wifi_qr_probe` 仅通过 USB 诊断当前 176 行 I1 图，内容含临时热点密钥，必须私有保存，不向 MiniApp 或网络开放。离线相机解码、字形及回收测试见[实机报告](../../test-reports/wifi-phone-setup.zh_CN.md)。

## 门户与同一手机热点 — OS 1.0.67

手机从加密配网热点获得 DHCP 地址后，第 1 步自动进入第 2 步一次。AP DHCP 将 192.168.4.1 设为 DNS，绑定 AP 的 UDP 服务对 A 查询返回此地址，对 AAAA 返回无记录；HTTP 联网探测重定向至本地页面，响应带非空正文。不拦截 HTTPS，不使用无效的 HTTP DHCP 114 选项。参考 [ESP-IDF 示例](https://github.com/espressif/esp-idf/tree/v5.5.3/examples/protocols/http_server/captive_portal)。手机自动弹页取决于系统，保留地址二维码和手动入口。

同一手机热点需勾选“连接这台手机的热点”，手动填写热点名称、密码，或选择已保存网络。手机连接设备 AP 时不能同时广播自身热点：先持久保存配置，约留出 1.8 秒响应窗口，再关闭 Passport 热点。随后打开手机个人热点，Passport 优先等待该网络，最长五分钟。iPhone 开启“允许其他人加入”，支持的机型开启“最大兼容性”（2.4 GHz；[Apple 说明](https://support.apple.com/en-ca/guide/security/secfd166f620/web)）。以后只需开启已保存热点。此模式禁用附近扫描，因为同一手机尚未广播热点。

普通提交等待 40 秒。实际连接 SSID 必须等于提交目标，获得有效 DHCP 地址且稳定 1.5 秒才算成功；成功提示保留 2.5 秒后自动退出。这里验证局域网连接，不验证公网可达性。回退至其他已保存网络不算成功。取消结束等待并恢复正常选网，不删除保存的凭据。热点等待页 OK 重新配网，双击 OK 取消。

现有有界表单和请求头规则同时适用于 POST `/hotspot`（延后连接新配置）、`/connect-hotspot`（延后连接保存项）。GET `/status` 仅提供状态、错误、延后及保存标志，不返回凭据；`/connect` 保持立即连接保存项。关闭 AP 前停止 UDP 和 HTTP 服务，延后切换期间继续监控完整流程。独立状态机/DNS 测试见 `tests/test_portal.c`，实际发布页面测试见 `tests/test_wifi_setup_page.py`。

系统状态栏已连接时使用 LVGL 内置 Wi-Fi 图标，时间水平居中。设置 → Wi-Fi 显示“已连接”及实际关联的 SSID，不显示尚未连接的保存项或目标项。即使系统语言为英文，网络名称也使用多语言字体；任意 SSID 的显示仍受已安装字形覆盖限制。
