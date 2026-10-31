<p align="right"><strong>简体中文</strong> · <a href="app-library-card-crash.md">English</a></p>

# 应用库卡片栈保护异常

## 复现与证据

Passport OS 1.0.18 上进入“应用”卡片会导致 `main` 任务触发栈保护异常。设备
重启后仍进入已确认的 1.0.18 槽；这是运行时 panic，不是 OTA 回滚。

采集日志与归档 1.0.18 ELF 匹配（`c317ec41cb3630a0c95e3cfaccb1c278449b4d55ace3c14c98e3e885d6052027`）。panic PC `0x4217588a` 符号化为 newlib `_svfprintf_r`；RA `0x4216e35e` 位于 `sprintf` 内。任务 SP 为 `0x3fcadee0`，比报告栈底 `0x3fcadf64` 低 132 字节。

应用库卡片绘制期间，`library_app_version()` 会延迟调用 `ps_store_load()` 读取已安装
应用版本。该路径会验签并解析整个 PAP 包，包括格式化发布者哈希；这发生在较大的
`draw()` 栈帧仍然有效时。代码调用路径和栈数据支持这是故障原因。

## 1.0.19 修复

`refresh_app_list()` 现在在绘制前读取已安装应用版本并缓存。`draw()` 只从缓存获取
卡片版本，不再在卡片渲染过程中加载/解析应用包。新增源码契约回归测试，确保元数据
读取始终位于绘制路径之外。

## 验证与发布

- `python3 tests/passport/test_library_render_stack.py`：通过。
- `PASSPORT_OS_VERSION=1.0.19 ./tools/validate.sh`：完整宿主和 ESP-IDF 5.5.3 门禁通过。
- 应用镜像：2,278,416 / 2,621,440 字节；剩余 343,024 字节（13.08%）。
- 合并镜像：2,409,488 字节；SHA-256 `c6ccc92c740e5a3d71eac5b26c3ec204d5d048094e6777d61cefbd6d7ee0d5ac`。
- 已签名发布 1.0.19 / build `2026102522`；应用 SHA-256 `2b340c94f700ac2691ad03f1a5df61a62d1dc0c55749d61999e6358f84262f2e`。
- 正在运行的 Mac 更新服务已返回清单和完整镜像；签名、大小及 SHA-256 验证通过。
- 安装 1.0.19 后尚待在真机复测应用库卡片。

原始 USB 采集保存在被 Git 忽略的 `local/`，未提交。
