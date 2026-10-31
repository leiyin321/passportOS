#include "icon_registry.h"
#include "system_masks.h"
#include <string.h>
static const ps_system_icon icons[] = {
    {"system.app-default", "\xef\x80\xbe", NULL, 0, 0},
    {"system.wifi", "\xef\x87\xab", NULL, 0, 0},
    {"system.battery", "\xef\x89\x80", NULL, 0, 0},
    {"system.back", "\xef\x81\x93", NULL, 0, 0},
    {"system.settings", "\xef\x80\x93", NULL, 0, 0},
    {"system.microphone", NULL, ps_mask_microphone, 24, 24},
    {"system.cloud", NULL, ps_mask_cloud, 24, 24},
    {"system.gamepad", NULL, ps_mask_gamepad, 24, 24},
    {"system.chat", NULL, ps_mask_chat, 24, 24},
    {"system.home", "\xef\x80\x95", NULL, 0, 0},
    {"system.download", "\xef\x80\x99", NULL, 0, 0},
    {"system.refresh", "\xef\x80\xa1", NULL, 0, 0},
    {"system.check", "\xef\x80\x8c", NULL, 0, 0},
    {"system.close", "\xef\x80\x8d", NULL, 0, 0}};
unsigned ps_icon_count(void) { return sizeof(icons) / sizeof(*icons); }
unsigned ps_icon_index(const char *id) {
  if (id)
    for (unsigned i = 0; i < ps_icon_count(); i++)
      if (!strcmp(icons[i].id, id))
        return i;
  return 0;
}
const ps_system_icon *ps_icon_get(unsigned index) {
  return &icons[index < ps_icon_count() ? index : 0];
}
#ifdef ESP_PLATFORM
_Static_assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 5,
               "Icon renderer audited for LVGL 9.5");
void ps_icon_draw(lv_layer_t *layer, unsigned index, int x, int y,
                  unsigned size, uint32_t color) {
  const ps_system_icon *icon = ps_icon_get(index);
  if (icon->symbol) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = icon->symbol;
    d.color = lv_color_hex(color);
    d.font = size <= 16 ? &lv_font_montserrat_14 : &lv_font_montserrat_20;
    lv_area_t a = {x, y, x + (int)size - 1, y + (int)size - 1};
    lv_draw_label(layer, &d, &a);
  } else {
    static lv_image_dsc_t masks[14];
    lv_image_dsc_t *image = &masks[index < 14 ? index : 0];
    if (!image->data) {
      *image = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                           .cf = LV_COLOR_FORMAT_A1,
                                           .w = 24,
                                           .h = 24,
                                           .stride = 3},
                                .data_size = 72,
                                .data = icon->alpha};
    }
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = image;
    d.recolor = lv_color_hex(color);
    d.recolor_opa = LV_OPA_COVER;
    lv_area_t a = {x, y, x + 23, y + 23};
    lv_draw_image(layer, &d, &a);
  }
}
#endif
