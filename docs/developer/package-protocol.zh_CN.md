[English](package-protocol.md)

# Passport 应用包协议 1

状态：V1 协议已实现；硬件覆盖和剩余验收见[任务](../tasks.zh_CN.md)及测试报告。
宿主 `sdk/passport` 与设备 `os/package/package.c` 共用此格式。
整数均为无符号小端，不压缩；原生 ELF 使用下述受限配置。

| 偏移 | 字节数 | 含义 |
|---|---:|---|
| 0 | 4 | ASCII `PAP1` |
| 4 | 4 | Manifest UTF-8 长度，1..4096 |
| 8 | 4 | 入口 UTF-8 长度，1..16384 |
| 12 | 4 | 签名长度，固定 256 |
| 16 | Manifest 长度 | JSON Manifest |
| 随后 | 载荷长度 | 单一文本入口，禁止内嵌 NUL |
| 末尾 | 256 | RSA-2048 PKCS#1 v1.5 / SHA-256，对此前全部字节签名 |

总长度必须精确一致，拒绝尾部多余字节、截断和超限字段。
规范输出：JSON 键排序、无冗余空白、ASCII 转义；同键同内容可重复生成同包。
TLS 不代替包身份。`publisher_key_id` 为公钥 SPKI DER 的 SHA-256 前 16 个小写十六进制字符，
`sha256` 为入口内容完整 SHA-256。固件只接受已配置的可信公钥。
固件内置一个发布者，并支持经可信USB/设备确认注册最多4个额外RSA2048发布者。私钥禁止入库；换键必须明确处理。

源码 Manifest 规范见[`manifest-v1.schema.json`](../../protocol/manifest-v1.schema.json)。必填：`format:passport.app/1`、反向域名 `id`（至多 63 UTF-8 字节）、`name`、数字三段 `version`、
`publisher_key_id`、`runtime:lua|declarative|native-elf`、`entry:main.lua|ui.json`、`min_os_api:1`、
`data_schema:1`、`permissions`、`limits`、`sha256`。
拒绝重复键、越权权限和不支持的 API/schema。V1 只接受 schema 1，避免未经验证的迁移破坏旧数据。
应用版本严格使用数字格式 `MAJOR.MINOR.PATCH`。商店卡片显示版本号，并与已安装包进行数字比较。
未安装或版本较旧时下载；版本相同或已更新时视为最新，再按 OK 打开应用。
限额：脚本堆 1..64KiB（默认 32KiB）、FPS 1..20、持久数据 1..4KiB。
权限：`ui`、`graphics`、`input`、`timer`、`storage.private`、`network.fetch`、
`network.websocket`、`audio.features`。Manifest 不可获得系统权限。

固件版本、Capability API、PAP 格式、应用版本、数据 schema 分别版本化。
修改二进制头语义须升级 magic/格式；可增加可选字段；提升 API/schema 需要对应固件支持。
禁止字节码和 ZIP；原生 ELF 仅允许使用下述配置。

身份由可信发布者公钥哈希和 App ID 共同确定。代码缓存与数据分区物理分离。
Offload 必须删除现用及历史代码，保留身份、入口和数据；彻底删除须系统明确操作。
更新先写 `.new` 并验证，保留 `.old` 后切换；现用包损坏可回退上个已验证版本。
SPIFFS 掉电一致性仍需真机故障注入，不能仅凭 rename 推断原子耐久性。

PAP2保留签名封装并兼容PAP1；入口长度前缀、资源索引、PAI1及流式验证见[资源协议](assets.zh_CN.md)。静态商店目录格式见[`catalog-v1.schema.json`](../../protocol/catalog-v1.schema.json)，当前设备最多接受 8 个条目。目录只用于发现应用，不能替代应用包签名或设备上的发布者确认。

## Native ELF 配置（固件 1.0.34 起）

PAP1/PAP2 可携带至多 32 KiB 的二进制 ELF 入口；UTF-8、NUL 限制和 16 KiB 上限仅适用于 Lua/声明式入口。原生 Manifest 必须指定 `entry:app.app.elf`、`target:esp32c3`、`abi:1`、`trusted:true`，使用 `limits.native_exec_kib`（1–16）替代 `script_heap_kib`。只接受 OS 内置发布者签名，权限限于 UI、图形、输入和计时器。ELF 校验规则、ABI、工具及信任边界见[原生开发](native-apps.zh_CN.md)。

Native ABI 2 从 OS 1.0.35 起提供通用网络、身份、存储及音频服务。Manifest 使用 `abi:2`，按权限和域名声明能力；ABI 1 保持原限制。详见 [ABI 2 服务](native-services.zh_CN.md)及[原生小智](xiaozhi-native.zh_CN.md)。

Native OS 1.0.51 新增 `audio.playback` 权限，仅限可信 Native ABI 2 应用；结合 `network.fetch` 使用可选 HTTPS MP3 媒体扩展，不需要麦克风授权。必须显式声明台源及重定向域名。参见[媒体所有者、API 与限制](city-radio.zh_CN.md)。
