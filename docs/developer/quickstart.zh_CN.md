[English](quickstart.md)

# 开发者快速开始

SDK 正在真机验证中，阶段状态见[任务](../tasks.md)，不能将未验收能力当成完成。
需要 Python 3.10+、OpenSSL 命令行、宿主 C 编译器；构建固件需要 ESP-IDF 5.5.3。
已缓存离线应用运行无需账号、开发者云后端或 PC 常驻进程。

```sh
python3 -m venv .venv
.venv/bin/pip install -e ./sdk
passport --help
passport validate examples/packages/counter.pap --public sdk/publisher-public.pem
```

每个应用目录包含 `manifest.json` 和 `main.lua` 或 `ui.json`。复制示例，使用自己的反向域名 ID，
只声明需要的能力，格式见[包协议](package-protocol.zh_CN.md)。更新应用时保持发布者密钥不变，数据身份包含公钥。

```sh
mkdir -p local dist
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out local/my-private.pem
openssl pkey -in local/my-private.pem -pubout -out local/my-public.pem
passport pack examples/counter --key local/my-private.pem --public local/my-public.pem -o dist/my-counter.pap
passport validate dist/my-counter.pap --public local/my-public.pem
```

设备内置 `sdk/publisher-public.pem`，并支持最多4个额外RSA2048发布者。
执行 `passport trust --public local/my-public.pem`，核对设备显示的密钥ID并按OK确认，无需重刷OS；拒绝时不注册。
已信任发布者仍不能冒领其他发布者的ID，应使用 `passport init local/my-app --id org.yourname.app` 创建新应用。
内置私钥不入库，已签名示例不需要私钥即可使用。禁止发布私钥或Wi-Fi配置。
图像开发详见[资源指南](assets.zh_CN.md)。

```sh
passport wifi --ssid '你的2.4GHz网络'  # USB 配网，密码隐藏输入
passport install examples/packages/counter.pap --public sdk/publisher-public.pem
passport device list
passport device launch org.passport.counter
passport device view
passport device home
passport device offload org.passport.counter
# 重装同发布者同 ID 的包：保留存档。
passport device remove org.passport.counter  # 必须输入 DELETE 确认
```

也可在设备上打开 **Settings** → OK，手机连接屏幕显示的 `Passport-XXXX` WPA2 热点并输入本次密钥，
再访问 `http://192.168.4.1`，填写目标 2.4 GHz 网络名称和密码。本地页面会保存到 NVS；设备获取 Wi-Fi IP 后
自动关闭临时热点。详见[显示与手机配网规范](display-layout.zh_CN.md)。

USB 是可信物理开发/管理通道，不是小程序能力。二进制安装必须遵循 READY → 128 字节分块 →
ACK 偏移 → 最终验证结果，不可盲传二进制。按键及 worker 通过有界队列通信。

静态商店发布流程，不要求离线或声明式应用拥有业务后端：

```sh
passport publish-index dist --public local/my-public.pem --base-url https://你的主机/apps -o dist/index.json
passport serve dist --host 0.0.0.0 --port 8040
passport device service 192.168.1.10 8040 8041
```

在设备上确认所显示的 Mac IP/HTTP/WS 端口；进入商店按 OK 获取目录并选择应用，商店与 Apps 库卡片都会显示版本号。按 OK 检查已安装版本：未安装或有更新时才下载/更新；已是最新版本时提示“已是最新版本。按 OK 打开应用。”，再次按 OK 启动。更新安装完成后提示“安装完成。按 OK 打开应用。”，按 OK 会直接启动，不会重复下载。Apps 库中按 OK 打开应用操作，可设为/取消默认应用；系统同时只保存一个默认应用，开机自动启动，双击 OK 返回主界面。单击 OK 确认，长按 OK 留给应用处理。卸载或 Offload 默认应用时会清除默认项。
本地授权限于当前配置，不可用于扫描内网。开发默认端口 8040 为目录、8041 为 echo。
商店同时校验包 SHA256 与发布者签名。`passport serve` 默认只绑定本机；通过
`--host 0.0.0.0` 显式向开发局域网开放所选目录。只服务公开应用产物，不能把密钥或私有文件放在
目录中。此简易服务没有身份验证，仅用于可信开发网络。

