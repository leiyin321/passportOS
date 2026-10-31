[简体中文](N1_PROTECTED_POOL.zh_CN.md)

# N1 follow-up — ELF loading with locked C3 memory protection

Date: 2026-10-26. ESP-IDF v5.5.3 / ESP32-C3 rev v1.1 / 8 MiB Flash / no PSRAM.

**PASS on hardware:** ELF loading, calls and unloading now work with both `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE=y` and `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE_LOCK=y`. The generic EXEC heap intentionally remains zero. The loader uses a separate statically reserved 16 KiB executable arena; ordinary heap memory is not made executable. Original Passport Flash was restored and digest-verified, and the Lua app starts/exits normally. This remains an independent N1 experiment; no N2–N6 integration was implemented.

## Protection policy and implementation

The IDF C3 [default protection implementation](https://github.com/espressif/esp-idf/blob/v5.5.3/components/esp_hw_support/port/esp32c3/esp_memprot.c) uses a main I/D split plus two additional IRAM split lines. Default IRAM areas are RX/RX/RX/none; all locks become irreversible until reset. Merely replacing an allocator cannot change a locked policy.

The first-party `native_exec_pool` component implements a bounded exception:

1. Link a 512-byte-aligned, 16 KiB array in `.iram1.native_exec_arena`. IDF includes it in the IRAM reservation, preventing overlap with the ordinary heap. Its actual address comes from the linker, not a guessed C6 mapping.
2. Wrap the startup call to `esp_mprot_set_prot` using the linker `--wrap` option. First apply the original default policy without its final locks, then place I0/I1 at the arena boundaries and give **only area 1** RWX permission. Keep areas 0/2 RX, area 3 inaccessible, and the original DRAM permissions unchanged.
3. Lock split lines, permissions and violation monitors for IRAM, DRAM and RTC-fast memory before returning from the startup wrapper. Monitoring remains enabled. A debugger-attached startup is rejected, rather than accepting IDF's debug bypass.
4. Verify all boundary readbacks, IRAM and DRAM permissions, lock bits, and enabled monitors before allocating. Failure closes allocation. The local ELF adapter uses this arena only when `CONFIG_NATIVE_EXEC_POOL=y`; there is no fallback to executable ordinary heap.
5. The allocator accepts one block at a time, rejects zero/oversized/busy requests, clears used words on free, and issues `fence.i` after relocation and clearing. It is a single-owner POC, not a concurrent allocator.

No installed ESP-IDF source or Passport OS main/Lua code was edited. The use of private IDF APIs and startup wrapping is pinned to v5.5.3 and must be re-audited for an IDF upgrade.

**Tradeoff:** this is a fixed, permanently RWX exception, not runtime W^X. Its permissions cannot be revoked after unload because the PMS registers are locked. Unload clears the code/data, while the arena remains RWX until reset. Native code still shares OS privileges; this is not a third-party sandbox. Dynamic RX/RW switching or separate code/data permissions are not claimed.

## Build

```sh
source "$IDF_PATH/export.sh"
./experiments/native_elf_c3/build_poc.sh       # default: protected pool
./experiments/native_elf_c3/build_pool.sh heap # deliberate heap execution fault
./experiments/native_elf_c3/build_pool.sh text # deliberate OS IRAM write fault
```

`build_legacy_poc.sh` retains the original unprotected comparison explicitly. `sdkconfig.pool.defaults` enables the pool on top of `sdkconfig.protected.defaults`; the component requires protection and locking to be enabled.

The fault builds intentionally reboot on forbidden accesses. They are acceptance probes, not normal app behavior. Use the established full backup/test/restore procedure in [N1](N1_C3_POC.md); the standalone test partition table must not replace Passport's OTA layout without restoration.

Normal protected POC image: **191,856 bytes**; size report (`../../experiments/native_elf_c3/logs/pool-size.log`; local diagnostics, not published). Static arena reservation: **16,384 bytes** plus alignment and implementation state. Tested image identities and backup hashes: pool-artifacts.json (`../../experiments/native_elf_c3/logs/pool-artifacts.json`; local diagnostics, not published).

## Actual device results

Normal image arena: **`0x40381400 .. 0x40385400`**, entry **`0x40381518`**. `native_exec_verify()` succeeds before and during loading, proving boundary/permission readbacks and all three types' lock/monitor state.

| Property | Actual result |
|---|---|
| Memory protection / lock | enabled / enabled |
| Generic EXEC free/min/largest | 0 / 0 / 0, intentionally unchanged |
| Dedicated pool capacity | 16,384 bytes |
| ELF loaded pool usage / peak | 4,588 / 4,588 bytes |
| Pool usage after each unload | 0 bytes; whole-arena zero check = 1 |
| Ordinary 8BIT heap before/during/after successful load | 312,292 / 312,292 / 312,292 bytes |
| Ordinary 8BIT minimum / largest during successful load | 312,292 / 172,032 bytes |
| Calls, two load/unload cycles | 42 then 43 each cycle; PASS |
| Zero size / oversized / second concurrent allocation | rejected |
| Unsupported `R_RISCV_64` | -22; no entry call; heap drift=0 |

Pool bytes are statically reserved, so the load cost is reported as pool usage instead of a reduction in generic heap. Unload does not return the 16 KiB reservation to ordinary heap: it clears the block and makes it reusable. The unused pool tail cannot service a second allocation while the single block is owned. Negative-relocation testing also allocates a temporary ELF copy in normal heap; that extra test buffer is not included in the successful-load minimum above.

The actual permission readback required by the verifier is:

| Region | Area 0 | Area 1 | Area 2 | Area 3 |
|---|---|---|---|---|
| IRAM | RX | RWX arena | RX | none |
| DRAM | none | RW | RW | RW |

All IRAM, DRAM and RTC-fast split/permission/monitor locks and monitor enables are checked. These are the existing defaults except the bounded IRAM area 1 exception.

## Fault controls and restoration

- **OS code write:** target `0x40385400`, immediately outside the arena, raises an actual **Memory protection fault** with that faulting address. No success marker is reached.
- **Ordinary heap execution:** target `0x40394530`, outside the arena, fails with **Illegal instruction**, MEPC at the attempted target and MTVAL=0. This is the observed exception category; the test's initial label expected a PMS panic, so it must not be reported as a named Memory protection fault. Permission readback independently confirms IRAM area 3 has no access/execute permission. No success marker is reached.
- Fresh full **8 MiB backup and restore digest checks pass**. Restored OS retains one installed `org.passport.xiaozhi` app, reports Lua startup `ok=1`, and releases Lua state on exit. No monitor remains open. Raw Flash/network data remain private under ignored `local/`.

Evidence:

- Normal complete device log (`../../experiments/native_elf_c3/logs/pool-device.log`; local diagnostics, not published)
- Heap execution fault capture (`../../experiments/native_elf_c3/logs/pool-heap-device.log`; local diagnostics, not published), OS code write fault capture (`../../experiments/native_elf_c3/logs/pool-text-device.log`; local diagnostics, not published)
- Backup verification (`../../experiments/native_elf_c3/logs/pool-backup-verify.log`; local diagnostics, not published), restore verification (`../../experiments/native_elf_c3/logs/pool-restore-verify.log`; local diagnostics, not published), sanitized restored Lua log (`../../experiments/native_elf_c3/logs/pool-os-restored.log`; local diagnostics, not published)
- Automated acceptance of recorded evidence (`../../experiments/native_elf_c3/logs/pool-acceptance.log`; local diagnostics, not published): `python3 experiments/native_elf_c3/verify_pool_logs.py`
- Normal/fault build logs: `logs/pool-normal-build.log`, `logs/pool-heap-build.log`, `logs/pool-text-build.log`; original no-pool build also compiles (`logs/pool-legacy-build.log`).

## Scope and next steps

The allocation blocker is resolved for this protected, bounded POC. Generic `MALLOC_CAP_EXEC` is not changed globally, and existing Passport firmware does not yet contain a Native Runtime. Before OS integration, budget the static arena alongside LVGL/network/Lua and explicitly accept the bounded RWX policy for trusted apps. Capacity can be configured in KiB; this test used 16 KiB. Larger modules still fail cleanly when they exceed that budget.

N2 ABI, App Manager/App Store integration, concurrent execution, 1000-cycle resource stress, and live system update publication remain outside this change. This experiment does not validate the existing XiaoZhi remote/audio issue, visual UI quality, or general third-party Native safety.

Build: PASS (default protected, fault variants and legacy comparison). Host tests: PASS (ELF lint, recorded-device acceptance and the unchanged Lua/Passport regression suite in `logs/pool-host-regression.log`). Device tests: PASS within the scope above. The repository static checker still fails only on the pre-existing Chinese-only input plan lacking its bilingual peer; see `logs/pool-static-gate.log`. Unverified: full-OS Native integration, concurrent allocation, long stress, and the security properties explicitly excluded above.
