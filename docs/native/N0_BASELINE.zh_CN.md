[English](N0_BASELINE.md)

# N0 — Passport OS 基线与 C3 ELF 支持审计

日期：2026-10-26
范围：只读审计及现有 Lua MiniApp 回归测试。本实验没有修改 Passport OS Runtime 或 Lua 源码。

## 结论

**N0 Gate：审计与 Lua 基线通过。** 用户追加授权关闭 monitor 后，已完成联网 Launcher 与 Lua App 真机采样。整片 8 MiB Flash 备份、恢复前后摘要校验均通过，恢复后 Lua 应用启动/退出正常。当前 OS 的 metrics 命令没有 EXEC 字段：OS 的 EXEC 数值根据相同 IDF/config 能力表推导为 0；独立开启内存保护的对照固件直接测得 EXEC free/min/largest 均为 0。不得把推导值描述为直接从已安装 OS 的 metrics 中读取。

## 工具链与目标

| 项目 | 观测值 |
|---|---|
| 开始时分支 | `feature/passport-os` |
| 开始时 HEAD | `6326c2414f3cab844d5257351ff88279fd9ee414` |
| 工作区 | 原本已有大量用户修改；本实验仅放在 `experiments/native_elf_c3/` 与 `docs/native/` |
| ESP-IDF | v5.5.3 |
| 编译器 | `riscv32-esp-elf-gcc 14.2.0 (esp-14.2.0_20251107)` |
| IDF target | `esp32c3` |
| Flash | 硬件实测 ESP32-C3 rev v1.1，内置 8 MiB Flash（XMC） |
| PSRAM | 未配置；Passport 硬件没有 PSRAM |
| Passport 内存保护配置 | `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y`，并锁定 |
| Lua 实现 | `libpassport_lua.a` 归档贡献 102,470 字节（代码 100,262 + rodata 2,208） |

IDF size 报告见 `../experiments/native_elf_c3/logs/passport-idf-size.txt` (`../../experiments/native_elf_c3/logs/passport-idf-size.txt`; 本地诊断，不公开)，组件明细见 `../experiments/native_elf_c3/logs/passport-size-components.txt` (`../../experiments/native_elf_c3/logs/passport-size-components.txt`; 本地诊断，不公开)。

## Flash 分区

Flash 配置为 8 MiB，当前 `partitions.csv`：

| 名称 | 偏移 | 大小 | 用途 |
|---|---:|---:|---|
| nvs | `0x9000` | `0x6000` | 系统设置 |
| phy_init | `0xf000` | `0x1000` | Wi-Fi PHY 校准 |
| otadata | `0x10000` | `0x2000` | OTA 槽选择 |
| ota_0 | `0x20000` | `0x280000` | 固件槽 |
| ota_1 | `0x2a0000` | `0x280000` | 固件槽 |
| packages | `0x520000` | `0x260000` | 已安装应用包 |
| appdata | `0x780000` | `0x80000` | 应用持久数据 |

当前应用镜像 2,078,560 字节（`0x1fb760`），在 2.5 MiB OTA 槽内剩余 543,904 字节。`idf.py size` 的链接段为 Flash Code 1,611,470 字节、rodata 350,820 字节、静态 DRAM 180,406 字节（BSS 64,544 + data 14,084 + DRAM text 101,778）。这些是链接期段，不代表运行时 free heap。

## App Manager 与 Lua 生命周期

- `main/passport_main.c:744` 的 `open_app()` 先关闭旧应用，通过 `ps_store_load()` 加载包、开启 session，再调用 `ps_runtime_start()` 并记录耗时。
- `main/passport_main.c:730` 的 `close_app()` 结束 session；session 持有的清理回调释放包和 runtime。这是 Native 集成必须保留的 Launcher/App Manager 生命周期。
- `os/runtime/runtime.c:603` 的 `ps_runtime_start()` 首先调用 `ps_runtime_stop()`，校验包及显示契约，再创建 Lua/声明式 UI Runtime。
- `os/runtime/runtime.c:773` 的 `ps_runtime_stop()` 调用 Lua `on_stop`、关闭 `lua_State`、停止声明式 UI 并重置应用状态。
- POC 未修改上述代码、包格式、manifest schema 或 Runtime 接口。

## Lua 基线测试

命令：`./tools/passport/test.sh`
结果：**通过**。完整输出保存在 `../experiments/native_elf_c3/logs/lua-baseline-tests.log` (`../../experiments/native_elf_c3/logs/lua-baseline-tests.log`; 本地诊断，不公开)，包含 Runtime API/按键、包及声明式 UI、网络 worker/WebSocket、音频生命周期回收、图标资产编译器/Runtime 等套件。POC 没有更改 Lua 行为。

## Espressif elf_loader v1.3.5 官方源码审计

上游来源：组件元数据记录 Espressif 仓库 commit `4962410c80a51cd5822da1d8d2041eb4285265f1`、路径 `components/elf_loader`、版本 1.3.5。固定版本副本及 Apache-2.0 许可证在 `components/elf_loader/`。

