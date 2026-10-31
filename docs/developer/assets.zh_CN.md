[English](assets.md)

# 图标、精灵及流式资源包

OS 精确锁定 LVGL9.5.0。图片只在电脑端转换；设备不新增 SVG/PNG/JPEG 解码器，复用 BSP 部分 DMA 缓冲及 LVGL 内置解码。

```sh
pip install -e './sdk[assets]'
passport assets build examples/weather/assets --manifest examples/weather/manifest.json --no-dither
passport pack examples/weather --key local/my-private.pem --public local/my-public.pem -o dist/weather.pap
passport assets report dist/weather.pap --public local/my-public.pem
passport assets inspect examples/weather/.passport-assets/icons/weather.pai --preview local/preview.png
```

新开发者须使用自己的 App ID 并先注册发布者公钥；Offload 后原 ID 仍属于原发布者。
examples/packages 提供不需要私钥的已签名测试包；生成的 .passport-assets 可随时删除重建。

Manifest 片段（非完整文件）：

```json
{
  "icon":"app://icons/weather",
  "limits":{"script_heap_kib":32,"fps":20,"persistent_kib":4,"image_cache_kib":8},
  "assets":[{"id":"icons/weather","source":"assets/icon.png","width":48,"height":48,"encoding":"I4","kind":"app-icon"}]
}
```

引用 `system.cloud` 不需要打包图像。源文件必须在应用目录内，拒绝绝对路径、穿越及符号链接逃逸。
源 PNG/SVG 至多256KiB、单帧且限制源像素数；输出每边1..96像素，每图至多16KiB。
SVG 只接受几何子集，禁止外部引用/实体及嵌入图。固定 CairoSVG2.8.2 栅格化、Pillow11.3.0 缩放量化，默认无抖动。
不承诺任意浏览器SVG语法。

auto 为遮罩选择A1/A4、彩色图选择I4/I8；也可显式指定RGB565。索引0保留透明，BGRA调色板保存alpha。
I4最多15种可见色+透明；I8最多255种+透明。RGB565只接受不透明输入。
Alpha图运行时调色，不生成明暗主题副本；需要不同尺寸/方向时预生成，不支持任意索引图变换。

PAI1 小端二进制包装：

| 偏移 | 类型 | 含义 |
|---|---|---|
|0|4字节|ASCII PAI1|
|4|u8|1 A1、2 A4、3 I4、4 I8、5 RGB565|
|5,6|u8,u8|LVGL主版本9/次版本5|
|7|u8|flags必须0，禁止压缩|
|8,10|u16,u16|宽、高|
|12|u16|精确扫描行stride=ceil(width*bpp/8)|
|14|u16|含透明索引0的已用调色板数；非索引图为0|
|16|u32|含完整调色板的载荷字节数|
|20|u32|保留0|
|24|字节|调色板+像素，或遮罩/RGB565像素|

I4固定16项BGRA（64字节）、I8固定256项（1024字节）；索引0四字节全0，像素索引不得超过已用色数。
A1从bit7开始，A4/I4从高半字节开始，各行按精确stride独立打包。RGB565小端。
OS 校验后构建真正的lv_image_dsc_t，不直接把自定义头交给LVGL。

PAP2保留PAP1的16字节封装与RSA签名方案，magic改为PAP2。载荷为
`u32入口长度 + 入口字节 + 连续PAI1资源`。Manifest≤12288字节、代码≤16384字节、资源≤32个、
资源区≤192KiB、整包≤256KiB。资源索引按ID排序，包含id/offset/size/format/width/height/stride/palette_count/sha256。
偏移相对资源区从0连续排列，所有字节必须被索引。Manifest.sha256仍是代码哈希，RSA覆盖头、Manifest、代码和全部资源。
PAP1保持兼容。

安装先流式写Flash，完整校验签名及每张图片再切换。启动顺序读取验证整个签名包，仅读取有界代码/索引，
图片按需再次校验头/哈希；不整包读入RAM。应用传入的路径不会作为文件名打开，app://只查当前签名Manifest。

```lua
passport.ui.icon("system.wifi",24) -- 未知ID回退system.app-default
passport.ui.icon("system.cloud",24,160,10,0x68d5e8)
passport.graphics.sprite("app://sprites/bird",25,80) -- 失败返回false并显示占位图
```

声明式支持icon/image节点及x/y；坐标在216×180画布内，尺寸来自验证后的资源。
Symbol复用14/20像素字体，A1系统遮罩固定24像素；不支持的缩放不会申请大缓冲。
应用图片按编译尺寸显示，节点width/height只作描述。

每帧最多16条图片指令。编码缓存12槽，默认8KiB、Manifest最多16KiB；当前显示资源保持pin，
只有未显示项可被LRU回收。六图网格用16KiB容纳前后两页，避免整图解码。
代际句柄拒绝过期引用；退出时在LVGL锁内清空显示引用并释放缓存。LVGL自身图片/头缓存关闭，索引解码仅用一行ARGB8888。
图像属于代码缓存，不属于App Data；Offload不删进度，Store/Library缺图时显示共享占位。

示例：icon_gallery（12系统ID/主题/尺寸/回退+A1）、weather（48像素I4）、tetris（矩形+长按UP开关小精灵）、
voicebird（透明精灵+麦克风）、appstore_grid（30资源、6可见）。模拟器字体不同，真机解码哈希用于验证真实颜色/透明度。
测量结果见I0-I4、I5-I6报告。

```sh
python tests/assets/test_compiler.py
./tools/passport/test.sh
python tests/assets/hardware_assets.py
python tests/assets/hardware_render.py
```

硬件测试须授权且串口独占，测试脚本不刷写。可信USB命令cancel_cycle可协作取消长时间压力循环。
