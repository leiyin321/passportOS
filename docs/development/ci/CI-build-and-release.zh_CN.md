<p align="right"><strong>简体中文</strong> · <a href="CI-build-and-release.md">English</a></p>

# Passport OS 构建及发布

`.github/workflows/build-firmware.yml` 为手动触发，使用 ESP-IDF 5.5.3 构建并验证
ESP32-C3 固件，上传审核产物，不签名、不创建 GitHub Release。
`version.txt` 提供默认 OS 版本，开发时可通过有效的 `PASSPORT_OS_VERSION` 覆盖。

OS 标签使用 `v<version>`，如 `v1.0.75`。应用有独立 PAP 版本，不使用不同 OS 固件标签。
维护者执行完整门禁、验证归档、编译测试精选应用，在可信本机签名并创建分发包。
参阅[分发命令](../../../distribution/README.zh_CN.md)。

`.github/workflows/pages-distribution.yml` 为独立手动流程：下载已审核 Release 的
`passport-distribution.zip`，以仓库分发公钥验证固件和应用，保留已验证历史文件并生成静态站点。
部署还需要 deploy 输入、发布变量及 Pages 环境的 `main` 限制。CI 不持有签名私钥。
第三方 Action 固定完整提交 SHA，更新前须核对官方仓库；PR 检查不部署、不签名。

## 首次安装及升级

`partitions.csv` 为 OS 产品的双 2.5 MiB 槽、应用包和独立数据分区布局。
当前应用从 `0x20000` 开始，不能套用硬件 demo 的应用偏移。
普通合并构建产物不是已初始化的社区完整镜像，应从验证归档运行
`tools/passport/community_image.py` 生成 8 MiB 首次安装文件。
仅首次安装或明确选择完全重置时从 `0x0` 刷入，它会替换设置/数据。
已有 OS 使用设置 OTA，只写非活动应用槽；OTA 应用 `.bin` 不能写到 `0x0`。

发布需包含版本、目标硬件、校验文件、首次安装覆盖提示及玩家操作，参阅
[社区草稿](../../../distribution/community-post.zh_CN.md)及[验收报告](../../test-reports/public-distribution.zh_CN.md)。
源码构建或上传成功不能替代硬件刷写/首次启动实测。
