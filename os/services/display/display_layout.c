#include "display_layout.h"
#include <string.h>
void ps_display_layout_default(ps_display_layout *o, ps_display_mode mode) {
  if (!o) return;
  *o = (ps_display_layout){.mode = mode,
                           .width = 240,
                           .height = 320,
                           .viewport = {0, mode == PS_DISPLAY_STANDARD ? 24 : 0,
                                        240, mode == PS_DISPLAY_STANDARD ? 272 : 320},
                           .status_bar = {0, 0, 240,
                                          mode == PS_DISPLAY_STANDARD ? 24 : 0},
                           .navigation_bar = {0,
                                              mode == PS_DISPLAY_STANDARD ? 296 : 320,
                                              240,
                                              mode == PS_DISPLAY_STANDARD ? 24 : 0},
                           .navigation = "buttons"};
}
bool ps_display_layout_read(const cJSON *manifest, ps_display_layout *out) {
  if (!out) return false;
  const cJSON *d = cJSON_GetObjectItemCaseSensitive(manifest, "display");
  ps_display_mode mode = PS_DISPLAY_STANDARD;
  const char *navigation = "buttons";
  if (d) {
    if (!cJSON_IsObject(d)) return false;
    for (const cJSON *field = d->child; field; field = field->next) {
      if (strcmp(field->string, "mode") && strcmp(field->string, "orientation") &&
          strcmp(field->string, "navigation") && strcmp(field->string, "design_width") &&
          strcmp(field->string, "design_height")) return false;
    }
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(d, "mode");
    if (m && !cJSON_IsString(m)) return false;
    if (m && !strcmp(m->valuestring, "fullscreen")) mode = PS_DISPLAY_FULLSCREEN;
    else if (m && strcmp(m->valuestring, "standard")) return false;
    const cJSON *o = cJSON_GetObjectItemCaseSensitive(d, "orientation");
    if (o && (!cJSON_IsString(o) || strcmp(o->valuestring, "portrait"))) return false;
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(d, "navigation");
    if (n) {
      if (!cJSON_IsString(n) || (strcmp(n->valuestring, "buttons") &&
          strcmp(n->valuestring, "scroll") && strcmp(n->valuestring, "select") &&
          strcmp(n->valuestring, "game"))) return false;
      navigation = n->valuestring;
    }
    const char *dimensions[] = {"design_width", "design_height"};
    for (unsigned i = 0; i < 2; i++) {
      const cJSON *v = cJSON_GetObjectItemCaseSensitive(d, dimensions[i]);
      if (v && (!cJSON_IsNumber(v) || v->valuedouble != v->valueint ||
                v->valueint < 1 || v->valueint > (i ? 320 : 240))) return false;
    }
  }
  ps_display_layout_default(out, mode);
  out->navigation = navigation;
  return true;
}
bool ps_display_clip(const ps_display_layout *l, int x, int y, int w, int h,
                     ps_display_rect *out) {
  if (!l || !out || w <= 0 || h <= 0) return false;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int64_t x1 = (int64_t)x + w, y1 = (int64_t)y + h;
  if (x1 > l->viewport.width) x1 = l->viewport.width;
  if (y1 > l->viewport.height) y1 = l->viewport.height;
  if (x0 >= x1 || y0 >= y1) return false;
  *out = (ps_display_rect){x0, y0, x1 - x0, y1 - y0};
  return true;
}
