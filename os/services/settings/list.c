#include "ps_settings_list.h"

unsigned ps_settings_scroll_base(unsigned item_count, unsigned selected,
                                 unsigned visible_count, unsigned current_base) {
  if (!item_count || !visible_count) return 0;
  if (selected >= item_count) selected = item_count - 1;
  if (visible_count >= item_count) return 0;
  unsigned max_base = item_count - visible_count;
  if (current_base > max_base) current_base = max_base;
  if (selected < current_base) current_base = selected;
  else if (selected >= current_base + visible_count)
    current_base = selected - visible_count + 1;
  return current_base;
}
