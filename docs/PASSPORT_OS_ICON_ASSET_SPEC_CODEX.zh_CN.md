[English](PASSPORT_OS_ICON_ASSET_SPEC_CODEX.md)

# Passport OS — 图标与图像资源系统开发交接文档

版本：v1.0（2026-10-25）  
目标硬件：FoloToy AI Passport，ESP32-C3 / 8 MiB Flash / 无 PSRAM / 240×320 LCD  
目标读者：Codex / Passport OS 开发者  
状态：设计与验收规范；以下接口、路径、CLI 为**拟议实现**，不是声称已有代码。

## 0. 任务总目标

在现有 Passport OS（ESP-IDF、LVGL、App Manager、`.pap` 包格式、Package Cache / App Data 分离）中实现一套**空间小、RAM 可控、开发者易用**的图标资源系统。开发者仍然使用 PNG/SVG 等常见素材；电脑端打包器转换为设备友好的格式；应用只通过资源 ID 使用图标，不接触 LCD 驱动和原始文件系统。

本任务交付必须包含真实代码、示例应用、单元测试、尺寸测量和（有设备时）真机测试；禁止仅提交设计文档就宣布完成。

## 1. 非目标和关键约束

- 不增加浏览器、SVG 渲染引擎、完整 PNG 解码器或在线图标字体下载能力。
- **不要在 MCU 上实时解析 SVG。** 所有 SVG 在主机构建期栅格化。
- 不把整套 Lucide/Material 图标字体打入固件；系统只保留常用且实际启用的图标。
- 不为了节省 Flash 而引入过大的解码时 RAM 峰值。
- 不要求所有小游戏使用图像素材：纯矩形/线条/文字应优先使用绘图原语。
- 普通小程序不得通过图片路径访问其他 App Data、NVS、系统文件和其他应用缓存。
- **锁定工程实际使用的 LVGL major/minor** 再实现图像转换与加载；特别注意 LVGL 8/9 的图像描述结构不兼容。以下 I4 / A1 等选择以 LVGL 9 文档为参考，具体支持由编译版本决定。
- 官方硬件 BSP、LVGL 绘图线程/锁和显示 DMA 缓冲必须优先复用，不得重新建一个 240×320 RGB565 全屏帧缓冲（约 150 KiB）。

## 2. 三层资源体系

### 2.1 System Icon Registry（系统共享图标）

系统拥有一个经构建期裁剪的只读图标注册表。小程序只引用稳定的逻辑 ID：`system.wifi`、`system.battery`、`system.back`、`system.settings`、`system.microphone`、`system.cloud`、`system.gamepad`、`system.chat` 等。

- 系统图标优先使用 LVGL 已有 Symbols；缺失时使用 1-bit alpha mask（A1）或 4-bit alpha mask（A4），运行时调色。
- 一个图标只存一次，任何小程序复用；不复制进 `.pap` 包。
- ID 含语义，不能依赖底层字形 Unicode 编码；OS 内部负责映射。
- 未识别图标返回标准 `system.app-default` 占位图，不崩溃。
- 用户主题影响颜色，不增加独立深色/浅色图标副本。

### 2.2 App Icon（App Store、Launcher 应用专有图标）

- 默认 48×48 像素；允许构建期从更大的源图缩放，设备端一般不缩放。
- 推荐 **I4（16 色索引）**：48×48 原始像素约 1,152 B，加调色板（以 16 项 × 4 B 估算 64 B）及头部。
- 简单单色应用可引用系统图标而不打包任何图像。
- 需要更多色彩时自动升至 I8，若非必要不使用 RGB565。
- 透明像素使用保留的透明调色板项；确认当前 LVGL 版本对索引图透明度/调色板 alpha 的解码语义，不得假定所有版本相同。
- App Registry 可以仅保留轻量元数据；图标不存在时先显示系统默认图标，必要时按需获取。

### 2.3 Game Sprite / Illustration（游戏图块与插图）

