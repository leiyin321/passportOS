#include "asset_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  const char *names[] = {"a1", "a4", "i4", "i8", "bird", "rgb565"};
  for (unsigned i = 0; i < 6; i++) {
    char path[128];
    snprintf(path, sizeof(path), "tests/assets/fixtures/%s.pai", names[i]);
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    unsigned char *data = malloc(n);
    assert(fread(data, 1, n, f) == (size_t)n);
    fclose(f);
    ps_asset_info info;
    assert(ps_asset_validate(data, n, &info));
    assert(!ps_asset_validate(data, n - 1, &info));
    unsigned char saved = data[7];
    data[7] = 1;
    assert(!ps_asset_validate(data, n, &info));
    data[7] = saved;
    data[8] = 255;
    data[9] = 255;
    assert(!ps_asset_validate(data, n, &info));
    free(data);
  }
  assert(ps_asset_id_valid("icons/weather"));
  const char *bad[] = {"../other",      "icons/../other", "file://foo",
                       "/absolute",     "icons//foo",     "icons/",
                       "app://icons/a", "other.app/icon"};
  for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++)
    assert(!ps_asset_id_valid(bad[i]));
  puts("I2/I3 PASS exact lengths, all five encodings, huge dimensions, "
       "compression flags and path isolation");
}
