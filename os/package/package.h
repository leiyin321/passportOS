#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PS_PAP_MAX 262144
#define PS_CODE_MAX 16384
#define PS_NATIVE_FILE_MAX 32768
typedef struct {
  cJSON *manifest;
  char *code;
  size_t code_size;
  char id[64];
  char identity[16];
  char runtime[16];
  unsigned permissions;
  char package_path[96];
  uint32_t asset_base, asset_bytes;
  unsigned char package_hash[32];
} ps_package;
enum {
  PS_UI = 1,
  PS_GRAPHICS = 2,
  PS_INPUT = 4,
  PS_TIMER = 8,
  PS_STORAGE = 16,
  PS_FETCH = 32,
  PS_WS = 64,
  PS_AUDIO = 128,
  PS_AUDIO_STREAM = 256,
  PS_DEVICE_IDENTITY = 512,
  PS_BLE_HID = 1024,
  PS_AUDIO_PLAYBACK = 2048,
  PS_AUDIO_HTTP = 4096
};
bool ps_package_parse(const void *data, size_t size, ps_package *out);
void ps_package_free(ps_package *p);
bool ps_json_safe(const cJSON *j, unsigned depth);

cJSON *ps_json_parse(const char *raw, size_t length);

bool ps_package_file(const char *path, ps_package *out);
bool ps_package_file_hash(const char *path, char hex[65]);
cJSON *ps_json_parse_budget(const char *raw, size_t length, unsigned tokens);

bool ps_package_file_lazy(const char *path, ps_package *out);

bool ps_json_preflight(const char *raw, size_t size, unsigned max_depth,
                       unsigned max_values);
