[English](native-apps.md)

# Native ELF 应用

Passport OS 1.0.34 在 Lua 和声明式应用之外增加了受信任的 ESP32-C3 Native Runtime。Native App 按普通签名 `.pap` 安装，出现在现有应用库中，无需重启 OS 即可运行，退出后返回主页。它不是任意 ESP-IDF 完整固件，也不是进程沙箱。

## 构建、签名与运行

先激活 ESP-IDF 5.5.3 环境。固件与应用使用同一 RISC-V 工具链（GCC 14.2.0、`rv32imc_zicsr_zifencei`、ILP32 soft-float）。

```sh
PYTHONPATH=sdk python -m passport init /tmp/native-demo \
  --id org.example.native-demo --runtime native-elf
PYTHONPATH=sdk python -m passport native build /tmp/native-demo
PYTHONPATH=sdk python -m passport native inspect /tmp/native-demo/app.app.elf
PYTHONPATH=sdk python -m passport pack /tmp/native-demo \
  --key /path/to/os-publisher-private.pem --public sdk/publisher-public.pem \
  -o /tmp/native-demo.pap
PYTHONPATH=sdk python -m passport validate /tmp/native-demo.pap --public sdk/publisher-public.pem
PYTHONPATH=sdk python -m passport install /tmp/native-demo.pap \
  --public sdk/publisher-public.pem --port /dev/cu.usbmodem1101
PYTHONPATH=sdk python -m passport device launch org.example.native-demo --port /dev/cu.usbmodem1101
PYTHONPATH=sdk python -m passport device view --port /dev/cu.usbmodem1101
PYTHONPATH=sdk python -m passport device home --port /dev/cu.usbmodem1101
PYTHONPATH=sdk python -m passport device metrics --port /dev/cu.usbmodem1101
```

使用实际设备串口。示例在 [`examples/native_hello`](../../examples/native_hello/main.c)。`native build` 编译 C 文件并附带少量 freestanding 编译器支持，不会把 BSP、LVGL 或 ESP-IDF 链接进每个应用。未解析导入、不支持的重定位、无效入口/段和超预算镜像会在预检阶段失败；编译错误会在打包前报告。

**发布者策略：**Native v1 仅接受 OS 内置发布者。为 Lua 添加信任的发布者不自动获得 Native 机器码权限，`trusted:true` 只是声明，不是自我授权。开发者需向 OS 发布者提交源码/ELF 审核并签名；其他私钥签名可在本机校验，但该 OS 会拒绝安装。不要分发签名私钥。

## Manifest 与应用包

```json
{
  "format":"passport.app/1",
  "id":"org.example.native-demo",
  "name":"Native demo",
  "version":"0.1.0",
  "runtime":"native-elf",
  "entry":"app.app.elf",
  "target":"esp32c3",
  "abi":1,
  "trusted":true,
  "min_os_api":1,
  "data_schema":1,
  "permissions":["ui","graphics","input","timer"],
  "limits":{"native_exec_kib":16,"fps":10,"persistent_kib":1},
  "display":{"mode":"standard","orientation":"portrait","navigation":"buttons"}
}
```

Native 入口为包含 NUL 的二进制字节，仍使用 PAP1/PAP2 的长度、哈希和签名封装。Lua/声明式入口的 UTF-8 与大小约束不变，`publisher_key_id`、`sha256` 由 `pack` 生成。Native v1 文件上限为 32 KiB，PT_LOAD 跨度（含元数据、填充、BSS）最多 16 KiB；`native_exec_kib` 可声明更小预算。Native 不使用 `script_heap_kib`，不能提供该字段。通用包契约仍要求 `persistent_kib`，但 v1 没有 Native 私有存储 API。

此 ABI 仅开放 `ui`、`graphics`、`input`、`timer` 权限，没有导出 Native 存储、网络、音频或硬件驱动接口，Lua 原有能力继续保留。包仍复用 cache、registry、data 归属及现有 App Store 传输，系统固件与应用独立更新。

## C ABI v1

规范头文件为 [`passport_native.h`](../../sdk/native/include/passport_native.h)，机器可读摘要为 [`abi-v1.json`](../../sdk/native/abi-v1.json)。

