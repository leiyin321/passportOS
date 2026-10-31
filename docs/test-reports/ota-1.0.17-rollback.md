<p align="right"><a href="ota-1.0.17-rollback.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# OTA 1.0.17 rollback and 1.0.18 fix

## Device evidence

The user downloaded and installed release 1.0.17 through Settings → System
Update. The candidate booted from `ota_1` with ESP-IDF state `PENDING_VERIFY`.
Storage initialization passed, then the `passport_http` task raised a stack
protection fault before the candidate could confirm itself. ESP-IDF rolled back
to `ota_0`; the device restarted as 1.0.16 and reached `Passport OS ready` with
Wi-Fi online.

The matching archived ELF was verified before symbolication:

- Version/build: `1.0.17` / `2026102520`
- ELF SHA-256: `030e1405510005777d424d5c2f8aa7676179440f03194df1e24500dcbbe6a172`
- Faulting task: `passport_http`
- Stack bounds: `0x3fcdc8b4`–`0x3fcde8b0`; captured SP: `0x3fcdbd10`, 2,980 bytes below the lower bound
- PC `0x42012a52` maps to `worker` in `os/services/network/network.c`; MEPC `0x420131f2` maps to `xQueueReceive` in that worker.

The target ABI sizes the network `job` descriptor at 9,308 bytes. The worker
declared it as an automatic local while its FreeRTOS stack was only 8,192 bytes;
the worker also has other local buffers. This directly explains the observed
stack overrun. The fault was not caused by signature rejection or image
download corruption: the candidate image booted and reached storage startup.

## Fix in 1.0.18

Network jobs are now allocated once on the heap and the one-entry FreeRTOS queue
stores a pointer rather than copying a full job. The worker and all request
entry points no longer place the large job descriptor on their task stacks.
Submission rejection and worker completion release the allocation. A host
regression test guards this ownership/stack-placement contract.

The pointer queue uses 4 bytes instead of the 9,308-byte queue item, saving
9,304 bytes of idle queue storage. An active request uses one 9,308-byte heap
job. The firmware `.dram0.bss` is 64,608 bytes, unchanged from 1.0.17.

## Validation and release

- `python3 tests/passport/test_network_worker_stack.py`: 2 tests passed.
- `PASSPORT_OS_VERSION=1.0.18 ./tools/validate.sh`: full host and ESP-IDF 5.5.3 gate passed.
- App image: 2,278,416 / 2,621,440 bytes; 343,024 bytes free (13.08%).
- Merged image: 2,409,488 bytes; SHA-256 `719895b49c04d73027e822a66e253a7641267bc657abc7bcac30eacfb8ffa6e1`.
- Signed update: version `1.0.18`, build `2026102521`, app SHA-256 `ecd9e121907b68b187deb1eeda97d5d28d5c7f602272793ef3fcbbb64d4cf99e`.
- The local update service returned the complete image with matching size and SHA-256. Signature verification passed during release generation.
- Follow-up device acceptance: the user installed 1.0.18 from Settings. The candidate booted from `ota_1` as version 1.0.18 with ELF SHA-256 `c317ec41cb3630a0c95e3cfaccb1c278449b4d55ace3c14c98e3e885d6052027`. Storage initialized, `OTA self-test valid rc=0` was logged, and Wi-Fi reached `WIFI online`; the user confirmed the About version is 1.0.18. The full capture contained no panic after the candidate boot.
- Candidate startup metrics: free heap 85,320 bytes, minimum free heap 85,264 bytes, largest free block 73,728 bytes; `passport_http` task stack high-water mark 3,884 bytes. These are startup measurements, not peak network-transfer measurements.

The raw panic capture is kept under ignored `local/`; it is not committed because
it contains device-specific log data.
