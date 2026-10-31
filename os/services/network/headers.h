#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PS_HEADERS_BYTES 1024
#define PS_HEADERS_COUNT 8
/* Owned compact storage. Offsets remain valid when the struct is copied. */
typedef struct {
  char data[PS_HEADERS_BYTES + 1];
  uint16_t names[PS_HEADERS_COUNT], values[PS_HEADERS_COUNT];
  unsigned count;
} ps_headers;
bool ps_headers_parse(ps_headers *out, const char *raw, size_t size, unsigned max_count);
bool ps_headers_format(const ps_headers *headers, char *out, size_t capacity, size_t *size);
