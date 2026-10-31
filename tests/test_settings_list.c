#include "ps_settings_list.h"

#include <assert.h>

int main(void) {
  assert(ps_settings_scroll_base(12, 0, 8, 0) == 0);
  assert(ps_settings_scroll_base(12, 7, 8, 0) == 0);
  assert(ps_settings_scroll_base(12, 8, 8, 0) == 1);
  assert(ps_settings_scroll_base(12, 11, 8, 1) == 4);
  assert(ps_settings_scroll_base(12, 2, 8, 4) == 2);
  assert(ps_settings_scroll_base(5, 99, 8, 0) == 0);
  assert(ps_settings_scroll_base(0, 0, 8, 0) == 0);
  assert(ps_settings_scroll_base(12, 10, 0, 0) == 0);
  return 0;
}
