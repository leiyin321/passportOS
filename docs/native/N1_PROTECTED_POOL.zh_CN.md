[English](N1_PROTECTED_POOL.md)

# N1 补充 — C3 内存保护锁定时加载 ELF

日期：2026-10-26。ESP-IDF v5.5.3 / ESP32-C3 rev v1.1 / 8 MiB Flash / 无 PSRAM。

**真机通过：**同时开启 `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y` 和 `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE_LOCK=y` 时，ELF 已能加载、调用和卸载。通用 EXEC 堆刻意保持为 0，Loader 改用独立静态预留的 16 KiB 可执行区域，没有将普通堆改为可执行。原 Passport Flash 已恢复并校验，Lua 启动/退出正常。本次仍是独立 N1 实验，没有实施 N2–N6 集成。

## 保护策略与实现

IDF C3 的[默认内存保护实现](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_hw_support/port/esp32c3/esp_memprot.c) 使用主 I/D 分界线和两条附加 IRAM 分界线。默认 IRAM 四区权限为 RX/RX/RX/无权限；锁定后必须复位才能修改。只换一个分配函数无法更改已锁定的策略。

新增的本项目组件 `native_exec_pool` 实现有界的例外区：

1. 在 `.iram1.native_exec_arena` 中链接一段 512 字节对齐的 16 KiB 数组。IDF 将其计入 IRAM 预留，避免与普通堆重叠。实际地址来自链接器，没有猜测 C6 映射。
2. 通过链接器 `--wrap` 包装启动时的 `esp_mprot_set_prot` 调用。先应用原始默认策略但暂不进行最后的锁定，再把 I0/I1 放在池的两侧，**只给 area 1** 配置 RWX。area 0/2 保持 RX，area 3 保持不可访问，DRAM 权限不变。
3. 在启动包装函数返回前，锁定 IRAM、DRAM、RTC-fast 的分界线、权限和异常监视器，监视器保持开启。检测到调试器时拒绝启动配置，不接受 IDF 的调试绕过路径。
4. 分配前回读验证边界、IRAM/DRAM 权限、锁位和监视器开启状态；验证失败则拒绝分配。仅在 `CONFIG_NATIVE_EXEC_POOL=y` 时，局部 ELF adapter 使用该池；不会回退到放开普通堆执行权限。
5. 分配器一次只允许一个块，拒绝零长度、超容量和已有占用的请求。释放时清零已使用区域，重定位和清零后执行 `fence.i`。当前是单一调用者 POC，不是并发分配器。

没有修改本机 ESP-IDF 安装源码，也没有修改 Passport OS 主程序或 Lua。使用的 IDF 私有 API 和启动包装方式固定于 v5.5.3；升级 IDF 时必须重新审计。

**权衡：**这是固定、永久 RWX 的例外区，不是运行时 W^X。PMS 寄存器已经锁定，因此卸载后不能撤销执行权限。卸载会清零代码/数据，但该区域直到复位仍有 RWX 权限。Native 代码仍与 OS 同权限，不是第三方应用沙箱；没有声称支持动态 RX/RW 切换或代码/数据独立权限。

## 构建

```sh
source "$IDF_PATH/export.sh"
./experiments/native_elf_c3/build_poc.sh       # 默认：开启保护的独立内存池
./experiments/native_elf_c3/build_pool.sh heap # 故意从普通堆执行代码
./experiments/native_elf_c3/build_pool.sh text # 故意写入 OS IRAM 代码区
```

原先关闭保护的对照显式保存在 `build_legacy_poc.sh`。`sdkconfig.pool.defaults` 在 `sdkconfig.protected.defaults` 基础上开启内存池；组件要求内存保护和锁定都开启。

反例固件会因违规访问故意重启，只用于验收。按照 [N1](N1_C3_POC.zh_CN.md) 的完整备份/实验/恢复流程操作，不能在没有恢复方案时以独立实验分区表替换 Passport OTA 布局。

正常受保护 POC 镜像：**191,856 字节**；size 报告 (`../../experiments/native_elf_c3/logs/pool-size.log`; 本地诊断，不公开)。内存池静态预留 **16,384 字节**，另有对齐与实现状态开销。实际测试镜像及备份哈希：pool-artifacts.json (`../../experiments/native_elf_c3/logs/pool-artifacts.json`; 本地诊断，不公开)。

## 真机结果

正常镜像内存池：**`0x40381400 .. 0x40385400`**，入口 **`0x40381518`**。加载前后 `native_exec_verify()` 都成功，验证了边界/权限回读及三个内存类型的锁位和监视器状态。

