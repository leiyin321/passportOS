#include "asset_format.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t u32(const uint8_t *p) {
  return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
bool ps_asset_id_valid(const char *id) {
  if (!id || !*id || strlen(id) > 48)
    return false;
  bool separator = true;
  for (; *id; id++) {
    bool word = (*id >= 'a' && *id <= 'z') || (*id >= '0' && *id <= '9');
    if (!word) {
      if ((*id != '/' && *id != '_' && *id != '-') || separator)
        return false;
      separator = true;
    } else
      separator = false;
  }
  return !separator;
}
bool ps_asset_header(const void *header, size_t total, ps_asset_info *info) {
  if (!header || total < 24 || total > PS_ASSET_MAX_BYTES)
    return false;
  const uint8_t *p = header;
  if (memcmp(p, "PAI1", 4) || p[5] != 9 || p[6] != 5 || p[7] || u32(p + 20))
    return false;
  unsigned fmt = p[4], w = u16(p + 8), h = u16(p + 10), stride = u16(p + 12),
           colors = u16(p + 14);
  uint32_t size = u32(p + 16);
  if (fmt < PS_A1 || fmt > PS_RGB565 || !w || !h || w > 96 || h > 96)
    return false;
  unsigned bits = fmt == PS_A1                   ? 1
                  : fmt == PS_A4 || fmt == PS_I4 ? 4
                  : fmt == PS_I8                 ? 8
                                                 : 16;
  unsigned slots = fmt == PS_I4 ? 16 : fmt == PS_I8 ? 256 : 0;
  uint64_t expected = (uint64_t)((w * bits + 7) / 8) * h + slots * 4;
  if (stride != (w * bits + 7) / 8 || size != expected ||
      total != 24 + expected ||
      (slots ? (!colors || colors > slots) : colors != 0))
    return false;
  *info = (ps_asset_info){w, h, stride, colors, fmt, size};
  return true;
}
bool ps_asset_validate(const void *data, size_t size, ps_asset_info *info) {
  if (!ps_asset_header(data, size, info))
    return false;
  const uint8_t *p = data;
  if (info->format == PS_I4 || info->format == PS_I8) {
    unsigned slots = info->format == PS_I4 ? 16 : 256;
    if (p[24] || p[25] || p[26] || p[27])
      return false;
    p += 24 + slots * 4;
    for (unsigned y = 0; y < info->height; y++)
      for (unsigned x = 0; x < info->width; x++) {
        unsigned index =
            info->format == PS_I8
                ? p[y * info->stride + x]
                : (p[y * info->stride + x / 2] >> (x % 2 ? 0 : 4)) & 15;
        if (index >= info->colors)
          return false;
      }
  }
  return true;
}
