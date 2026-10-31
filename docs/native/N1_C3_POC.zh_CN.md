[English](N1_C3_POC.md)

# N1 — ESP32-C3 RAM ELF Loader POC

日期：2026-10-26

**最新补充：**[锁定保护的内存池](N1_PROTECTED_POOL.zh_CN.md)已在开启保护和锁定时通过真机测试。下文保留原先仅使用 heap 的对照结果。默认 `build_poc.sh` 现已改为构建受保护内存池。

**N1 Gate：独立、关闭内存保护的实验通过。** 已在真实 ESP32-C3 上验证加载、调用、返回、卸载、再次加载、静态字符串/BSS、多文件调用、支持的重定位与不支持重定位的拒绝。原始 8 MiB Passport 镜像已恢复并校验，Lua 启动/退出正常。**当前 Passport OS 集成仍受启用的内存保护策略限制。未开始 N2–N6。**

上游：Espressif `elf_loader` v1.3.5，仓库 commit `4962410c80a51cd5822da1d8d2041eb4285265f1`，Apache-2.0。实现基线 commit：`cf6d487`；后续真机验证/修复提交包含本次更新报告。硬件：ESP32-C3 rev v1.1、内置 8 MiB Flash、无 PSRAM。工具链：ESP-IDF v5.5.3、RISC-V GCC 14.2.0。

