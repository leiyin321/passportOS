[简体中文](assets.zh_CN.md)

# Icons, sprites and streamed packages

The OS pins LVGL9.5.0. Image assets are compiled on a computer; no SVG/PNG/JPEG engine is added to
the device. Existing BSP partial DMA rendering and LVGL native BIN/image decoding are reused.

Install the optional compiler dependencies in your SDK virtual environment:

```sh
pip install -e './sdk[assets]'
passport assets build examples/weather/assets --manifest examples/weather/manifest.json --no-dither
passport pack examples/weather --key local/my-private.pem --public local/my-public.pem -o dist/weather.pap
passport assets report dist/weather.pap --public local/my-public.pem
passport assets inspect examples/weather/.passport-assets/icons/weather.pai --preview local/preview.png
```

Use your own new application ID and enroll your publisher key before device installation. Existing
IDs belong to their registered publisher even after offload. Original signed fixtures are included
under examples/packages for testing without a private key. Generated .passport-assets directories are disposable.

Manifest source example:

```json
{
  "icon":"app://icons/weather",
  "limits":{"script_heap_kib":32,"fps":20,"persistent_kib":4,"image_cache_kib":8},
  "assets":[{"id":"icons/weather","source":"assets/icon.png","width":48,"height":48,"encoding":"I4","kind":"app-icon"}]
}
```

This is an excerpt, not a complete manifest. `system.cloud` as icon needs no bundled image.
Each source belongs inside the app directory; symlink escape, absolute paths and traversal fail.
PNG/SVG sources are limited to256KiB, one frame and bounded source pixel count; output is1..96px
per side and at most16KiB per image. SVG accepts geometry only, with external references/entities
and embedded images denied. Host rasterization uses fixed CairoSVG2.8.2; Pillow11.3.0 handles resizing
and deterministic quantization. No-dither is the default. Do not assume arbitrary SVG browser support.

`auto` selects A1/A4 for masks and I4/I8 for colored images. Explicit formats: A1, A4, I4, I8, RGB565.
Indexed formats reserve transparent slot0 and retain alpha in the BGRA palette. I4 uses up to15 visible
colors plus transparency; I8 uses up to255. RGB565 requires opaque input. Alpha-only images use runtime
theme tint. Precompile required sizes/orientations: arbitrary indexed-image transforms are not supported.

PAI1 binary wrapper (little endian):

| Offset | Type | Meaning |
|---|---|---|
|0|4 bytes|ASCII PAI1|
|4|u8|1 A1, 2 A4, 3 I4, 4 I8, 5 RGB565|
|5,6|u8,u8|LVGL major9/minor5|
|7|u8|flags0; compression is forbidden|
|8,10|u16,u16|width/height|
|12|u16|exact row stride ceil(width*bpp/8)|
|14|u16|used palette entries, including slot0;0 for nonindexed|
|16|u32|payload bytes, including complete palette|
|20|u32|reserved0|
|24|bytes|palette followed by pixels, or mask/RGB565 pixels|

I4 always stores16 BGRA entries (64B), I8 stores256 (1024B). Slot0 is four zero bytes. Index values
must be below the used palette count. A1 starts at bit7, A4/I4 start at the high nibble; each row
starts at the declared exact stride. RGB565 is little endian. The OS validates all fields and builds
a real lv_image_dsc_t; it never passes this wrapper directly to LVGL.

PAP2 preserves the16-byte PAP1 envelope and RSA signature scheme, changing magic toPAP2. The
payload is `u32 entry_length + entry_bytes + concatenated PAI1 assets`. Manifest max12288B, code
max16384B, <=32 assets, asset bundle <=192KiB, total package <=256KiB. Asset records are sorted by
ID and contain id/offset/size/format/width/height/stride/palette_count/sha256. Offsets start at0 in the
asset region and must be contiguous; all bytes are indexed. The manifest `sha256` remains the entry
hash; the RSA signature covers the header, manifest, entry and every image byte. PAP1 still works.

Installation streams to Flash and validates signature plus all images before switching versions.
Launching rechecks the complete signed package sequentially, parses its bounded index/code and
validates individual image headers/hashes on demand. It does not read the whole bundle into RAM.
No path from a mini-app is opened as a file. `app://` resolves only the current signed manifest index.

```lua
passport.ui.icon("system.wifi",24) -- semantic ID; unknown IDs use system.app-default
passport.ui.icon("system.cloud",24,160,10,0x68d5e8)
passport.graphics.sprite("app://sprites/bird",25,80) -- returns false + placeholder on failure
```

Declarative nodes support `{"type":"icon","name":"system.cloud","size":24,"x":160,"y":10}`
and `{"type":"image","src":"app://icons/weather","width":48,"height":48,"x":100,"y":40}`.
Dimensions come from the verified resource, not an unchecked UI node. x/y refer to the216x180 canvas.
Symbols select the existing14/20px fonts; shared A1 masks are native24px. Theme recolor does not
create duplicate bitmaps. Native assets are rendered at compiled size; width/height are descriptive.

The current frame holds up to16 image commands. Encoded cache has12 slots,8KiB default and16KiB
maximum from Manifest. Visible handles stay pinned until frame presentation; LRU evicts only unpinned
entries. A six-icon grid uses16KiB so old/new six-icon pages coexist without a large decoded buffer.
Cache generations reject stale handles; app exit clears image references under the LVGL lock, then
frees owned buffers. LVGL image and header caches stay disabled; native indexed decoding uses one
ARGB8888 row. Assets are package cache, never App Data. Offload removes images/code together and
retains saved game progress. Missing/offloaded Store/Library icons use the shared placeholder.

Examples: icon_gallery (12 system IDs/theme/size/fallback+A1), weather (custom48px I4), tetris
(rectangles+optional16px sprite via long UP), voicebird (transparent sprite+microphone features),
appstore_grid (30 assets,6 visible). Simulator fonts differ; device decoder hash tests verify actual
palette/alpha semantics. See reports I0-I4 and I5-I6 for measured Flash, cache, decoder scratch and cleanup.

Validation:

```sh
python tests/assets/test_compiler.py
./tools/passport/test.sh
python tests/assets/hardware_assets.py
python tests/assets/hardware_render.py
```

Hardware scripts require explicit authorization and exclusive serial access; no flashing occurs in
these tests. `cancel_cycle` on the trusted USB channel cooperatively stops a long stress loop.
