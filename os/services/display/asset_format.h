#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PS_ASSET_MAX_BYTES 16384
#define PS_ASSET_MAX_DIM 96
enum { PS_A1 = 1, PS_A4, PS_I4, PS_I8, PS_RGB565 };
typedef struct {
  uint16_t width, height, stride, colors;
  uint8_t format;
  uint32_t payload_size;
} ps_asset_info;
bool ps_asset_id_valid(const char *id);
bool ps_asset_header(const void *header, size_t total, ps_asset_info *info);
bool ps_asset_validate(const void *data, size_t size, ps_asset_info *info);
