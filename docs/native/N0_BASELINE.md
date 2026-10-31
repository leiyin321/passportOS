[简体中文](N0_BASELINE.zh_CN.md)

# N0 — Passport OS baseline and C3 ELF support audit

Date: 2026-10-26
Scope: read-only audit plus existing Lua regression suite. No Passport OS runtime or Lua source was changed for this experiment.

## Result

**Gate N0: audit and Lua baseline PASS.** After the user authorized closing the monitor, connected Launcher and Lua App heap samples were collected on the device. Full 8 MiB backup and restoration digest checks pass; Lua starts and exits after restoring the original image. The installed OS metrics command has no EXEC fields: OS EXEC values are derived as zero from the same IDF/config capability map; a separate protected POC directly measures EXEC free/min/largest as zero. Do not describe derived OS values as directly read from its metrics command.

## Toolchain and target

| Item | Observed |
|---|---|
| Project branch at start | `feature/passport-os` |
| Project HEAD at start | `6326c2414f3cab844d5257351ff88279fd9ee414` |
| Worktree | Already contained extensive user changes; this experiment is isolated under `experiments/native_elf_c3/` and `docs/native/` |
| ESP-IDF | v5.5.3 |
| Compiler | `riscv32-esp-elf-gcc 14.2.0 (esp-14.2.0_20251107)` |
| IDF target | `esp32c3` |
| Flash | Hardware verified ESP32-C3 rev v1.1, embedded 8 MiB Flash (XMC) |
| PSRAM | None configured / Passport board has no PSRAM |
| Passport security config | `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y`, lock enabled |
| Lua implementation | `libpassport_lua.a`: 102,470 bytes archive contribution (100,262 code + 2,208 rodata) |

The IDF size report is in `../experiments/native_elf_c3/logs/passport-idf-size.txt` (`../../experiments/native_elf_c3/logs/passport-idf-size.txt`; local diagnostics, not published); component details are in `../experiments/native_elf_c3/logs/passport-size-components.txt` (`../../experiments/native_elf_c3/logs/passport-size-components.txt`; local diagnostics, not published).

## Flash layout

Configured flash size: 8 MiB. Current `partitions.csv`:

| Name | Offset | Size | Purpose |
|---|---:|---:|---|
| nvs | `0x9000` | `0x6000` | system settings |
| phy_init | `0xf000` | `0x1000` | Wi-Fi PHY calibration |
| otadata | `0x10000` | `0x2000` | OTA selection |
| ota_0 | `0x20000` | `0x280000` | firmware slot |
| ota_1 | `0x2a0000` | `0x280000` | firmware slot |
| packages | `0x520000` | `0x260000` | installed app packages |
| appdata | `0x780000` | `0x80000` | persistent app data |

Current app image: 2,078,560 bytes (`0x1fb760`), leaving 543,904 bytes in a 2.5 MiB OTA slot. `idf.py size` reports 1,611,470 bytes Flash Code, 350,820 bytes rodata, and 180,406 bytes static DRAM (64,544 BSS + 14,084 data + 101,778 DRAM text). These are link-time sections, not free heap.

## App Manager and Lua lifecycle

- `main/passport_main.c:744` `open_app()` closes a previous app, loads a package through `ps_store_load()`, opens a session, calls `ps_runtime_start()`, and measures startup.
- `main/passport_main.c:730` `close_app()` ends the session, whose owned cleanup releases the package and runtime. This is the existing Launcher/App Manager lifecycle to preserve.
- `os/runtime/runtime.c:603` `ps_runtime_start()` first calls `ps_runtime_stop()`, validates the package/display contract, then creates the Lua/declarative runtime.
- `os/runtime/runtime.c:773` `ps_runtime_stop()` calls Lua `on_stop`, closes `lua_State`, stops declarative UI, and resets app-local state.
- The POC does not modify any of these files, package format, manifest schema, or runtime interfaces.

## Lua baseline tests

Command: `./tools/passport/test.sh`
Result: **PASS**. The full captured output is `../experiments/native_elf_c3/logs/lua-baseline-tests.log` (`../../experiments/native_elf_c3/logs/lua-baseline-tests.log`; local diagnostics, not published). It includes runtime API and gesture tests, package/declarative checks, network worker/WebSocket checks, audio lifecycle cleanup, icon asset compiler/runtime checks, and other OS test suites. No Lua behavior was changed by the POC.

## Official Espressif elf_loader v1.3.5 audit

Upstream provenance: component metadata identifies Espressif repository commit `4962410c80a51cd5822da1d8d2041eb4285265f1`, path `components/elf_loader`, version 1.3.5. The vendored copy and upstream Apache-2.0 license are under `components/elf_loader/`.

