#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
  const char *id;
  const char *symbol;
  const uint8_t *alpha;
  uint8_t width, height;
} ps_system_icon;
unsigned ps_icon_count(void);
unsigned ps_icon_index(const char *id);
const ps_system_icon *ps_icon_get(unsigned index);
#ifdef ESP_PLATFORM
#include "lvgl.h"
void ps_icon_draw(lv_layer_t *layer, unsigned index, int x, int y,
                  unsigned size, uint32_t color);
#endif
