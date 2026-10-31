#pragma once
#include "asset_format.h"
#include "package.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PS_ASSET_SLOTS 12
typedef struct {
  size_t current, peak;
  unsigned loads, failures;
  uint64_t load_us, max_load_us;
} ps_asset_metrics;
void ps_assets_budget(size_t bytes);
uint32_t ps_asset_open(const ps_package *package, const char *uri);
bool ps_asset_get(uint32_t token, ps_asset_info *info);
const char *ps_asset_name(uint32_t token);
void ps_assets_unpin(void);
void ps_asset_pin(uint32_t token);
void ps_assets_close(void);
const ps_asset_metrics *ps_assets_metrics(void);
const void *ps_asset_lvgl(uint32_t token);
const uint8_t *ps_asset_encoded(uint32_t token, size_t *size);
