[English](public-distribution.md)

# 公共分发验收

目标：`leiyin321/passportOS`，Pages 项目路径 `/passportOS`。
公共候选：OS **1.0.73**，签名构建 **2026103103**。

Build: PASS，ESP-IDF 5.5.3 完整门禁。应用 **2,423,024 字节**，
距 2.5 MB 余 **76,976 字节**。归档
`build/firmware/3f9bbb4de29f2efc929651db090b479e01d1a1bf79fe11ab83b5c06e16455d39/`，
匹配 ELF SHA-256 `35bec8697bba09ecd015fcd4d2c0dae6eb56315774ab34c176049efd2da83427`。
镜像已包含两个实际公共 Pages 服务地址。

Host tests: PASS，包含 OS 门禁及独立源码快照。站点构建测试覆盖独立 fork 密钥、
签名/哈希拒绝、相对应用 URL、不可覆盖输出、分发包重建及 ZIP 越界拒绝。
安装后的 SDK wheel 已在工程目录之外创建并编译 Native 应用，包含 C 资源。
干净源码字体测试不再依赖托管组件下载，CI 显式安装素材依赖。
导出不包含私有历史、私钥、原始日志及生成固件，保留署名和签名测试样例。

商店精选 Native 小智 1.0.4、蓝牙外设 0.1.4、城市电台 0.1.2。
旧 Lua 小智保持下架，保留 Lua 兼容和示例。源码 PR 无签名/部署秘密。
生产及 fork 公钥可配置，不修改固定的测试公钥。

Device tests: 此检查点的公共分发 NOT RUN。GitHub 部署、公共 HTTPS 产物验证及
实际 Pages OTA/应用下载将在执行后补充。社区首次安装镜像已生成并初始化应用包/
数据分区；备用硬件上的实际覆盖式首次安装仍未验证，当前硬件必须通过 OTA 保留数据。