```c
int passport_app_entry(const passport_native_host_v1 *host,
                       passport_native_app_v1 *app);
```

检查 `host->abi` 和 `host->size`，填写 `app->abi`、`app->size`、可选的 `event`/`stop` 函数指针，成功返回 0，失败返回负数。OS 检查回调版本/大小及函数指针是否位于已加载的可执行段。入口和回调在前台上下文同步执行，必须尽快返回；死循环仍可能导致 OS 崩溃或复位，Native 没有 Lua 指令钩子或抢占沙箱。

| Host 方法 | 契约 |
|---|---|
| `text(utf8,color)` | 复制短于 512 字节、NUL 结尾的 UTF-8 到受管视图；RGB24；要求 `ui` |
| `clear(background)` | 清空受管绘图/图片/文字并填充应用区域；要求 `graphics` |
| `rect(x,y,w,h,color)` | 非负坐标、正尺寸、RGB24；裁剪到应用区域，最多 192 条；要求 `graphics` |
| `millis()` | 单调 32 位毫秒数，自然回绕；没有 `timer` 权限时返回 0 |
| `language()` | 只读系统语言，`en` 或 `zh-CN` |
| `display_mode()` | 只读 `light` 或 `dark`；应用自行决定配色 |

绘图成功返回 0，参数、配额或权限错误返回 -1。接口只修改受管视图，不暴露 LVGL 指针，现有 OS renderer 负责同步。standard 240x272、fullscreen 240x320 契约与 Lua 一致。

事件：press=1、release=2、short=3、double=4、long=5、tick=6。按键值：up=0、down=1、OK=2；tick 参数是经过的毫秒数。OS 保留双击 OK 返回主页，UP/DOWN 不生成双击。`stop` 在代码清除前调用，卸载后不再派发回调。

## 所有权、限制与安全

静态预留的 16 KiB 池存放应用代码/data/BSS。Lua 运行时该区域也持续预留；Native 退出后清零并可复用，不返还普通堆。通用 EXEC heap 刻意保持为 0。内存保护与锁定保持开启，只有有界池为 RWX；这不是动态 W^X，也不能隔离 Native 对可写 OS 数据的访问。

不支持应用自行创建 RTOS task、ISR、长期异步回调、raw Flash/驱动或重初始化 OS 服务。支持全局变量和有界栈临时区，不导出 OS heap 分配器。SDK 在模块内提供 `memcpy`/`memset`/`memmove`/`memcmp` 支持编译器生成的操作。全部 Native 代码都受信任并与 OS 同权限，函数表是 API 边界，不是安全边界。

OS 仅在同步入口、事件与 stop 返回后卸载单个前台镜像。Native 与 Lua 使用同一 AppManager session 清理流程。卸载代码（`offload`）保留数据，完整删除使用现有 `remove` 明确确认。

## 验证与排查

```sh
PYTHONPATH=sdk python -m passport device cycle org.example.native-demo 1000 \
  --timeout 300 --port /dev/cu.usbmodem1101 --verbose
```

`metrics` 报告池容量/占用/峰值；当前诊断行中的 `protection=0` 表示验证返回 `ESP_OK`，不是关闭了保护。`view` 报告运行时文字/错误和 Lua 占用。Native 启动失败会输出 `PASSPORT launch FAIL`，CLI 返回失败，具体原因见 `view`。Manifest ABI 不符会拒绝安装，回调 ABI 不符会在接收事件前安全卸载。

ELF 无效时，检查 ISA、入口、PT_LOAD 边界、相对重定位、未定义符号。Native v1 要求小端 ELF32 RISC-V ET_DYN、零基址加载布局、无导入/TLS/构造函数，且仅支持 `R_RISCV_RELATIVE`。先检查声明预算及根发布者签名，不要把通用 EXEC heap 为 0 误判为网络错误。

## ABI 2 服务扩展（OS 1.0.35 起）

ABI 1 保持兼容。ABI 2 增加通用 HTTPS、WebSocket、JSON 字段读取、设备身份、私有存储及 Opus 音频，详见 [ABI 2](native-services.zh_CN.md)。Manifest 使用 `abi:2`；1.0.34 不支持此扩展，必须先升级 OS。
