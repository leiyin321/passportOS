[English](0001-runtime-choice.md)

# ADR 0001：Lua 5.4.8，单个限额前台 VM

状态：真机测量后接受（2026-10-25）。使用 MIT Lua 5.4.8，官方归档 SHA256
`4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`。
单 VM，32KiB 分配器配额，指令及时间限制；仅加载 base/table/string/math，
移除文件与动态代码加载及 GC 参数控制。不加载 OS/io/debug/package/coroutine。
绘图只提供通用指令，不将游戏规则放入原生服务。API 与 Lua 解耦。
真机预算不达标时重审，不同时引入 XS/WASM。

实测：Tetris 18.2Hz，Lua 峰值 19,565 字节；500 次会话通过。
