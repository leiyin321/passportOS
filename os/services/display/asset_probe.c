#include "asset_probe.h"
#include "asset_loader.h"
#include "icon_registry.h"
#include "lvgl.h"
#include "mbedtls/sha256.h"
#include "src/draw/lv_image_decoder_private.h"
#include <stdio.h>
#include <string.h>
// Diagnostic adapter deliberately pinned to the audited 9.5 decoder descriptor
// ABI.
_Static_assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 5,
               "Asset probe requires LVGL 9.5");
bool ps_asset_probe(uint32_t token, char hash[65], unsigned *rows,
                    unsigned *scratch) {
  const lv_image_dsc_t *image = ps_asset_lvgl(token);
  if (!image)
    return false;
  lv_mem_monitor_t memory;
  lv_mem_monitor(&memory);
  size_t before = memory.free_size, minimum = before;
  lv_image_decoder_dsc_t d = {0};
  lv_image_decoder_args_t args = {
      .no_cache = true, .premultiply = false, .use_indexed = false};
  if (lv_image_decoder_open(&d, image, &args) != LV_RESULT_OK)
    return false;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  *rows = 0;
  bool ok = true;
  if (d.decoded) {
    unsigned bpp = lv_color_format_get_bpp(d.decoded->header.cf);
    size_t bytes = (image->header.w * bpp + 7) / 8;
    for (unsigned y = 0; y < image->header.h; y++) {
      mbedtls_sha256_update(
          &sha, d.decoded->data + y * d.decoded->header.stride, bytes);
      (*rows)++;
    }
    lv_mem_monitor(&memory);
    if (memory.free_size < minimum)
      minimum = memory.free_size;
  } else {
    lv_area_t full = {0, 0, image->header.w - 1, image->header.h - 1},
              part = {LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN, LV_COORD_MIN};
    while (*rows < image->header.h) {
      if (lv_image_decoder_get_area(&d, &full, &part) != LV_RESULT_OK ||
          !d.decoded) {
        ok = false;
        break;
      }
      unsigned bpp = lv_color_format_get_bpp(d.decoded->header.cf);
      size_t bytes = (image->header.w * bpp + 7) / 8;
      mbedtls_sha256_update(&sha, d.decoded->data, bytes);
      (*rows)++;
      lv_mem_monitor(&memory);
      if (memory.free_size < minimum)
        minimum = memory.free_size;
    }
  }
  unsigned char digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  lv_image_decoder_close(&d);
  for (int i = 0; i < 32; i++)
    sprintf(hash + i * 2, "%02x", digest[i]);
  *scratch = before - minimum;
  return ok;
}
bool ps_icon_fonts_ready(void) {
  for (unsigned i = 0; i < ps_icon_count(); i++) {
    const ps_system_icon *icon = ps_icon_get(i);
    if (!icon->symbol)
      continue;
    const uint8_t *s = (const uint8_t *)icon->symbol;
    uint32_t code = (s[0] & 15u) << 12 | (s[1] & 63u) << 6 | (s[2] & 63u);
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(&lv_font_montserrat_14, &g, code, 0) ||
        !lv_font_get_glyph_dsc(&lv_font_montserrat_20, &g, code, 0))
      return false;
  }
  return true;
}