| 项目 | 实测结果 |
|---|---|
| 内存保护 / 锁定 | 开启 / 开启 |
| 通用 EXEC free/min/largest | 0 / 0 / 0，刻意保持不变 |
| 专用池容量 | 16,384 字节 |
| ELF 加载池占用 / 峰值 | 4,588 / 4,588 字节 |
| 每次卸载后池占用 | 0 字节；整个池清零检查 = 1 |
| 成功加载前/中/后普通 8BIT heap | 312,292 / 312,292 / 312,292 字节 |
| 成功加载期间 8BIT minimum / largest | 312,292 / 172,032 字节 |
| 两次加载/卸载调用结果 | 每次均依次返回 42、43；通过 |
| 零长度 / 超容量 / 第二次同时分配 | 均拒绝 |
| 不支持的 `R_RISCV_64` | -22，不调用入口，heap drift=0 |

池是静态预留的，因此加载开销体现为池占用，不再体现为通用堆减少。退出不会将 16 KiB 预留返还普通堆，而是清零后供下次加载复用。单块被占用期间，池中剩余部分不服务第二次分配。错误重定位测试还会在普通堆中临时复制 ELF，该测试副本不计入表中成功加载的 minimum。

验证器要求的实际权限回读为：

| 区域 | area 0 | area 1 | area 2 | area 3 |
|---|---|---|---|---|
| IRAM | RX | 内存池 RWX | RX | 无权限 |
| DRAM | 无权限 | RW | RW | RW |

IRAM、DRAM、RTC-fast 的分界线锁、权限锁、监视器锁和监视器开启状态均有检查。除有界的 IRAM area 1 例外，其余保留默认配置。

## 违规访问对照与恢复

- **写 OS 代码：**目标 `0x40385400`，恰好位于池外，实际触发 **Memory protection fault**，faulting address 与目标一致，没有执行成功标记。
- **从普通堆执行：**目标 `0x40394530`，位于池外，实际异常是 **Illegal instruction**，MEPC 为该目标，MTVAL=0。这是实测异常类型；测试标签最初预期 PMS panic，不能将实测结果描述成名为 Memory protection fault 的异常。权限回读独立确认 IRAM area 3 没有访问/执行权限，没有执行成功标记。
- 新备份和恢复的 **8 MiB 整片摘要均校验通过**。原 OS 恢复后保留一个已安装的 `org.passport.xiaozhi` 应用，Lua 启动 `ok=1`，退出后 Lua 状态释放，没有遗留串口 monitor。原始 Flash/网络数据只保存在被忽略的本机 `local/`。

证据：

- 正常版本完整真机日志 (`../../experiments/native_elf_c3/logs/pool-device.log`; 本地诊断，不公开)
- 普通堆执行异常捕获 (`../../experiments/native_elf_c3/logs/pool-heap-device.log`; 本地诊断，不公开)、OS 代码写入异常捕获 (`../../experiments/native_elf_c3/logs/pool-text-device.log`; 本地诊断，不公开)
- 备份校验 (`../../experiments/native_elf_c3/logs/pool-backup-verify.log`; 本地诊断，不公开)、恢复校验 (`../../experiments/native_elf_c3/logs/pool-restore-verify.log`; 本地诊断，不公开)、恢复后 Lua 脱敏日志 (`../../experiments/native_elf_c3/logs/pool-os-restored.log`; 本地诊断，不公开)
- 日志自动验收结果 (`../../experiments/native_elf_c3/logs/pool-acceptance.log`; 本地诊断，不公开)：`python3 experiments/native_elf_c3/verify_pool_logs.py`
- 正常/反例构建日志：`logs/pool-normal-build.log`、`logs/pool-heap-build.log`、`logs/pool-text-build.log`；原先不使用池的对照也编译通过（`logs/pool-legacy-build.log`）。

## 范围与后续

开启保护后无法分配的阻塞，已在有界 POC 中解决。没有全局改变通用 `MALLOC_CAP_EXEC`，现有 Passport 固件也尚未集成 Native Runtime。接入 OS 前，需将静态内存池与 LVGL/网络/Lua 一起做预算，并明确接受受信任应用的有界 RWX 策略。容量可按 KiB 配置，本次使用 16 KiB；更大的模块超过预算仍会安全返回分配失败。

N2 ABI、App Manager/App Store 集成、并发执行、1000 次资源压力测试、系统更新发布都不在本次范围。实验不验证原有小智远端/音频问题、UI 视觉效果或任意第三方 Native 代码的安全性。

构建：通过（默认受保护版本、反例版本与 legacy 对照）。主机测试：通过（ELF lint、设备日志自动验收和未修改的 Lua/Passport 回归套件，见 `logs/pool-host-regression.log`）。真机测试：在上述范围内通过。仓库静态检查仍仅被原有中文输入计划缺少双语配对阻断，见 `logs/pool-static-gate.log`。未验证：完整 OS Native 集成、并发分配、长期压力，以及上文明确排除的安全特性。
