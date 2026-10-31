#include "display_layout.h"
#include <assert.h>
#include <string.h>
int main(void) {
  ps_display_layout l;
  cJSON *m = cJSON_Parse("{}");
  assert(ps_display_layout_read(m, &l));
  assert(l.mode == PS_DISPLAY_STANDARD && l.viewport.x == 0 &&
         l.viewport.y == 24 && l.viewport.width == 240 &&
         l.viewport.height == 272 && l.status_bar.height == 24 &&
         l.navigation_bar.y == 296 && l.navigation_bar.height == 24);
  cJSON_Delete(m);
  m = cJSON_Parse("{\"display\":{\"mode\":\"fullscreen\",\"orientation\":\"portrait\",\"navigation\":\"game\"}}");
  assert(ps_display_layout_read(m, &l));
  assert(l.mode == PS_DISPLAY_FULLSCREEN && l.viewport.y == 0 &&
         l.viewport.height == 320 && l.status_bar.height == 0 &&
         l.navigation_bar.height == 0 && !strcmp(l.navigation, "game"));
  ps_display_rect r;
  assert(ps_display_clip(&l, -8, 12, 32, 20, &r));
  assert(r.x == 0 && r.y == 12 && r.width == 24 && r.height == 20);
  assert(!ps_display_clip(&l, 240, 0, 1, 1, &r));
  cJSON_Delete(m);
  m = cJSON_Parse("{\"display\":{\"mode\":\"rotate\"}}");
  assert(!ps_display_layout_read(m, &l));
  cJSON_Delete(m);
  m = cJSON_Parse("{\"display\":{\"orientation\":\"landscape\"}}");
  assert(!ps_display_layout_read(m, &l));
  cJSON_Delete(m);
  m = cJSON_Parse("{\"display\":{\"unsupported\":true}}");
  assert(!ps_display_layout_read(m, &l));
  cJSON_Delete(m);
  return 0;
}
