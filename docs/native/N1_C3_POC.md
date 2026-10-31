[简体中文](N1_C3_POC.zh_CN.md)

# N1 — ESP32-C3 RAM ELF loader POC

Date: 2026-10-26

**Current follow-up:** [locked-protection pool](N1_PROTECTED_POOL.md) now passes on hardware with protection and locking enabled. The heap-only results below are retained as the original comparison. The default `build_poc.sh` now builds the protected pool.

**Gate N1: PASS for the independent, memory-protection-disabled experiment.** Real ESP32-C3 loading, calls, return, unload, reload, static strings/BSS, multi-file calls, supported relocation, and unsupported-relocation rejection were tested. The original 8 MiB Passport image was restored and verified; Lua starts/exits successfully. **Current Passport OS integration remains blocked by its enabled memory-protection policy. N2–N6 were not started.**

Upstream: Espressif `elf_loader` v1.3.5, repository commit `4962410c80a51cd5822da1d8d2041eb4285265f1`, Apache-2.0. Implementation base commit: `cf6d487`; the following validation/fix commit contains this updated report. Hardware: ESP32-C3 rev v1.1, 8 MiB embedded Flash, no PSRAM. Toolchain: ESP-IDF v5.5.3, RISC-V GCC 14.2.0.

[Pinned upstream source and license](https://github.com/espressif/esp-iot-solution/tree/4962410c80a51cd5822da1d8d2041eb4285265f1/components/elf_loader)

## Isolated implementation and local patches

The project resides at `experiments/native_elf_c3/`. Passport OS App Manager, Lua Runtime, package protocol, partitions and SDK were not edited. The upstream documentation directory is explicitly registered in the repository checker as vendored, following the existing policy; sensitive-content checks remain active. The vendored component has these experimental patches:

1. Add C3 to the local component manifest and Kconfig target conditions to expose the generic RISC-V implementation.
2. Request `MALLOC_CAP_EXEC` for code in the non-mirror allocator path. Upstream uses `MALLOC_CAP_8BIT` there even when `exec=true`. No C6 memory addresses are assumed; the IDF C3 allocator supplies the actual executable address.
3. Use `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=n` only in the isolated POC configuration. The protected comparison build retains `y`. This is not a change to Passport OS's security configuration.
4. Propagate the return from `esp_elf_arch_relocate()` in `src/esp_elf.c`. The upstream caller ignored it: the device logged “info=2 is not supported” yet `esp_elf_relocate()` returned 0. The local fix returns the error; the caller deinitializes the partially loaded image without calling it.

The official `elf_loader.cmake` uses freestanding PIC/shared linking with a dedicated entry. The experiment follows that model using explicit C3 ISA/ABI flags and `--no-relax`, rather than loading a full firmware ELF. Scope is a zero-based PT_LOAD layout checked by the linter. It does not establish support for arbitrary ELF layouts, C++ constructors/TLS, or every relocation type.

## Build and reproduction

```sh
source "$IDF_PATH/export.sh"
./experiments/native_elf_c3/build_legacy_poc.sh

IDF_TARGET=esp32c3 idf.py -C experiments/native_elf_c3 \
  -B experiments/native_elf_c3/build-protected \
  -D SDKCONFIG=sdkconfig.protected \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.protected.defaults' build
```

Normal POC: `build/native_elf_c3_poc.bin`, **167,488 bytes**. Protected comparison: `build-protected/native_elf_c3_poc.bin`, **172,864 bytes**. Module: `module/native.app.elf`, **1,960 bytes**. Paths are relative to the experiment. Exact tested hashes are retained in device-artifacts.json (`../../experiments/native_elf_c3/logs/device-artifacts.json`; local diagnostics, not published).

The normal POC uses a standalone factory partition at `0x10000`. Its initial write replaced bootloader `0x0`, partition table `0x8000`, and application `0x10000`; later POC variants replaced only that test application. These addresses are incompatible with the original Passport OTA layout. The original full Flash was backed up and digest-verified first, and restored afterward. Do not flash the POC into Passport without that explicit backup/restore procedure.

Reproduction sequence on the verified device port:

1. Close the authorized monitor. Read the full `0x800000` bytes while leaving the chip in the bootloader (`esptool.py --after no_reset read_flash ...`), then `verify_flash 0x0 BACKUP` without starting the OS. Keep the backup private.
2. Write the reviewed POC images at the addresses above; capture bounded boot/run output using `capture_serial.py PORT LOCAL_LOG --seconds 10`.
3. Exercise the normal POC and protected comparison separately.
4. Restore `BACKUP` at `0x0` with flash mode/frequency/size `keep`, keep the chip in the bootloader, and verify the full digest again.
5. Boot the restored OS; exercise `home`, `list`, `launch org.passport.xiaozhi`, `metrics`, `home`, `metrics`. End the capture and release the port.

## ELF inspection

readelf (`../../experiments/native_elf_c3/logs/module-readelf.txt`; local diagnostics, not published), program headers (`../../experiments/native_elf_c3/logs/module-program-headers.txt`; local diagnostics, not published), objdump (`../../experiments/native_elf_c3/logs/module-objdump.txt`; local diagnostics, not published), linter result (`../../experiments/native_elf_c3/logs/module-lint.log`; local diagnostics, not published):

- ELF32, little-endian, RISC-V `ET_DYN`, C3 `rv32imc_zicsr_zifencei` / `ilp32`; entry `0x118`; no unresolved imports.
- `.text` 82, `.rodata` 4, `.data` 4, `.bss` 4 bytes. PT_LOAD span is `0x11ec` = **4,588 bytes**, including ELF metadata and segment padding.
- One `R_RISCV_RELATIVE` patches an indirect function pointer to `helper_add()` in a separate translation unit. Disassembly confirms actual `lbu` reads of the static string, rather than compile-time folding.
- The entry returns 42 and then 43 as BSS state increments. `esp_elf_request()` is also invoked, but reports loader status rather than the app's return value; direct `elf.entry` calls verify the integers.
- The negative test copies the trusted module, changes its relocation type to unsupported `R_RISCV_64`, and expects `-EINVAL`. It never calls the rejected entry.

## Actual executable-memory measurements

ESP32-C3, no display/Wi-Fi/audio initialized in this independent POC. All values below are bytes. EXEC and 8BIT describe overlapping physical heaps here: **do not add the two values**.

| Normal POC phase, cycles 0 and 1 | EXEC free | EXEC minimum | EXEC largest | 8BIT free | 8BIT minimum | 8BIT largest |
|---|---:|---:|---:|---:|---:|---:|
| before first load | 328,708 | 328,708 | 188,416 | 328,708 | 328,708 | 188,416 |
| loaded | 324,096 | 324,096 | 188,416 | 324,096 | 324,096 | 188,416 |
| after unload | 328,708 | 324,096 | 188,416 | 328,708 | 324,096 | 188,416 |
| after second load/unload | 328,708 | 324,096 | 188,416 | 328,708 | 324,096 | 188,416 |

Measured successful-load heap increase: **4,612 bytes** including allocator overhead; PT_LOAD payload span: 4,588 bytes. App code performs no heap allocation. Each load/unload returns EXEC and 8BIT free bytes to its own pre-load value, **drift=0**. The extra temporary payload copy used by the negative test is outside this successful-load peak; its final free-byte drift is also zero.

Actual cycle-0 segment base is `0x403905a4`; entry `0x403906bc`. RX segment: address `0x403905a4`, filesz/memsz 368/368. RW segment: address `0x40391714`, filesz/memsz 120/124. These addresses were returned by the C3 allocator and logged on the device.

Protected comparison (`CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y`): EXEC free/min/largest **0/0/0**; 8BIT free **328,692**. Loading returns **-12 (`-ENOMEM`)**, leaves entry/segment null, and consumes no heap. This directly verifies the current-policy blocker; compiling for C3 alone does not resolve it.

## Test evidence

| Test | Result and evidence |
|---|---|
| Existing Lua/Passport host tests | PASS before and after device work; final full suite output (`../../experiments/native_elf_c3/logs/host-regression.log`; local diagnostics, not published) |
| Repository checker unit tests | PASS, 22 tests; output (`../../experiments/native_elf_c3/logs/repo-checker-tests.log`; local diagnostics, not published) |
| Complete repository static gate | FAIL on the pre-existing Chinese-only input plan (`docs/PASSPORT_OS_NATIVE_ELF_CODEX_PLAN.md`), which lacks a Chinese peer and English default prose; output (`../../experiments/native_elf_c3/logs/static-gate.log`; local diagnostics, not published). The supplied plan was preserved. |
| Normal and protected POC firmware builds | PASS; final normal build (`../../experiments/native_elf_c3/logs/poc-device-build.log`; local diagnostics, not published), protected build (`../../experiments/native_elf_c3/logs/poc-protected-build.log`; local diagnostics, not published); initial clean build remains in `poc-build.log` |
| Initial calculation/BSS load/reload | PASS; device log (`../../experiments/native_elf_c3/logs/poc-basic-device.log`; local diagnostics, not published) |
| Strings, locals, BSS, multi-file indirect function, relative relocation | PASS; complete final device log (`../../experiments/native_elf_c3/logs/poc-device.log`; local diagnostics, not published) |
| Unsupported relocation, upstream behavior | FAIL reproduced: error logged but rc=0; before-fix device log (`../../experiments/native_elf_c3/logs/unsupported-before-fix.log`; local diagnostics, not published) |
| Unsupported relocation, local fix | PASS: rc=-22, no entry call, cleanup drift=0; final device log |
| Enabled memory protection | Expected block confirmed; complete device log (`../../experiments/native_elf_c3/logs/poc-protected-device.log`; local diagnostics, not published) |
| Full original Flash restoration | PASS; device digest verification (`../../experiments/native_elf_c3/logs/restore-verify.log`; local diagnostics, not published) |
| Lua App after restoring original OS | PASS for serial start/exit, installed list retained; sanitized OS log (`../../experiments/native_elf_c3/logs/os-restored.log`; local diagnostics, not published) |
| 100/1000-cycle stress and Launcher Native integration | NOT RUN; N3/N4, outside this request |

Raw private OS/network logs and Flash data remain under ignored `local/`; committed OS logs contain only lifecycle/metrics. POC logs are complete after device identifiers are removed. The initial UART-lock failure is retained as historical evidence, not the final status. No monitor remains open.

## Next-stage recommendation

C3 RAM ELF loading is mechanically feasible in the tested `memprot=n` configuration. The official manifest/Kconfig exclusions are proven; the experiment does not establish Espressif's undocumented rationale for those exclusions. Current Passport `memprot=y` remains incompatible with normal executable-heap allocation.

Before implementing N2, decide how to preserve or explicitly change that protection policy. Research whether a reserved executable arena and C3 memory-protection configuration can support writing before execution without globally disabling protection; feasibility is not established here. If this requires a protection downgrade, keep it an explicit trusted/developer-only firmware variant with a documented tradeoff. The stable Lua OS remains the fallback.

After that decision, proceed to a small versioned C function-table ABI, then separately perform N3 lifecycle/stress validation and N4 App Manager integration. No Native ABI, App Store/package integration, OS service exports, XiaoZhi Native implementation or XIP work was added in N1. A successful POC does not create process isolation or a sandbox.
