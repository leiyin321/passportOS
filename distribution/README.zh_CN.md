[English](README.md)

# 公共及本地分发

单仓库 `leiyin321/passportOS` 包含 OS、SDK、应用源码和静态发布工具。
公共地址为 `https://leiyin321.github.io/passportOS/updates/latest.json` 和
`https://leiyin321.github.io/passportOS/apps/index.json`。Pages 无需动态后端，
各应用的动态服务由其开发者维护。

## 首次安装和恢复

社区审核后的**首次安装**镜像从 `0x0` 刷入，目标 ESP32-C3、8 MiB Flash。
完整镜像初始化应用包和应用数据分区，并替换已有固件、Wi-Fi、设置和应用。
已有 OS 且需保留数据时不要使用首次安装镜像，应从设置 OTA 升级。
Wi-Fi/OTA 失败时仍可通过 USB 恢复。镜像不含 Wi-Fi 凭据、预装应用或签名私钥。

仅从已验证归档构建社区镜像：

```sh
python tools/passport/community_image.py build/firmware/ARCHIVE_SHA build/PassportOS-first-install.bin
```

记录 SHA-256，发布时附版本、硬件目标、首次安装覆盖数据提示、三键说明、
手机配网和公共服务地址。不能将合并首次安装镜像当作 OTA 应用文件。
OTA 签名只覆盖应用镜像，不覆盖社区镜像的全部字节；完整镜像还需维护者审核及公布哈希。

## 玩家操作及首次联网

上下键移动，短按 OK 确认，双击 OK 返回主页。进入设置 → Wi-Fi，按 OK 开始手机配网，
扫描设备 Wi-Fi 二维码。手机接入后打开本地配网页，选择附近 2.4 GHz 网络或手动输入；
Passport 验证连接成功后自动结束。配置同一手机的热点时，先勾选热点模式并保存名称/密码，
等 Passport 关闭配网热点后再开启手机热点。支持的 iPhone 开启“允许其他人加入”和“最大兼容性”。

设置 → 语言可选中英。打开应用商店按 OK 浏览/安装，安装完成或已是最新版时 OK 打开应用。
设置 → 系统更新检查公共签名发布；有新版本时显示字节及百分比进度，验证完成后才允许重启。
首次联网也会启动公共 NTP 同步。手机配网独立于 Pages 服务。

## 本地构建公共站点

```sh
python -m pip install ./sdk
# 签名私钥仅在本机，不能进入仓库或 CI 构建。
passport firmware-release build/FoloToy-AI-Passport.bin --version VERSION --build MONOTONIC_BUILD --key /private/release.pem --public distribution/release-public.pem -o build/updates
python tools/passport/pages.py build --firmware build/updates --packages dist --selection distribution/apps.json --public distribution/release-public.pem --output build/pages
python tools/passport/pages.py bundle build/pages build/passport-distribution.zip
```

产物包括不可覆盖的版本文件、`updates/latest.json` 和 `apps/index.json`。
构建器验证签名/哈希，生成适用于项目子路径的相对应用 URL，只复制明确选定的文件，
拒绝覆盖已有输出目录。整体原子部署，不能先发布索引再上传文件。
不会复制私钥和无关文件。当前商店精选 Native 小智、城市电台及蓝牙外设，
不选入旧 Lua 小智；Lua 运行时和示例继续保留。

## 本地二次开发及 fork

```sh
passport keygen --private local/fork-private.pem --public local/fork-public.pem
PASSPORT_RELEASE_PUBLIC_KEY="$PWD/local/fork-public.pem" PASSPORT_DISTRIBUTION_URL='' PASSPORT_OS_VERSION=1.0.0 ./tools/validate.sh
passport init local/my-app --id org.yourname.app
passport native build local/my-app
passport pack local/my-app --key local/fork-private.pem --public local/fork-public.pem -o local/my-app.pap
```

