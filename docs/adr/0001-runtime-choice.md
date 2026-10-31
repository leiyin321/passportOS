[简体中文](0001-runtime-choice.zh_CN.md)

# ADR 0001: Lua 5.4.8, one bounded foreground VM

Status: accepted after real hardware measurements (2026-10-25). Use Lua 5.4.8 (MIT), official archive SHA256
`4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`.
Single VM, 32KiB allocator budget, instruction hooks and wall time; only base/table/string/math,
remove file loading, dynamic code loaders and collectgarbage tuning. No OS/io/debug/package/coroutine.
Use generic draw commands, not native game logic. API independent of Lua.
A failing hardware budget requires revisiting this decision; no XS/WASM alongside Lua.

Measured: Tetris 18.2Hz, Lua peak 19,565 bytes; 500 sessions passed.
