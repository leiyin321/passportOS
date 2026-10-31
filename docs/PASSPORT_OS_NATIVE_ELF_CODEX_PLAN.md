[简体中文](PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.zh_CN.md)

# Passport OS Native ELF Runtime — Codex execution plan v0.1

Updated: 2026-10-26

## 0. Goal and boundaries

Hardware: FoloToy AI Passport / ESP32-C3 / 8 MiB Flash / no PSRAM. Passport OS already supports basic Lua MiniApps. Add C/C++ Native ELF Apps that run without an OS reboot and return to Launcher on exit. Existing Lua Apps must not regress.

Explicitly excluded at this stage: Linux/Windows-style process isolation, loading arbitrary ESP-IDF firmware as a module, unrestricted hardware-function access, complete Flash XIP, multiple simultaneous third-party Native Apps, partition-table changes and OTA redesign. Validate a RAM ELF Loader first. Native code shares the OS address space and can crash or overreach the system; clearly mark it as a trusted experimental feature.

### Success criteria

1. Load an independently built ELF on real ESP32-C3 hardware, call its entry, return and unload without rebooting.
2. Call exported OS functions through a stable ABI.
3. Repeat 1000 load/run/unload cycles without crashes or sustained leaks, with quantitative measurements and conditions.
4. Launcher starts/exits a Native Demo while Lua MiniApps, display and buttons still work.
5. Provide reproducible build, flash and execution commands/logs. Mark anything not measured as unverified.

## N0: audit the current project (no direct refactoring)

- Branch `feature/native-elf-loader`; retain baseline build/runtime logs; do not change Lua Runtime interfaces.
- Record `idf.py --version`, compiler version, IDF_TARGET, actual Flash capacity, partitions, LVGL/audio/network initialization and App Manager lifecycle.
- Record `idf.py size`, `idf.py size-components` and firmware binary size.
- Record free/minimum/largest `MALLOC_CAP_8BIT` and `MALLOC_CAP_EXEC` for idle Launcher, a running Lua App, and connected networking.
- Inspect official `espressif/elf_loader` v1.3.5 source/LICENSE: exact C3 exclusions, architecture dependencies, allocation/address conversion, relocations and symbol exports.
- Deliver `docs/native/N0_BASELINE.md` and the C3 adaptation-gap list.
- Gate N0: existing Lua tests pass and sufficient evidence is collected before N1.

## N1: independent ESP32-C3 RAM ELF Loader POC

- Create a standalone IDF host and ELF App under `experiments/native_elf_c3/`. Prefer a fork/port of the official loader, pin the upstream tag/commit and keep patches minimal. Do not merely bypass target restrictions or guess C6 addresses.
- Inspect official `elf_loader`/`elf` build methods and C3 RV32 ABI/ISA first. Produce a real `.app.elf` module, not an ordinary whole-firmware ELF.
- Use `riscv32-esp-elf-readelf -h -S -r -s` and `objdump -dr` to inspect ELF32/RISC-V, entry, relocations and undefined symbols. Explicitly reject unsupported relocations and record a negative case.
- Use genuinely executable memory associated with `MALLOC_CAP_EXEC`; data/BSS must satisfy write/access, alignment and address-remapping rules. Offline preprocessing is allowed to avoid unstable runtime RELAX handling.
- POC #1: `add(20,22)==42`; #2: static strings and local variables; #3: calls and symbols across files; #4: return normally and load again.
- Do not bring Lua, XiaoZhi, audio or WebSocket into this POC.
- Gate N1: real-device demonstration with segment addresses/sizes and RAM peak. Report precise blockers and alternatives instead of claiming success if loading is infeasible.

## N2: Native ABI and OS symbols

- Add `components/passport_native/include/passport_native.h`; fix a pure C ABI with size/version/alignment, calling convention, struct-extension policy and matching host/App architecture/toolchain.
- Export `passport_app_entry`. Choose either entry receiving `passport_host_api_v1_t*` and returning lifecycle callbacks, or explicit start/stop calls; document ownership. Prefer a function table over unrestricted ESP-IDF symbol resolution.
- v1 APIs: log, clock, device info, managed draw/text, key events, managed memory, App Data KV, introduced incrementally. Pass ABI version and reject mismatches.
- Validate App `host->log(...)` and keyboard events.
- Optionally validate read-only ESP-IDF APIs such as `esp_get_free_heap_size()`. Initially disallow `esp_wifi_deinit`, raw Flash, direct driver-register writes and LCD/I2S reinitialization.
- An export allowlist is not a security sandbox. Native code still accesses the OS address space; mark apps trusted/developer-only.
- Gate N2: ABI mismatch, missing symbols and bad entry fail safely. ABI docs and hello-native SDK compile and run on hardware.

