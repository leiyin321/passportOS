#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
typedef enum { PS_DISPLAY_STANDARD = 0, PS_DISPLAY_FULLSCREEN = 1 } ps_display_mode;
typedef struct { uint16_t x, y, width, height; } ps_display_rect;
typedef struct {
  ps_display_mode mode;
  uint16_t width, height;
  uint8_t safe_left, safe_top, safe_right, safe_bottom;
  ps_display_rect viewport, status_bar, navigation_bar;
  const char *navigation;
} ps_display_layout;
bool ps_display_layout_read(const cJSON *manifest, ps_display_layout *layout);
void ps_display_layout_default(ps_display_layout *layout, ps_display_mode mode);
bool ps_display_clip(const ps_display_layout *layout, int x, int y, int width,
                     int height, ps_display_rect *clipped);