- 优先程序化绘制：俄罗斯方块使用着色矩形即可，不必提供 7 套大位图。
- 8×8 至 32×32 小 Sprite 优先 I4；色彩复杂可使用 I8。
- 需要旋转/缩放的图片：提前生成不同方向/尺寸的图块，或转为当前 LVGL 路径支持的格式；**不能假定索引图支持运行时任意旋转/缩放**。
- 不透明复杂插图才考虑 RGB565；照片类可以在第二阶段评估 JPEG，但不得在 v1 引入额外高内存解码依赖。
- 动画用 sprite sheet/图块序列，但不得一次把整张大图解压进 RAM。

## 3. 格式选型矩阵

| 用途 | 源素材 | 设备端格式 | 默认尺寸 | 备注 |
|---|---|---|---|---|
| 状态栏图标 | SVG/PNG | 系统 LVGL Symbol / A1 | 12–16 | 可主题重着色 |
| 通用 UI 图标 | SVG/PNG | A1 / A4 | 20–24 | 单色或抗锯齿遮罩 |
| App Store 默认图标 | 逻辑 ID | System Registry | 32–48 | 不重复存储 |
| 应用自定义图标 | PNG/SVG | I4，必要时 I8 | 48×48 | 透明度严格验证 |
| 小游戏 Sprite | PNG | I4 / I8 | 8–32 | 尽量整像素绘制 |
| 装饰插图 | PNG/JPEG | 按实测选择 I8/RGB565 | 按需 | 延迟加载，有尺寸/体积上限 |

容量公式（均不含编码头、文件系统开销）：

- A1：`ceil(width * height / 8)` 字节；24×24 = **72 B**。
- A4：`ceil(width * height / 2)` 字节；24×24 = **288 B**。
- I4：`ceil(width * height / 2)` + 调色板；48×48 = **1,152 B + palette**。
- I8：`width * height` + 调色板；48×48 = **2,304 B + palette**。
- RGB565：`width * height * 2`；48×48 = **4,608 B**。
- 50 个 48×48：I4 像素总计约 56.25 KiB；RGB565 约 225 KiB；不含调色板和文件开销。

以上是像素载荷理论值，最终以打包器产物和真机加载内存为准。扫描行 stride、对齐、像素打包顺序与调色板编码遵循选定 LVGL 版本，**不能直接按裸像素公式自行拼凑并声称兼容**。

## 4. 应用资源 API（协议示例）

声明式 UI：

```json
{"type":"icon","name":"system.wifi","size":24,"color":"theme.text"}
```

应用自带图标：

```json
{"type":"image","src":"app://icons/weather","width":48,"height":48}
```

拟议 Lua API：

```lua
passport.ui.icon("system.wifi", 24)
passport.graphics.sprite("app://sprites/bird", 48, 96)
```

规则：

- `system.*` 只能指向 OS 预置注册表；`app://` 仅能解析当前应用签名包内 Manifest 声明的资源。
- 不允许任意绝对路径、`../` 路径穿越、`file://` 或网络 URL 直通图像解码器。
- 图片资源必须在加载前通过 Manifest 索引、尺寸、字节数和哈希校验。
- 图片句柄属于当前 App Session；退出 App 后释放句柄与解码/绘图缓存，但保留应用包缓存及独立 App Data。
- 每个 App 同时活跃图片/缓存预算必须可配置并可观测。

## 5. 构建期 Asset Compiler

建议实现 `tools/passport_assets/`（与已有工程目录合并即可）。不要自行重写成熟图像量化器，优先使用主机端 Python + Pillow 或 LVGL 官方离线转换工具，具体以项目许可、格式兼容性与测试结果决定。

输入：开发者 PNG/SVG（SVG 需在主机端通过经过固定版本的栅格化工具转 PNG）；Manifest 资源描述。

处理步骤：

1. 验证尺寸、像素数、帧数、源文件大小、路径合法性；拒绝过大或损坏的素材。
2. 依据资源用途和 alpha/颜色复杂度选择 A1 / A4 / I4 / I8 / RGB565。
3. 对索引图执行确定性的颜色量化和调色板生成；必要时提供 `--no-dither`，避免图标边缘噪点。
4. 输出**当前 LVGL 版本可直接读取的 BIN 图像**或有明确版本的 Passport 资源包装；如果使用自有头部，仅在 OS AssetLoader 解析校验后转换为 LVGL 描述，不能将自定义头直接传给 LVGL。
5. 生成资源索引（ID、类型、宽高、编码、数据长度、SHA-256、可选主题/状态变体）。
6. 将静态资源打入 `.pap`；生产构建拒绝散落的原始 SVG/PNG，除非声明为服务端资源且由网络服务独立处理。
7. 打印每个资源和整个应用包的体积报告，给出 RGB565 基准下的节省比例。