同一 fork 的应用更新应保持稳定密钥。改变信任根需通过 USB 首次安装该 fork OS；
仅修改服务 URL，不会使公共 OS 接受 fork 签名固件或 Native 应用。
公开 fork 时替换 `distribution/release-public.pem` 中的**公钥**，保留 SDK 测试公钥不变，修改 `distribution/config.json` 和精选清单，复用同一流程。
安装版 SDK 包含 C 头文件、支持代码及模板；RISC-V 编译器需激活 ESP-IDF。

分别在终端运行本地静态服务：

```sh
passport serve build/pages/apps --host 0.0.0.0 --port 8040
passport serve build/pages/updates --host 0.0.0.0 --port 8765
passport device service LAN_IP 8040 8041 --port SERIAL_PORT
passport device update-source http://LAN_IP:8765/latest.json --port SERIAL_PORT
```

设备上按 OK 授权本地服务。使用局域网私有 IPv4，公共明文 HTTP 会被拒绝。
本地商店授权仅当前会话有效，重启后需重复；更新源会持久保存。
`passport device catalog HTTPS_URL` 设置持久 HTTPS 商店覆盖地址。
默认地址来自 `distribution/config.json`，可用 `PASSPORT_DISTRIBUTION_URL` 覆盖，
空值表示无公共默认地址；有效保存项优先。切换服务不会改变签名校验。
`passport init --runtime lua` 显式创建 Lua 应用。

## GitHub 协作和发布

1. fork/分支提交**源码** PR，附测试、权限、最低 API/ABI、许可、隐私/后端及实机验收，不提交密钥。
2. 维护者审核 Native 代码，构建测试 OS/应用，在可信本机签名，更新 `distribution/apps.json`。PR 任务无签名密钥。
3. 创建审核后的 GitHub Release，附 `passport-distribution.zip`。版本和构建号递增，不覆盖已有版本。
4. Pages 选择 GitHub Actions，设置仓库变量 `PASSPORT_PUBLISH_ENABLED=true`，并将 `PASSPORT_DISTRIBUTION_URL` 设为 HTTPS Pages 基地址，根据账户能力为 `github-pages` 环境配置 `main` 分支限制及审核者。
5. 手动运行 **Verify and optionally deploy Passport distribution**，选择已审核 Release。工作流用仓库公钥重新构建验证站点，审核后勾选部署；普通 push/PR 不部署。
6. 验证公共 HTTPS 清单、所有大小/哈希、真实设备更新、已是最新版提示、商店及真实应用下载安装。

固件构建工作流为手动触发，仅上传待审核产物，不签名或发布；`version.txt` 提供默认 OS 版本。
已禁用自动合并上游；手动同步还需 `PASSPORT_UPSTREAM_SYNC=true`。
Pages 任务没有私钥。保留旧签名版本供恢复，但回退发布仍须遵守设备的版本/构建号递增规则。

[GitHub Pages 工作流要求](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
包含部署权限及 Pages 环境。遵守 [Pages 限制](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits)：
站点最大 1 GB，月流量软限制 100 GB；保留有限版本，大型归档及调试文件放到适当发布存储。

## 当前边界

Native 是受信代码，可能影响 OS 稳定性，ABI 不提供进程隔离。
暂未实现固件签名密钥轮换/吊销，也没有超出现有版本/构建号规则的防回退机制。
Pages 与各应用后端的可用性独立；不承诺类似商业 App Store 的沙箱或托管 AI 后端。

部署前下载并验证当前公共产物，保留前两个签名 OS 版本及仍上架应用的上一包版本；下架 ID 不保留。相同版本不能替换成不同内容，公共 OS 构建号不能递减。历史验证失败则停止部署。CDN 传播/缓存及公网速度会影响更新可见性，应等待传播后重试，而不是覆盖版本文件。

社区发布正文：[可审核草稿](community-post.zh_CN.md)，发帖为独立的维护者操作。

固件 CI 也会将所有 Native 示例编译为无签名 C3 ELF，并核对精选源码的 ID/版本；签名仍为可信维护者的独立步骤。