## N3: resource ownership, unloading and stress

- One foreground Native App by default. OS owns its main task and stack. Use cooperative `on_start/on_stop`; stop external event sources before destroying objects.
- Register all LVGL objects, timers, subscriptions, buffers and network handles in an instance resource registry. Arbitrary C malloc or `xTaskCreate` resources are not automatically reclaimed by dlclose.
- Initially prohibit or strictly wrap App-created FreeRTOS tasks, hardware ISRs and raw drivers. Validate busy/timeout handling. Never unload code while it or its callbacks still run.
- Run 100 cycles, then 1000; record before/after/low-watermark/largest-block measurements and alternate Lua/Native starts.
- If arbitrary unloading is uncontrollable, restrict v0.1 to managed Native Apps. Do not promise generic cleanup by forcibly deleting arbitrary FreeRTOS tasks.
- Gate N3: repeated loading/unloading is stable; failure cases leave no dangling callbacks; retain device serial evidence.

## N4: integrate the existing App Manager

- Add runtime kind `native-elf`; do not create separate Launcher, storage or App Store systems.
- Example manifest:

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

- Install through existing `.pap`/repository packaging: validate magic, target, ABI, size and integrity, then store in the package cache; keep persistent data separate.
- Start: Manager -> Loader -> instance -> viewport/key setup. Exit: cancel callbacks/wait for entry return -> unmap/free -> Launcher. Removing code keeps App Data; complete deletion requires its separate confirmation.
- Preserve standard 240x272/fullscreen 240x320 contracts and App-root ownership; access LVGL only in its designated context.
- Gate N4: alternate Lua and Native counters without reboot or data loss.

## N5: Native SDK and examples

- `sdk/native/`: C/C++ templates, CMake/toolchain, C3 ABI headers, package scripts, ELF lint and documentation.
- `examples/native_hello`: arithmetic/log/time; `native_counter`: text, buttons and persistent count; `native_tetris`: lightweight Canvas game; `native_device_info`: managed hardware information.
- Each example provides build/package/install/start/exit/uninstall commands. Actually validate at least hello/counter; explicitly label missing hardware validation.
- Gate N5: a new app requires no OS-source change or firmware rebuild once the Loader/API and installable package path exist.

## N6: independently investigate Flash XIP (must not block N0–N5)

- Inspect `esp_partition_mmap`, MMU page granularity and executable mapping permissions; an ordinary LittleFS file is not directly executable XIP.
- POC: fixed mappable code/constants, page-aligned contiguous partition, PIC/relocations and host calls. Measure code-RAM savings, mapping/unload, cache synchronization and stability.
- Back up App Data before any experimental partition changes; never silently reset the existing 8 MiB layout. Prefer a standalone test layout/firmware.
- Gate N6: if XIP is infeasible, retain RAM loading and state realistic maximum Native budgets.

## Per-stage report

- Git commit/upstream loader tag/target/IDF/compiler versions.
- Source changes, reasons and patch list.
- Build commands, exact artifacts and readelf output.
- Device reproduction and serial logs; distinguish build, emulator/QEMU and physical-device success.
- ELF text/rodata/data/BSS and EXEC/8BIT free/minimum/largest before/after, plus firmware size.
- Gate PASS/FAIL, blockers and next steps. Never claim unfinished work is complete.

## One-time Codex instruction

Develop Native ELF Runtime v0.1 in this Passport OS project through N0–N6 gates. Audit first and preserve Lua. Pin a reproducible official Espressif loader version, explicitly acknowledge C3 is not officially supported, build the independent C3 RAM POC before a maintained ABI, then validate lifecycle/stress, integrate App Manager and produce the SDK. Do not load whole-firmware ELF as a plugin, treat a target-list change as a completed port, or introduce XIP/all ESP-IDF APIs/XiaoZhi at once. Keep build/run evidence and actual measurements; do not cross failed gates. Each stage has a separate commit and `docs/native/Nx_*.md`; prioritize N0/N1 initially and report failures precisely.

## Official references

- https://components.espressif.com/components/espressif/elf_loader (v1.3.5 as of 2026-10-26; official targets exclude esp32c3)
- https://github.com/espressif/esp-iot-solution/tree/master/examples/elf_loader
- https://docs.espressif.com/projects/esp-idf/en/v5.5.6/esp32c3/api-reference/system/mem_alloc.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/spi_flash/index.html