```sh
.venv/bin/pip install -r server/dev/requirements.txt
.venv/bin/python server/dev/echo.py --port 8041
./simulator/build.sh
python3 simulator/serve.py --port 8050
# 浏览器打开 http://127.0.0.1:8050；方向键和 Enter 映射设备三键。
python3 tests/passport/test_packages.py
python3 tests/passport/test_runtime.py
```

模拟器运行同一套 C 运行时与 Lua 引擎，但指针尺寸、时钟、字体/渲染、存储及模拟网络/音频与真机不同。
模拟器 PASS 不等于真机 PASS。网络测试使用项目自带的独立 echo 服务；自有 WSS 服务可传入 `--cert`/`--key`。
不依赖第三方公开 echo 网站。

明确授权目标硬件后可执行 `python tests/passport/hardware.py --suite storage`，也支持 apps/store/stress/reconnect。
测试工具不刷写。私有日志在忽略的 `local/`，不要公开。固件刷写须遵循发布恢复指南，绝不可把 app.bin 写入 0x0。

应用库按OK进入操作页：打开、清包、离线Pin、确认删除。生成多发布者目录时，对每个发布者重复传入一次 `--public`；每个应用包独立验签，目录条目和 HTTPS 地址本身不代表发布者已受信。公共静态目录可通过 `passport device catalog https://你的主机/apps/index.json` 设置；仅该HTTPS主机可作为系统商店端点，重定向逐跳重新授权。

完整命令见 [SDK/CLI 参考](sdk-cli.zh_CN.md)，`ui.json` 的能力和限制见[声明式 UI/1 规范](declarative-ui.zh_CN.md)。

## Native ELF

固件 1.0.34 起可通过 `passport init DIR --id org.example.native --runtime native-elf` 创建项目，使用 `passport native build DIR` 编译、`passport native inspect DIR/app.app.elf` 检查。签名、安装与启动沿用现有工具；原生应用只允许内置 OS 发布者。完整 ABI、限制和示例见[原生应用开发](native-apps.zh_CN.md)。

商店和应用库卡片同时显示版本及 PAP 应用包大小，单位 KiB（1 KiB = 1,024 字节）。商店大小来自索引，应用库大小来自实际缓存文件；缺失或卸载缓存时显示 `-- KiB`。此数值不包含应用持久数据和文件系统开销。设置 → 关于显示当前运行 OS 应用镜像的字节数，不含 OTA 备用槽和其他分区。

设置 → 存储空间提供整片 Flash 占用条（蓝色：当前 OS 镜像，紫色：OTA 预留，橙色：应用及缓存，绿色：数据预留，银色：系统及开销，灰色：应用可用）、OS/OTA 预留概况、应用包分类、持久 NVS 项及逐应用详情。上下键循环翻页，OK 后台刷新。逐应用数据占用按有效 NVS 项数 × 32 字节估算，不含页/命名空间和垃圾回收开销，不代表数据内容长度；应用包文件大小不含文件系统开销。应用可用空间来自 SPIFFS，不包含未使用的 OTA 空间。刷新为只读操作，不移除应用包或数据。

总览占用条按实测物理 Flash 容量和实际分区表计算，不以 SPIFFS 容量为分母。OTA 预留包含所有应用槽中当前运行镜像之外的字节，包括当前槽尾部填充。数据预留表示整个应用数据分区，不表示其中所有字节均已有有效数据。系统及开销包含启动/控制/设置区域、对齐间隙和 SPIFFS 不对文件开放的容量。六类字节数之和精确等于整片 Flash；界面 KiB 分别向下取整。只有灰色部分可用于应用文件，当前/历史应用包分类仍在下一页。

占用条和下方分类按上述顺序从左到右对应，应用可用空间位于最右侧及分类末行。每行均有同色标记，标记行距跟随当前语言字体的行高。

Native ELF 是主要应用方向，`passport init` 默认创建 Native；上文 Lua 工作流需显式指定 `--runtime lua`。参阅[公共及 fork 分发指南](../../distribution/README.zh_CN.md)。