建议命令行形式（待开发）：

```bash
passport assets build examples/weather/assets --manifest examples/weather/manifest.json
passport assets report build/weather.pap
passport assets inspect build/weather.pap
```

### 5.1 压缩策略

v1 **优先采用未压缩 I4/I8/A1/A4**；图形本身已够小，避免增加复杂解码和 RAM 压力。

v1.1 再评估 RLE/LZ4，且必须先证明相关 LVGL 配置已开启、可以分块绘制，以及峰值 RAM 不高于预算。**不要直接对 `.bin` 做通用 zip 压缩后在 MCU 上整体解压。**

### 5.2 字体和字符

系统应复用一套必要的 UI 字体；不要每个应用内嵌中文字体。中文字符子集可由开发者工具生成，但先明确字形授权、字集范围、字体包更新及缺字替代方案。大字体或全量中文字库会迅速吃掉 Flash；字体优化要与图标优化分开统计。

## 6. Runtime Icon Registry + AssetLoader

建议模块：

```text
services/display/icon_registry.*
services/display/asset_loader.*
services/display/image_cache.*
tools/passport_assets/
tests/assets/
examples/icon_gallery/
```

必须提供：

- 静态系统图标 ID → LVGL Symbol / A1 / A4 的查找与回退。
- 应用资源 ID → 经 Package Manager 核验的对象 / 有界文件句柄。
- 读取小块图片时按需加载，不强制将完整 App 包放进 RAM。
- 解码与 LVGL 绘制遵循项目 UI task/lock 规则；网络任务和游戏脚本不能随意跨线程修改 LVGL 对象。
- 同时展示 App Store 网格时限制缓存的图标数量，滚动时复用组件或释放不可见图标。
- 禁止在无 PSRAM 设备上出现运行时临时全屏 RGBA/RGB565 缓冲。
- 对损坏的图片、错误格式、未授权资源访问返回错误码和占位图，不能导致 WDT、OOM 或崩溃。
- 指标输出：`flash_bytes`、`decoded_peak_heap`、`free_heap_after_close`、`largest_free_block`、`asset_load_ms`。

## 7. Package Cache 与数据语义

- 资源文件属于 **可回收 App Package**；应用自己创建的存档属于 **Persistent App Data**。
- 自动 LRU/其他缓存策略可回收图标和资源，但不得删除游戏进度。
- 若 App 包未缓存：Launcher 先展示系统默认占位图；联网后下载包或允许的独立 icon 资源。
- 同一个系统图标不应在多应用包中重复保存；资源去重可先做 ID 级，暂不要求复杂 CAS。
- 更新应用时保证资源索引/实际内容一致；临时文件校验成功后再切换有效版本。
- 退出时释放 RAM 图片对象；再次进入可从 Flash 缓存重新解析。

## 8. 安全和可靠性验收

输入攻防用例：巨大宽高、负数/溢出尺寸、伪造图像头、截断 palette、非法索引值、超长资源路径、目录穿越、重复 ID、错误 hash、压缩炸弹、损坏 Flash 文件、资源热更新中断电模拟。

防护要求：

- 用 64-bit 或显式溢出检查计算 `width * height * bpp`，再校验最大大小。
- 读文件不能超过 Manifest 声明的精确字节长度和 App Resource 配额。
- 不信任包里提供的 `cf`、stride、偏移量；校验 LVGL 版本支持及边界。
- 不渲染未验证的文件，不把 App 能控制的指针提供给原生图像加载 API。
- CPU/帧率和缓存内存设配额；连续打开关闭应用 500 次检查无持续堆下降。

## 9. 建议实施步骤（Codex 执行顺序）

