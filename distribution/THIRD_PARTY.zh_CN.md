[English](THIRD_PARTY.md)

# 署名及再分发

保留根目录 MIT 许可及 FoloToy 版权。Passport OS 是衍生项目，社区发布应分别说明
OS 及其作者和硬件制造商，不能暗示厂家背书。

- ESP-IDF 5.5.3：Espressif，Apache-2.0，从官方源获取。
- LVGL 及托管组件：保留依赖锁和注册表提供的许可；生成的 managed 文件不进入源码导出。
- ELF Loader：Espressif Apache-2.0，保留 [`license.txt`](../components/elf_loader/license.txt)，C3 修改在仓库内。
- Lua：[上游许可](../components/passport_lua/LICENSE.html)。
- MP3 解码器：[组件许可](../components/passport_mp3/LICENSE)。
- 字体：[素材来源](../assets/README.zh_CN.md)，包含 Source Han OFL 和 FontAwesome 许可，生成字体保留来源说明。
- 模拟器 cJSON 保留其[许可](../simulator/vendor/cJSON/LICENSE)。

应用开发者负责素材、音频流权限、第三方 AI 服务及后端条款，贡献不得夹带固件秘密、
私有服务凭据或专有素材。公开分发应用不代表取得外部内容提供方的授权。
