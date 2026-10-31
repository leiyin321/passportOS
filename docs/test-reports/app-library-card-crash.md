<p align="right"><a href="app-library-card-crash.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# App Library card stack fault

## Reproduction and evidence

On Passport OS 1.0.18, opening the App Library card caused a stack-protection
fault in the `main` task. The device rebooted back into the already-confirmed
1.0.18 slot; this was a runtime panic, not an OTA rollback.

The capture matched the archived 1.0.18 ELF (`c317ec41cb3630a0c95e3cfaccb1c278449b4d55ace3c14c98e3e885d6052027`). Panic PC `0x4217588a` symbolicates to newlib `_svfprintf_r`; RA `0x4216e35e` is inside `sprintf`. The task stack pointer was `0x3fcadee0`, 132 bytes below the reported lower bound `0x3fcadf64`.

The App Library card renderer called `library_app_version()`, which lazily called
`ps_store_load()` to read the installed version. That path verifies and parses
the complete PAP package, including formatted publisher-hash generation, while
the large `draw()` frame is live. The location and stack measurements support
this as the cause.

## Fix in 1.0.19

`refresh_app_list()` now resolves installed app versions before drawing and
caches them. `draw()` reads the cache for card labels and no longer performs
package load/version parsing for those cards. A source-contract regression test
checks that metadata loading stays outside the render path.

## Validation and release

- `python3 tests/passport/test_library_render_stack.py`: passed.
- `PASSPORT_OS_VERSION=1.0.19 ./tools/validate.sh`: full host and ESP-IDF 5.5.3 gate passed.
- App image: 2,278,416 / 2,621,440 bytes; 343,024 bytes free (13.08%).
- Merged image: 2,409,488 bytes; SHA-256 `c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`.
- Signed release: 1.0.19 / build `2026102522`; app SHA-256 `2b340c94f700ac2691ad03f1a5df61a62d1dc0c55749d61999e6358f84262f2e`.
- The running Mac update service returned the manifest and complete image; signature, size, and SHA-256 verification passed.
- Device retest of the App Library card after installing 1.0.19 is pending.

The raw USB capture is under ignored `local/` and is not committed.
