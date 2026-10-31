#pragma once
#include <stdint.h>
int ps_native_json_get(const void *json, uint32_t bytes, const char *path,
                       char *out, uint32_t capacity);