| 阶段 | 开发工作 | Gate |
|---|---|---|
| I0 | 检查 Passport 工程使用的 LVGL 版本、图像加载器和可用 Flash/RAM | 给出真实版本、编译与尺寸基线 |
| I1 | 实现 System Icon Registry 和 fallback | 系统页可稳定渲染 ≥12 个共享图标 |
| I2 | 编写主机端资源转换/检查/报告 CLI | 48×48 I4、24×24 A1 产物尺寸符合规则，可正确渲染 |
| I3 | App Package 集成和应用私有资源 ID 解析 | 应用可载入专属图标，恶意路径无法访问别的 App |
| I4 | Game Sprite + Runtime 生命周期 | 俄罗斯方块/声控飞行能够绘图，退出归还图片缓冲 |
| I5 | App Store 列表与多图标缓存压力测试 | 连续滚动、打开/退出 500 次无持续泄漏 |
| I6 | 与 Package Cache / OTA 的回归测试 | 清包不丢 App Data；回滚/重启不造成崩溃 |

**每个 Gate 都需要留下：代码提交、构建日志、自动测试结果、Flash 大小、free heap、最大可分配连续块；若无真机，必须标记硬件测试未执行，不得宣称通过。**

## 10. 示例和验收基准

至少交付：

1. `icon_gallery`：12 个系统图标、主题切换、缩放/重着色（仅对支持的格式），验证缺图回退。
2. `weather`：自定义 48×48 App Store 图标 + UI 天气 symbol + 天气 API 数据。
3. `tetris`：不依赖大图片的原生矩形绘制 + 可选 I4 小 Sprite，对比 Flash/RAM。
4. `voicebird`：带透明区域的 I4/I8 角色 Sprite + 实时麦克风事件，验证帧率。
5. `appstore_grid`：至少 30 个图标资源的滚动与加载压力测试。

尺寸和性能目标（**待真机验证，不是已有数据**）：

- 48×48 I4 图标未压缩像素载荷 1,152 B（调色板另算）；24×24 A1 为 72 B。
- 单图标包成本显著低于 RGB565 方案，测试报告显示实际差值。
- 图标加载不得引入超过预算的长期内存占用；项目自定初始图像缓存预算 8–16 KiB，根据 M0 内存测量修订。
- 简单 Launcher 滚动和俄罗斯方块更新无明显卡顿，统计真实绘制耗时及 dropped frame。
- 用户退出应用后释放其图片资源引用、VM、LVGL 对象；App Data 不丢失。

## 11. Codex 交付检查清单

- [ ] 真实实现的系统 Icon Registry、资源解码/加载器、应用路径隔离。
- [ ] `passport assets` 主机构建/检查/报告功能或等效 CLI。
- [ ] 文档化的格式选择规则和 Manifest 资源字段定义。
- [ ] 以上五种示例/回归场景。
- [ ] 边界输入、权限、缓存回收、版本更新、异常断电场景测试。
- [ ] 固件 `idf.py size-components` 与工具产物大小对比。
- [ ] 设备 RAM、图标解码峰值、重复运行泄漏报告。
- [ ] 明确区分主机自动测试 / 仿真 / 真机验证与阻塞项。
- [ ] 更新 Passport OS 主文档与 SDK，避免设计与实现分叉。

## 12. 参考资料

- AI Passport 官方仓库：<https://github.com/FoloToy/ai-passport>
- LVGL 颜色格式：<https://lvgl.io/docs/open/main-modules/images/color_formats>
- LVGL 图像添加及 BIN 离线转换：<https://lvgl.io/docs/open/9.5/main-modules/images/adding_images>
- LVGL Image Widget / 索引图像旋转约束：<https://github.com/lvgl/lvgl/blob/master/docs/src/widgets/image.mdx>

## 13. 交给 Codex 的启动提示词

> 阅读 `PASSPORT_OS_ICON_ASSET_SPEC_CODEX.md` 和现有 Passport OS 代码，先核实当前 LVGL 版本、图像加载路径与 RAM 预算，然后按 I0–I6 实现图标资源系统及主机 Asset Compiler。必须完成代码、示例、自动测试与真实尺寸报告。对无真机的功能明确标记未测试，不得仅写文档或声称未经执行的验收通过。保留现有 App Package / App Data 分离、Capability API 和安全权限模型。
