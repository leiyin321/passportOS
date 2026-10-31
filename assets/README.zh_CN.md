<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐、电池参数等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

`fonts/passport_cjk_extra_16.c` 是系统中文界面使用的思源黑体 16 px、4 bpp
字形子集，用于补足 LVGL 内置 CJK 子集缺失的字形。字体回退链依次连接 LVGL
内置 CJK 子集和 Montserrat 14。源字体来自 LVGL 托管的
`SourceHanSansSC-Normal.otf`，采用 SIL Open Font License 1.1，许可文件见
[`fonts/SourceHanSansSC-OFL-1.1.txt`](fonts/SourceHanSansSC-OFL-1.1.txt)。
生成子集含 145 个系统界面码点，并明确包含 U+53CC/U+51FB（“双击”）、U+7F16/U+8F91
（“编辑”）、U+95F9（“闹”）、U+89C2（“观”）和 U+8BFB（“读”），不再依赖 LVGL 内置 CJK 子集的覆盖范围。ESP-IDF 5.5.3 发布 ELF 中的字体
Flash 占用 16,932 字节（固件 1.0.15）。使用 `lv_font_conv` 1.5.3 重新生成时，
参数为 `--size 16 --bpp 4 --no-compress --no-prefilter --symbols <缺失码点>
--font <LVGL>/scripts/built_in_font/SourceHanSansSC-Normal.otf --format lvgl
--lv-font-name passport_cjk_extra_16 --lv-include lvgl.h --force-fast-kern-format`。
生成的 C 源码由 `main/CMakeLists.txt` 编译进固件。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## 电池参数（battery）

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
保存万锂新能源 500 mAh 电芯的 CW2017 参数（profile2、XIL40X、2026-10-20）。
该文件是项目提供的 `喜梨_智能工卡_万锂新能源500mAh_profile2_XIL40X_20261020.txt`
的原样副本，按维护者要求保留用于本次固件更新；原文件没有附带单独的许可说明。

格式为 80 行两位十六进制数字，每行一个字节，按寄存器顺序排列。
BSP 在 `components/bsp/src/bsp_battery.c` 中嵌入这 80 字节，主机检查会验证
源文件及数组内容完全一致。文本文件 SHA-256：
`7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`。
解码后 80 字节 SHA-256：
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`。
这是电量计参数，不是充电配置，也不能通用于所有 500 mAh 电芯；详见
[硬件指南](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md#9-cw2017-电池计)。

聊天补充字库 `fonts/passport_cjk_chat_16.c` 为共享 16 px／2 bpp LVGL RLE 压缩字体。保留此前内置 Source Han 的汉字／拉丁字母／FontAwesome 字符及 GB2312 一级与聊天字符，剔除已有 UI 补充字体中的重复字形。关闭重复内置字体，字体链为 UI 补充 → 共享字体 → Montserrat 14，不依赖应用／系统语言。使用 `python3 tools/passport/build_chat_font.py --converter <lv_font_conv-1.5.3>` 生成，沿用已有 OFL Source Han 及 LVGL FontAwesome 字体来源／许可。需要 `CONFIG_LV_USE_FONT_COMPRESSED=y`，使用 LVGL 有界绘制缓冲／字体解压路径。保留旧字集及新增应用文本覆盖测试。此前报告的 Flash 数值对应其历史精确构建，当前占用见蓝牙验收报告。

共享字体保留此前 FontAwesome 5 字形，许可见[随附 FontAwesome 许可](fonts/FontAwesome5-LICENSE.txt)。

共享字体生成器也收集维护中的 Native 应用文字；城市电台补入深圳的二级汉字“圳”，所有应用文字均经过共享字形集合检查。