[固定版本上游源码与许可证](https://github.com/espressif/esp-iot-solution/tree/4962410c80a51cd5822da1d8d2041eb4285265f1/components/elf_loader)

## 独立实现与本地补丁

工程位于 `experiments/native_elf_c3/`。没有编辑 Passport OS 的 App Manager、Lua Runtime、包协议、分区或 SDK。按照现有仓库规范，将该上游文档目录明确登记为 vendored；敏感内容检查仍有效。固定版本上游副本有以下实验补丁：

1. 本地组件 manifest 与 Kconfig target 条件加入 C3，以编译通用 RISC-V 实现。
2. 在非 mirror allocator 分支中，代码明确申请 `MALLOC_CAP_EXEC`。上游该分支即使 `exec=true` 仍使用 `MALLOC_CAP_8BIT`。没有猜测 C6 内存地址；实际可执行地址来自 IDF 的 C3 分配器。
3. 仅独立 POC 配置使用 `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n`，受保护的对照构建保留 `y`。没有更改 Passport OS 的安全配置。
4. 在 `src/esp_elf.c` 中传递 `esp_elf_arch_relocate()` 的返回值。上游调用者忽略错误：真机打印“info=2 is not supported”后，`esp_elf_relocate()` 仍返回 0。本地修复返回错误，调用者释放部分加载的映像，且不调用入口。

官方 `elf_loader.cmake` 使用带独立入口的 freestanding PIC/shared 链接。本实验沿用此模型，明确设置 C3 ISA/ABI 与 `--no-relax`，没有把完整固件 ELF 当作模块。当前范围限于 linter 检查的 PT_LOAD 零基址布局；不代表支持任意 ELF 布局、C++ 构造函数/TLS 或所有重定位类型。

## 构建与复现

```sh
source "$IDF_PATH/export.sh"
./experiments/native_elf_c3/build_legacy_poc.sh

IDF_TARGET=esp32c3 idf.py -C experiments/native_elf_c3 \
  -B experiments/native_elf_c3/build-protected \
  -D SDKCONFIG=sdkconfig.protected \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.protected.defaults' build
```

正常 POC：`build/native_elf_c3_poc.bin`，**167,488 字节**。内存保护对照：`build-protected/native_elf_c3_poc.bin`，**172,864 字节**。模块：`module/native.app.elf`，**1,960 字节**。路径均相对于实验目录。实际测试的哈希保存在 device-artifacts.json (`../../experiments/native_elf_c3/logs/device-artifacts.json`; 本地诊断，不公开)。

正常 POC 使用 `0x10000` 的独立 factory 分区。首次写入替换 bootloader `0x0`、分区表 `0x8000` 与应用 `0x10000`；后续 POC 变体只替换该测试应用。这些地址与原 Passport OTA 布局不兼容。测试前已完整备份并校验 Flash，测试后完整恢复。没有明确的备份/恢复流程时，不要向 Passport 刷入 POC。

在确认的设备串口上复现：

1. 关闭已授权的 monitor。读取全部 `0x800000` 字节并保持 bootloader 状态（`esptool.py --after no_reset read_flash ...`），不启动 OS，随后用 `verify_flash 0x0 BACKUP` 校验。备份保密存放。
2. 按上述已核验地址写入 POC，使用 `capture_serial.py PORT LOCAL_LOG --seconds 10` 限时采集启动/运行日志。
3. 分别运行正常 POC 与开启内存保护的对照固件。
4. 以 flash mode/frequency/size 均为 `keep`，将 `BACKUP` 写回 `0x0`；保持 bootloader 状态，再次校验整片摘要。
5. 启动恢复的 OS，执行 `home`、`list`、`launch org.passport.xiaozhi`、`metrics`、`home`、`metrics`。结束采集并释放串口。

## ELF 检查

readelf (`../../experiments/native_elf_c3/logs/module-readelf.txt`; 本地诊断，不公开)、program headers (`../../experiments/native_elf_c3/logs/module-program-headers.txt`; 本地诊断，不公开)、objdump (`../../experiments/native_elf_c3/logs/module-objdump.txt`; 本地诊断，不公开)、linter 结果 (`../../experiments/native_elf_c3/logs/module-lint.log`; 本地诊断，不公开)：

- ELF32、小端、RISC-V `ET_DYN`，C3 `rv32imc_zicsr_zifencei` / `ilp32`；入口 `0x118`；没有未解析导入。
- `.text` 82、`.rodata` 4、`.data` 4、`.bss` 4 字节。PT_LOAD 跨度 `0x11ec` = **4,588 字节**，包含 ELF 元数据及段间填充。
- 一条 `R_RISCV_RELATIVE` 修正指向另一编译单元 `helper_add()` 的间接函数指针。反汇编确认静态字符串由真实 `lbu` 指令读取，没有被编译期折叠掉。
- BSS 状态递增，入口依次返回 42、43。也调用了 `esp_elf_request()`，但该 API 只返回 Loader 状态；整数结果由直接调用 `elf.entry` 验证。
- 负面测试复制受信任测试模块，将重定位类型改为不支持的 `R_RISCV_64`，预期返回 `-EINVAL`，不调用被拒绝的入口。

## 实际可执行内存测量

独立 POC 的 ESP32-C3 未初始化显示、Wi-Fi、音频。下表单位均为字节。此处 EXEC 和 8BIT 指向重叠的物理堆，**不能将两者相加**。

| 正常 POC 阶段，cycle 0 和 1 | EXEC free | EXEC minimum | EXEC largest | 8BIT free | 8BIT minimum | 8BIT largest |
|---|---:|---:|---:|---:|---:|---:|
| 首次加载前 | 328,708 | 328,708 | 188,416 | 328,708 | 328,708 | 188,416 |
| 已加载 | 324,096 | 324,096 | 188,416 | 324,096 | 324,096 | 188,416 |
| 卸载后 | 328,708 | 324,096 | 188,416 | 328,708 | 324,096 | 188,416 |
| 第二次加载/卸载后 | 328,708 | 324,096 | 188,416 | 328,708 | 324,096 | 188,416 |

成功加载实测堆增量：**4,612 字节**，包含分配器开销；PT_LOAD 有效跨度 4,588 字节。模块代码没有堆分配。每次加载/卸载后，EXEC 与 8BIT 的 free 都恢复到该次加载前，**drift=0**。负面测试额外使用的临时 ELF 副本不包含在成功加载峰值中；其最终 free 漂移也为 0。

cycle 0 实际段基址为 `0x403905a4`，入口 `0x403906bc`。RX 段：地址 `0x403905a4`，filesz/memsz 368/368。RW 段：地址 `0x40391714`，filesz/memsz 120/124。地址来自 C3 分配器，并由真机日志记录。

内存保护对照（`CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y`）：EXEC free/min/largest **0/0/0**；8BIT free **328,692**。加载返回 **-12（`-ENOMEM`）**，入口和段指针均为空，没有消耗堆。这直接验证了当前策略的阻塞，C3 编译通过本身不能解决它。

## 测试证据

| 测试 | 结果与证据 |
|---|---|
| 现有 Lua/Passport 主机测试 | 真机实验前后均通过；最终完整套件输出 (`../../experiments/native_elf_c3/logs/host-regression.log`; 本地诊断，不公开) |
| 仓库检查器单元测试 | 通过，22 项；输出 (`../../experiments/native_elf_c3/logs/repo-checker-tests.log`; 本地诊断，不公开) |
| 仓库完整 static gate | 失败：原有中文输入计划 `docs/PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.md` 缺少中文配对文件，默认路径也不是英文；输出 (`../../experiments/native_elf_c3/logs/static-gate.log`; 本地诊断，不公开)。本次保留了用户提供的计划文件。 |
| 正常/受保护 POC 固件构建 | 通过；最终正常构建 (`../../experiments/native_elf_c3/logs/poc-device-build.log`; 本地诊断，不公开)、受保护构建 (`../../experiments/native_elf_c3/logs/poc-protected-build.log`; 本地诊断，不公开)；初始 clean build 日志仍保存在 `poc-build.log` |
| 初始计算/BSS 加载及再次加载 | 通过；真机日志 (`../../experiments/native_elf_c3/logs/poc-basic-device.log`; 本地诊断，不公开) |
| 字符串、局部变量、BSS、多文件间接调用、相对重定位 | 通过；最终完整真机日志 (`../../experiments/native_elf_c3/logs/poc-device.log`; 本地诊断，不公开) |
| 上游不支持重定位的行为 | 已复现失败：打印错误但 rc=0；修复前真机日志 (`../../experiments/native_elf_c3/logs/unsupported-before-fix.log`; 本地诊断，不公开) |
| 本地修复后拒绝不支持重定位 | 通过：rc=-22，不调用入口，清理后 drift=0；见最终真机日志 |
| 开启内存保护 | 已确认预期阻塞；完整真机日志 (`../../experiments/native_elf_c3/logs/poc-protected-device.log`; 本地诊断，不公开) |
| 原始整片 Flash 恢复 | 通过；设备摘要校验 (`../../experiments/native_elf_c3/logs/restore-verify.log`; 本地诊断，不公开) |
| 恢复原 OS 后 Lua App | 串口启动/退出通过，安装列表保留；脱敏 OS 日志 (`../../experiments/native_elf_c3/logs/os-restored.log`; 本地诊断，不公开) |
| 100/1000 次压力测试、Launcher Native 集成 | 未运行；属于 N3/N4，不在本次范围 |

私密原始 OS/网络日志及 Flash 数据只保存在被忽略的 `local/`；提交的 OS 日志仅保留生命周期/metrics。POC 日志只移除了设备标识，其余完整保留。初次串口被占用的日志作为历史记录保留，不代表最终状态。当前没有遗留的串口 monitor。

## 下一阶段建议

C3 RAM ELF 在本次 `memprot=n` 配置下确实可运行。官方 manifest/Kconfig 的排除已确认；本实验不能替 Espressif 推断其未说明的排除理由。当前 Passport 的 `memprot=y` 仍不支持常规可执行堆分配。

在实现 N2 前，先决定如何保留或明确改变该保护策略。研究预留可执行区域及 C3 内存保护配置能否在不全局关闭保护的前提下完成“写入后执行”；本阶段尚未证明此方案可行。如果必须降低保护，应保留为明确的 trusted/developer-only 固件变体并记录权衡，稳定 Lua OS 继续作为回退方案。

完成该决策后，再进入小型版本化 C 函数表 ABI，之后分别开展 N3 生命周期/压力验证及 N4 App Manager 集成。本阶段没有新增 Native ABI、App Store/包集成、OS 服务导出、小智 Native 实现或 XIP。POC 通过不代表具有进程隔离或沙箱。
