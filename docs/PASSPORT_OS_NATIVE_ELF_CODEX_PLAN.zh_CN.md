[English](PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.md)

# Passport OS Native ELF Runtime — Codex 执行计划 v0.1

更新：2026-10-26

## 0. 目标与边界

硬件：FoloToy AI Passport / ESP32-C3 / 8 MiB Flash / 无 PSRAM。现状：Passport OS 已支持简单 Lua MiniApp。本计划添加 C/C++ Native ELF App，应用运行时 OS 不重启，退出后返回 Launcher，原有 Lua App 不回归。

本阶段明确**不实现**：完整 Linux/Windows 进程隔离、任意 ESP-IDF 固件直接装载、任意硬件函数无约束访问、完整 Flash XIP、同时运行多个第三方 Native App、修改分区表和 OTA 方案。先验证 RAM ELF Loader。Native 代码与 OS 同一地址空间，存在系统崩溃/越权风险，必须明确标记为可信实验功能。

### 成功定义

1. ESP32-C3 真机无需重启，加载 independently built ELF，调用入口，返回并卸载。
2. 能从应用调用已导出的、ABI 稳定的 OS 函数。
3. 重复 1000 次 load/run/unload 不崩溃、不持续漏内存（给出量化统计和测量条件）。
4. Launcher 可以启动和退出 Native Demo，且 Lua MiniApp、显示、按键仍可正常工作。
5. 输出可复现构建、烧录、运行的命令、日志和测试结果。无法实测时严格标明“未验证”。

## N0：审计已有工程（禁止直接重构）

- 分支 `feature/native-elf-loader`；保存基线编译与运行日志；不要更改现有 Lua Runtime 接口。
- 记录 `idf.py --version`、编译器版本、`IDF_TARGET`、Flash 实际尺寸、分区表、LVGL/音频/网络初始化与 App Manager 生命周期。
- 记录 `idf.py size`, `idf.py size-components`, 固件二进制大小。
- 运行时记录 `MALLOC_CAP_8BIT` / `MALLOC_CAP_EXEC` 的 free, minimum, largest block；采样场景：Launcher 空闲、Lua App 运行、联网状态。
- 核对官方 `espressif/elf_loader` v1.3.5 的源码及 LICENSE，查明 `esp32c3` 被排除的位置、架构依赖、各内存分配与地址转换函数、重定位实现及导出符号机制。
- 交付 `docs/native/N0_BASELINE.md` 和“C3 适配缺口清单”。
- Gate N0：现有 Lua 测试通过；信息充分，方可进行 N1。

## N1：独立 ESP32-C3 RAM ELF Loader POC

- 在 `experiments/native_elf_c3/` 建独立 ESP-IDF host + ELF app 示例。优先 fork/移植官方 loader，固定 upstream tag/commit，最小补丁，**不要简单绕过芯片限制或猜测 C6 的内存地址**。
- 首先核查官方 loader 的 `elf_loader`/`elf` 构建方式和 C3 的 RV32 ABI/ISA。为独立模块输出适配的 `.app.elf`，不能把普通全固件 `.elf` 当动态模块。
- 使用 `riscv32-esp-elf-readelf -h -S -r -s` 和 `objdump -dr` 检查 ELF32/RISC-V、入口、重定位类型及 undefined symbols。对不支持的 relocation 明确报错并记录测试例。
- 代码装载到真实 `MALLOC_CAP_EXEC` 可执行内存；data/bss 分配符合读写属性、对齐和地址修正规则。必要时采用装载前离线预处理，避免不稳定的运行时 RELAX 处理。
- POC #1：纯计算 `add(20,22) == 42`；#2：静态字符串和局部变量；#3：函数调用与多文件符号；#4：正常返回后再次装载。
- 不把 Lua/小智/音频/WebSocket 合入本阶段。
- Gate N1：真机演示 POC 通过；记录每个段实际地址/大小、RAM 峰值；若 ELF Loader 不可行，给出精准阻塞点与备选，而非假装完成。

## N2：定义 Native ABI 与 OS 符号

- 新建 `components/passport_native/include/passport_native.h`；固定纯 C ABI，明确 size、version、alignment、调用约定、struct 字段扩展策略，宿主与 App 同架构同 toolchain。
- 约定应用导出 `passport_app_entry`，OS 传入 `passport_host_api_v1_t*`，返回生命周期函数表（或显式 start/stop 调用），二选一且写明所有权。推荐 **函数表而不是无约束解析全部 ESP-IDF 符号**。
- v1 API 只包含：log、clock、device info、managed draw/text、key event、managed memory、app data KV（逐步开放）。OS 向 Native App 传递入口 ABI version；拒绝版本不匹配。
- 验证 Native App 调用 `host->log(...)` 并收到键盘事件。
- 可选验证 OS 导出 `esp_get_free_heap_size()` 等只读 ESP-IDF API；首版禁止 `esp_wifi_deinit`、raw flash、直接写驱动寄存器、重新初始化 LCD/I2S。
- 说明：导出符号白名单**不是安全沙箱**，Native 代码仍可访问 OS 地址空间；App 标记 trusted/developer-only。
- Gate N2：ABI 版本校验、缺失符号和错误入口都能安全失败；ABI 文档和 hello-native SDK 编译通过并实测。

## N3：资源域、卸载及压力测试

