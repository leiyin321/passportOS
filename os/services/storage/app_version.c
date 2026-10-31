#include "app_version.h"
#include <stdint.h>
#include <stdlib.h>

static int parse(const char *text, uint32_t parts[3]) {
  if (!text || !*text) return 0;
  for (int i = 0; i < 3; ++i) {
    if (*text < '0' || *text > '9') return 0;
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (end == text || value > UINT32_MAX) return 0;
    parts[i] = (uint32_t)value;
    if (i < 2) {
      if (*end != '.') return 0;
      text = end + 1;
    } else if (*end) {
      return 0;
    }
  }
  return 1;
}

int ps_app_version_compare(const char *installed, const char *catalog) {
  uint32_t a[3], b[3];
  if (!parse(installed, a) || !parse(catalog, b)) return -2;
  for (int i = 0; i < 3; ++i) {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}
