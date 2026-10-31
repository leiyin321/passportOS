<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, sound effects, and battery profiles, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

`fonts/passport_cjk_extra_16.c` is the generated 16 px, 4 bpp Source Han Sans SC
subset used for Chinese system UI glyphs missing from LVGL's built-in CJK subset.
The built-in LVGL subset is chained as its fallback, followed by Montserrat 14.
The source font is LVGL's managed `SourceHanSansSC-Normal.otf`; it is distributed
under the SIL Open Font License 1.1, copied to
[`fonts/SourceHanSansSC-OFL-1.1.txt`](fonts/SourceHanSansSC-OFL-1.1.txt).
The generated subset contains 145 system UI code points. It explicitly includes
U+53CC/U+51FB (double-click), U+7F16/U+8F91 (the Chinese “editing” label),
U+95F9 (alarm), U+89C2 (appearance), and U+8BFB (read) so
the Home hint does not depend on the built-in CJK subset. In the ESP-IDF 5.5.3
release ELF, its linked descriptor, cmap, glyph descriptors, Unicode list, and
bitmap occupy 16,932 bytes of Flash in build 1.0.15.
Regenerate with `lv_font_conv` 1.5.3 using `--size 16
--bpp 4 --no-compress --no-prefilter --symbols <missing-codepoints> --font
<LVGL>/scripts/built_in_font/SourceHanSansSC-Normal.otf --format lvgl
--lv-font-name passport_cjk_extra_16 --lv-include lvgl.h --force-fast-kern-format`.
The generated C source is compiled by `main/CMakeLists.txt`.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

## Battery profiles

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
contains the supplied CW2017 profile for the Wanli New Energy 500 mAh cell
(profile2, XIL40X, 2026-10-20). It is an unchanged copy of the project-supplied
file, retained at the maintainer's request for this firmware update. Its
original filename is recorded in the paired Chinese document. No separate
license was included in the supplied file.

The format is 80 lines of two hexadecimal digits, one byte per line in register
order. The BSP embeds those bytes in `components/bsp/src/bsp_battery.c`; the
host gate checks the source file and initializer for an exact match. Text-file
SHA-256: `7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`.
Decoded 80-byte SHA-256:
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`.
This is a fuel-gauge profile, not a charging configuration or a generic profile
for every 500 mAh cell; see the [hardware guide](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md#9-cw2017-fuel-gauge).

`fonts/passport_cjk_chat_16.c` is the shared 16 px/2 bpp LVGL RLE-compressed font. It preserves the previous built-in Source Han CJK/Latin/FontAwesome inventory and GB2312 level 1 plus conversational characters, excluding glyphs supplied by the existing UI supplement. The duplicate built-in font is disabled; the fallback chain is UI supplement → shared font → Montserrat 14, independent of app/system language. Regenerate with `python3 tools/passport/build_chat_font.py --converter <lv_font_conv-1.5.3>`. The generator uses the existing OFL Source Han and LVGL FontAwesome sources/licenses. `CONFIG_LV_USE_FONT_COMPRESSED=y` is required; decompression uses LVGL's bounded draw-buffer/font path. Keep the previous-inventory and new app-text coverage tests. Absolute Flash costs in earlier reports refer to their exact historical binaries; current costs are measured in the Bluetooth acceptance report.

The shared font also retains the previous FontAwesome 5 glyphs under the [bundled FontAwesome license](fonts/FontAwesome5-LICENSE.txt).

The shared font generator also includes maintained Native application labels. City Radio adds one level-2 glyph for Shenzhen; all application text is checked against the shared font inventory.