- 默认单 Native foreground App，OS 负责创建/持有主任务和其栈；提供 `on_start/on_stop` 协作式退出，停止外部事件源后销毁对象。
- 所有注册的 LVGL 对象、定时器、订阅、缓冲、网络句柄纳入 instance resource registry。不能假设 C 代码的 malloc 与任意 xTaskCreate 都会在 dlclose 时自动回收。
- 在第一阶段禁用或严格封装 app-created FreeRTOS tasks、硬件 ISR、直接驱动操作；验证 busy timeout 的失败处理。运行中的程序/回调不可直接卸载代码段。
- 先进行 100 次循环，之后 1000 次；记录 before/after/low watermark/large block，反复启动 Lua 及 Native 测试。
- 若卸载失控，允许把 v0.1 实验版限定为受管 Native App；**不要以强杀 FreeRTOS 任意任务作为一般资源回收承诺**。
- Gate N3：重复加载卸载稳定；各类错误条件下不会留下悬挂回调；保留相应真机串口日志。

## N4：接入现有 OS App Manager

- 在现有 `AppManager` 中新增 runtime kind `native-elf`，而不是新建一套 Launcher、数据存储或 App Store。
- Manifest 样例：

```json
{
  "id":"org.passport.native.counter",
  "version":"0.1.0",
  "runtime":"native-elf",
  "entry":"app.app.elf",
  "target":"esp32c3",
  "abi":1,
  "display":{"mode":"standard"},
  "trusted":true,
  "permissions":["display","input"]
}
```

- 安装：读取 app 包（沿用 `.pap` 机制或现有仓库格式）、验证 magic/目标芯片/ABI/大小/完整性、写入现有 package cache，持久数据仍独立。
- 启动：Manager -> Loader -> instance -> setup viewport/key events；退出：取消回调/等待主入口退出 -> unmap/free -> Launcher；删除：删除包而非 App Data，完整卸载另行确认。
- 不改变现有 standard 240×272 / fullscreen 240×320 契约，UI 对象属于 app root；只能从指定 LVGL 上下文操作 LVGL。
- Gate N4：Lua 计数器和 Native 计数器可交替运行，无重启，原有应用数据不丢失。

## N5：Native SDK 与示例

- `sdk/native/`：C/C++ 模板、CMake/toolchain 配置、针对 esp32c3 的 ABI headers、打包脚本、ELF lint 脚本、文档。
- `examples/native_hello`：加法、日志、时间；`native_counter`：画文字、处理按键、保存计数；`native_tetris`：轻量 Canvas 游戏；`native_device_info`：调用受管理硬件信息函数。
- 每个示例均提供 `build`, `package`, `install`, `start`, `exit`, `uninstall` 命令，并实际验证至少 hello、counter；无法真机验证要标注。
- Gate N5：新建应用无需修改 OS 源码和重新刷写系统（前提是 Loader + API 已内置且包可安装）。

## N6：独立研究 Flash XIP（不得阻塞 N0-N5）

- 研究 `esp_partition_mmap`、MMU 映射粒度和执行映射权限；确认普通 LittleFS 文件不能被直接当成可执行 XIP 段。
- POC 固定地址可映射代码/常量、页对齐连续分区、PIC/relocation 和 host 函数调用；记录代码 RAM 节省、Flash 映射/卸载、缓存同步、运行稳定性。
- 任何需要调整分区表的 POC 必须先备份 App Data，不能悄悄重置已有 8MiB 布局；建议独立测试分区和测试固件。
- Gate N6：如 XIP 不可行，保留 RAM Loader，给出最大可安装/可执行 Native App 预算，不夸大能力。

## 每阶段报告模板

- Git commit / upstream loader tag / target / IDF version / toolchain version
- 所做源码变更、原因、补丁清单
- 构建命令与精确产物路径、ELF readelf 输出
- 真机复现步骤、串口运行日志；区分 `编译通过`、`QEMU/模拟器通过`、`真机通过`
- ELF `.text/.rodata/.data/.bss`，装载前后 `MALLOC_CAP_EXEC`/`8BIT` free/min/largest，固件大小
- 当前 Gate PASS/FAIL，阻塞问题、建议下一步；未完成不可声称完成

## 给 Codex 的一次性主指令

你在当前 Passport OS 工程中开发 `Native ELF Runtime v0.1`。请严格遵循本文件 N0→N6 的阶段/Gate；首先执行 N0 审计，保留 Lua 功能。参考并锁定 Espressif `elf_loader` 官方最新版可复现版本，明确其尚未官方支持 ESP32-C3，先建独立 C3 RAM ELF Loader POC，再定义可维护的 Native ABI，再做回收压力测试，最终接入已有 AppManager，并开发 Native SDK。不得将完整固件 ELF 误作插件，不得用单纯修改 CMake supported targets 视作适配成功，不得一次性引入 XIP/所有 ESP-IDF API/小智 AI。所有代码必须有构建/运行日志及真实测量证据，未通过 Gate 禁止跨阶段扩展，阶段失败要产生诊断报告。每阶段提交独立 commit 和 `docs/native/Nx_*.md`。优先完成 N0 与 N1 并报告结果。

## 官方参考

- https://components.espressif.com/components/espressif/elf_loader （截至 2026-10-26 最新 v1.3.5，官方 target 列表无 esp32c3）
- https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader
- https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32c3/api-reference/system/mem_alloc.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/spi_flash/index.html