1. **Registry rejection:** upstream `idf_component.yml:9-18` target allowlist contains ESP32, S2/S3, S31, C5/C6/C61, and P4; it excludes C3. With the component pinned at `==1.3.5`, IDF Component Manager reports “no versions ... match ==1.3.5” for target `esp32c3` and lists the other compatible targets.
2. **Kconfig rejection:** upstream `Kconfig:1-14` hides the menu and makes `ELF_LOADER` unavailable unless the target is in the same list; C3 is absent.
3. **Architecture implementation:** `CMakeLists.txt` selects `src/arch/esp_elf_riscv.c` for the generic `CONFIG_IDF_TARGET_ARCH_RISCV`, not specifically for C6. Thus source architecture selection alone does not prove C3 support.
4. **Relocations:** `src/arch/esp_elf_riscv.c:95-108` implements `R_RISCV_NONE`, `R_RISCV_32`, `R_RISCV_RELATIVE`, and `R_RISCV_JUMP_SLOT`; other relocation types return `-EINVAL`. Module compiler/linker output must be checked against this limited set.
5. **Executable allocation:** upstream `src/esp_elf_adapter.c:64-78` in the non-mirror/no-PSRAM path allocates `MALLOC_CAP_8BIT` without distinguishing `exec`. Passport OS has memory protection enabled. ESP-IDF v5.5.3 C3 `components/heap/port/esp32c3/memory_layout.c:37-40` removes `MALLOC_CAP_EXEC` from the C3 heap capabilities when `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` is enabled. Therefore the current Passport configuration cannot provide this POC with executable heap memory using the normal heap API. This is a concrete integration blocker independent of the registry target list.
6. **Entry return value:** `src/esp_elf.c:729-739` invokes `elf->entry()` and then returns zero, discarding the module's integer return value. A future runtime ABI must specify an explicit result channel or invoke/interpret a defined entry ABI; it must not assume `esp_elf_request()` returns the app result.

### C3 adaptation gaps

| Gap | Evidence | Consequence |
|---|---|---|
| C3 absent from registry metadata and Kconfig | upstream manifest and Kconfig target allowlists | Official package cannot be selected/built on C3 without an explicit port |
| Current Passport memory protection disables executable heap capability | Passport sdkconfig + IDF C3 heap layout | Loader cannot execute a RAM module in the existing firmware configuration |
| Upstream non-mirror allocator ignores the `exec` flag | `esp_elf_adapter.c` allocation branch | Experimental adapter must request executable memory; behavior must be verified on C3 |
| Small relocation subset | RISC-V relocation switch | General compiler output is not automatically loadable |
| Request API hides app return | request implementation | Return-value behavior needs a separate ABI contract |
| OS metrics lack EXEC fields | OS capability map + protected POC | OS EXEC=0 is configuration-derived; the protected POC directly measures zero |

The POC makes isolated experimental changes for the target allowlist, Kconfig, allocator and relocation-error propagation (see N1 for the device-reproduced upstream error). This is not represented as official Espressif support and is not copied into Passport OS.

## Device baseline and restoration

The following `MALLOC_CAP_8BIT` values were directly sampled in bytes. The raw local log confirms a Wi-Fi connection event.

| State | free | minimum | largest |
|---|---:|---:|---:|
| boot | 193,816 | 193,816 | 114,688 |
| after Wi-Fi initialization | 96,972 | 96,916 | 86,016 |
| connected Launcher idle | 89,724 | 86,136 | 77,824 |
| XiaoZhi Lua running | 46,424 | 24,304 | 15,872 |
| stable sample after Lua exit | 89,636 | 24,304 | 77,824 |
| Lua running after full restoration | 46,396 | 24,288 | 15,872 |
| Lua exited after full restoration | 89,548 | 24,288 | 77,824 |

Network background allocations vary between boots; differences between separate OS samples are not a Native leak measurement. Each independent Native POC load/unload directly measures zero drift, as reported in N1.

Both Lua starts report `ok=1`. The installed-app list retains `org.passport.xiaozhi`, count=1, before and after restoration. This does not validate the existing XiaoZhi remote protocol/audio issue or visually inspect the display. Original OS code and Lua interfaces were not changed.

Sanitized evidence:

- Launcher baseline (`../../experiments/native_elf_c3/logs/os-launcher-baseline.log`; local diagnostics, not published)
- Lua baseline (`../../experiments/native_elf_c3/logs/os-lua-baseline.log`; local diagnostics, not published)
- Lua after restoring the original OS (`../../experiments/native_elf_c3/logs/os-restored.log`; local diagnostics, not published)
- Backup verification (`../../experiments/native_elf_c3/logs/backup-verify.log`; local diagnostics, not published), full restoration verification (`../../experiments/native_elf_c3/logs/restore-verify.log`; local diagnostics, not published)

The Flash backup remains only at ignored `local/native_elf_c3/passport-verified-backup.bin`; it is not committed. The initial read reset the OS, which wrote configuration and caused the first comparison to differ. Keeping the chip in the bootloader, rereading system data/appdata, and combining the snapshot then passed a full-device digest check. Restoration was verified again before booting the OS. Private raw boot/network logs also remain local.

The follow-up [protected pool experiment](N1_PROTECTED_POOL.md) resolves loader allocation with protection/locks enabled by reserving a bounded arena; it intentionally does not change the generic EXEC heap capability.
