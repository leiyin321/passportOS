#pragma once
#include <stdbool.h>
#include "storage_layout.h"
#include <stddef.h>
typedef struct {
  char id[64];
  size_t package, previous, data_entries;
  bool data_valid;
} ps_storage_app_usage;
typedef struct {
  size_t total, used, data_used, data_available, data_total;
  bool packages_valid, data_valid;
  ps_storage_layout layout;
  bool layout_valid;
  unsigned count;
  ps_storage_app_usage apps[12];
} ps_storage_usage;
/* Background snapshot; never accesses LVGL. One request at a time. */
bool ps_storage_usage_request(void);
/* Main-task consumer. Returns a completed snapshot at most once. */
bool ps_storage_usage_take(ps_storage_usage *out);