1. **注册表拒绝 C3：** 上游 `idf_component.yml:9-18` 的 target 白名单包括 ESP32、S2/S3、S31、C5/C6/C61、P4，不含 C3。对 `esp32c3` 固定 `==1.3.5` 后，IDF Component Manager 报告没有匹配版本，并列出其他兼容 target。
2. **Kconfig 拒绝 C3：** 上游 `Kconfig:1-14` 只有 target 在同一白名单中时才显示菜单和启用 `ELF_LOADER`，C3 被排除。
3. **架构实现：** `CMakeLists.txt` 按通用 `CONFIG_IDF_TARGET_ARCH_RISCV` 选择 `src/arch/esp_elf_riscv.c`，并非 C6 专用。因此仅凭架构源文件分支不能证明 C3 支持。
4. **重定位：** `src/arch/esp_elf_riscv.c:95-108` 实现 `R_RISCV_NONE`、`R_RISCV_32`、`R_RISCV_RELATIVE`、`R_RISCV_JUMP_SLOT`；其他类型返回 `-EINVAL`。必须审查应用编译/链接产生的重定位。
5. **可执行内存分配：** 上游 `src/esp_elf_adapter.c:64-78` 在非 bus-mirror、无 PSRAM 分支里没有根据 `exec` 区分分配属性，始终使用 `MALLOC_CAP_8BIT`。Passport OS 开启了内存保护。ESP-IDF v5.5.3 C3 `components/heap/port/esp32c3/memory_layout.c:37-40` 在 `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` 开启时，从 C3 heap 能力中移除 `MALLOC_CAP_EXEC`。因此，按现有 Passport 配置，常规 heap API 无法提供此 POC 所需的可执行堆。这是独立于组件 target 白名单的实际集成阻塞。
6. **入口返回值：** `src/esp_elf.c:729-739` 调用 `elf->entry()` 后直接返回 0，丢弃应用入口的整数返回值。Runtime ABI 需定义明确的返回值通道或入口约定，不能假设 `esp_elf_request()` 会返回 App 结果。

### C3 适配缺口

| 缺口 | 证据 | 影响 |
|---|---|---|
| 官方 manifest 与 Kconfig 都排除 C3 | 上游组件 target 白名单 | 不做显式移植时，官方组件无法在 C3 上选用/构建 |
| 当前 Passport 内存保护禁用可执行 heap 能力 | Passport sdkconfig + IDF C3 heap layout | 现有固件配置下不能执行 RAM 模块 |
| 上游非 mirror 分配器忽略 `exec` 参数 | `esp_elf_adapter.c` 分配分支 | 实验分配器必须明确申请可执行内存，并在 C3 真机验证 |
| 支持的重定位类型有限 | RISC-V relocation switch | 通用编译输出不保证可加载 |
| request API 丢弃入口返回值 | request 实现 | 返回值必须由独立 ABI 约定 |
| OS metrics 没有 EXEC 字段 | OS 源码能力表 + 保护配置对照 POC | OS 的 EXEC=0 是配置推导；对照 POC 直接实测为 0 |

实验副本局部修改了 target allowlist、Kconfig、allocator 及重定位错误传递（N1 记录了真机复现的上游问题）。这不是 Espressif 官方支持声明，也没有复制到 Passport OS。

## 真机运行态基线与恢复

下面为 `MALLOC_CAP_8BIT` 的直接日志采样，单位均为字节；Wi-Fi 连接事件已在本地原始日志中确认。

| 状态 | free | minimum | largest |
|---|---:|---:|---:|
| boot | 193,816 | 193,816 | 114,688 |
| Wi-Fi 初始化后 | 96,972 | 96,916 | 86,016 |
| 联网 Launcher 空闲 | 89,724 | 86,136 | 77,824 |
| 小智 Lua 运行 | 46,424 | 24,304 | 15,872 |
| Lua 退出后稳定采样 | 89,636 | 24,304 | 77,824 |
| 恢复原始镜像后 Lua 运行 | 46,396 | 24,288 | 15,872 |
| 恢复原始镜像后 Lua 退出 | 89,548 | 24,288 | 77,824 |

不同启动间的联网后台分配会变化，不能把两次 OS 采样之间的差异当作 Native 泄漏。Native 独立 POC 的每次加载/卸载则直接测得 drift=0，见 N1。

Lua 启动均记录 `ok=1`；应用列表在恢复前后均保留 `org.passport.xiaozhi`，count=1。已安装应用运行测试没有验证小智远端协议或音频问题，也没有人工验看屏幕。原系统代码与 Lua 接口未修改。

已去除网络/设备私密信息的日志：

- Launcher 基线 (`../../experiments/native_elf_c3/logs/os-launcher-baseline.log`; 本地诊断，不公开)
- Lua 基线 (`../../experiments/native_elf_c3/logs/os-lua-baseline.log`; 本地诊断，不公开)
- 原系统恢复后的 Lua 验证 (`../../experiments/native_elf_c3/logs/os-restored.log`; 本地诊断，不公开)
- 备份校验 (`../../experiments/native_elf_c3/logs/backup-verify.log`; 本地诊断，不公开)、整片恢复校验 (`../../experiments/native_elf_c3/logs/restore-verify.log`; 本地诊断，不公开)

Flash 备份仅保存在被忽略的 `local/native_elf_c3/passport-verified-backup.bin`，未提交。首次读取后系统重启写入配置，导致第一次比较不匹配；随后保持 bootloader 状态，补读系统数据/appdata 并重新拼成快照，整片设备摘要校验通过。恢复后再次在启动 OS 前验证整片摘要匹配。原始私密启动/网络日志也只留在本地。

后续[受保护内存池实验](N1_PROTECTED_POOL.zh_CN.md)通过有界预留区，解决了开启保护/锁定时 Loader 的分配问题，没有更改通用 EXEC heap 能力。
