#include "asset_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "lvgl.h"
#include "mbedtls/sha256.h"
static uint64_t clock_us(void) { return esp_timer_get_time(); }
static void sha256(const void *p, size_t n, unsigned char out[32]) {
  mbedtls_sha256(p, n, out, 0);
}
#else
#include <time.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
static void sha256(const void *p, size_t n, unsigned char out[32]) {
  CC_SHA256(p, (CC_LONG)n, out);
}
#else
#include <openssl/sha.h>
static void sha256(const void *p, size_t n, unsigned char out[32]) {
  SHA256(p, n, out);
}
#endif
static uint64_t clock_us(void) {
  return (uint64_t)clock() * 1000000 / CLOCKS_PER_SEC;
}
#endif
typedef struct {
  char identity[16], name[49];
  uint8_t *data;
  size_t bytes;
  uint32_t token, access;
  unsigned char hash[32];
  bool pinned;
  ps_asset_info info;
#ifdef ESP_PLATFORM
  lv_image_dsc_t descriptor;
#endif
} entry;
static entry cache[PS_ASSET_SLOTS];
static size_t budget = 8192;
static uint32_t generation, access_counter;
static ps_asset_metrics metrics;
static FILE *resource_file;
static char resource_path[96];
static unsigned char resource_hash[32];
static size_t resource_position;
static entry *find(uint32_t token) {
  unsigned i = (token & 255);
  if (!i || i > PS_ASSET_SLOTS)
    return NULL;
  entry *e = &cache[i - 1];
  return e->data && e->token == token ? e : NULL;
}
static void drop(entry *e) {
  if (e->data) {
    metrics.current -= e->bytes;
    free(e->data);
  }
  memset(e, 0, sizeof(*e));
}
void ps_assets_close(void) {
  if (resource_file) {
    fclose(resource_file);
    resource_file = NULL;
  }
  resource_path[0] = 0;
  resource_position = 0;
  for (unsigned i = 0; i < PS_ASSET_SLOTS; i++)
    drop(&cache[i]);
}
void ps_assets_budget(size_t bytes) {
  budget = bytes < 1024 ? 1024 : bytes > 16384 ? 16384 : bytes;
  metrics.peak = metrics.current;
  metrics.loads = metrics.failures = 0;
  metrics.load_us = metrics.max_load_us = 0;
}
void ps_assets_unpin(void) {
  for (unsigned i = 0; i < PS_ASSET_SLOTS; i++)
    cache[i].pinned = false;
}
void ps_asset_pin(uint32_t token) {
  entry *e = find(token);
  if (e)
    e->pinned = true;
}
const ps_asset_metrics *ps_assets_metrics(void) { return &metrics; }
bool ps_asset_get(uint32_t token, ps_asset_info *info) {
  entry *e = find(token);
  if (!e)
    return false;
  if (info)
    *info = e->info;
  return true;
}
const char *ps_asset_name(uint32_t token) {
  entry *e = find(token);
  return e ? e->name : "";
}
const uint8_t *ps_asset_encoded(uint32_t token, size_t *size) {
  entry *e = find(token);
  if (!e)
    return NULL;
  *size = e->bytes;
  return e->data;
}
uint32_t ps_asset_open(const ps_package *package, const char *uri) {
  uint64_t started = clock_us();
  if (!package || !uri || strncmp(uri, "app://", 6) ||
      !ps_asset_id_valid(uri + 6) || !*package->package_path)
    goto fail;
  const char *id = uri + 6;
  cJSON *assets = cJSON_GetObjectItemCaseSensitive(package->manifest, "assets"),
        *record = NULL;
  for (cJSON *r = assets ? assets->child : NULL; r; r = r->next) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(r, "id");
    if (cJSON_IsString(name) && !strcmp(name->valuestring, id)) {
      record = r;
      break;
    }
  }
  if (!record)
    goto fail;
  cJSON *length = cJSON_GetObjectItemCaseSensitive(record, "size"),
        *offset = cJSON_GetObjectItemCaseSensitive(record, "offset"),
        *digest = cJSON_GetObjectItemCaseSensitive(record, "sha256");
  if (!cJSON_IsNumber(length) || !cJSON_IsNumber(offset) ||
      !cJSON_IsString(digest) || length->valueint < 24 ||
      length->valueint > PS_ASSET_MAX_BYTES || offset->valueint < 0 ||
      offset->valuedouble != offset->valueint ||
      length->valuedouble != length->valueint)
    goto fail;
  unsigned char requested_hash[32];
  if (strlen(digest->valuestring) != 64)
    goto fail;
  static const char hexchars[] = "0123456789abcdef";
  for (unsigned i = 0; i < 32; i++) {
    const char *hi = strchr(hexchars, digest->valuestring[i * 2]),
               *lo = strchr(hexchars, digest->valuestring[i * 2 + 1]);
    if (!hi || !lo)
      goto fail;
    requested_hash[i] = (hi - hexchars) * 16 + (lo - hexchars);
  }
  for (unsigned i = 0; i < PS_ASSET_SLOTS; i++)
    if (cache[i].data && !strcmp(cache[i].identity, package->identity) &&
        !strcmp(cache[i].name, id) &&
        !memcmp(cache[i].hash, requested_hash, 32)) {
      cache[i].access = ++access_counter;
      return cache[i].token;
    }
  size_t n = length->valueint, position = offset->valueint;
  if (n > budget || position > package->asset_bytes ||
      n > package->asset_bytes - position)
    goto fail;
  entry *slot = NULL;
  for (;;) {
    for (unsigned i = 0; i < PS_ASSET_SLOTS; i++)
      if (!cache[i].data) {
        slot = &cache[i];
        break;
      }
    if (slot && metrics.current + n <= budget)
      break;
    entry *oldest = NULL;
    for (unsigned i = 0; i < PS_ASSET_SLOTS; i++)
      if (cache[i].data && !cache[i].pinned &&
          (!oldest || cache[i].access < oldest->access))
        oldest = &cache[i];
    if (!oldest)
      goto fail;
    drop(oldest);
    slot = NULL;
  }
  uint8_t *data = malloc(n);
  if (!data)
    goto fail;
  if (!resource_file || strcmp(resource_path, package->package_path) ||
      memcmp(resource_hash, package->package_hash, 32)) {
    if (resource_file)
      fclose(resource_file);
    resource_file = fopen(package->package_path, "rb");
    snprintf(resource_path, sizeof(resource_path), "%s", package->package_path);
    memcpy(resource_hash, package->package_hash, 32);
    resource_position = 0;
  }
  size_t target = package->asset_base + position;
  bool ok = resource_file != NULL;
  if (ok && resource_position != target)
    ok = fseek(resource_file, target, SEEK_SET) == 0;
  if (ok) {
    size_t read = fread(data, 1, n, resource_file);
    resource_position = target + read;
    ok = read == n;
  }
  ps_asset_info info;
  if (ok)
    ok = ps_asset_validate(data, n, &info);
  if (ok) {
    static const char *formats[] = {"", "A1", "A4", "I4", "I8", "RGB565"};
    const char *fields[] = {"width", "height", "stride", "palette_count"};
    unsigned values[] = {info.width, info.height, info.stride, info.colors};
    for (int i = 0; i < 4; i++) {
      cJSON *v = cJSON_GetObjectItemCaseSensitive(record, fields[i]);
      if (!cJSON_IsNumber(v) || v->valuedouble != values[i])
        ok = false;
    }
    cJSON *fmt = cJSON_GetObjectItemCaseSensitive(record, "format");
    if (!cJSON_IsString(fmt) || strcmp(fmt->valuestring, formats[info.format]))
      ok = false;
  }
  if (ok) {
    unsigned char hash[32];
    char hex[65];
    sha256(data, n, hash);
    for (int i = 0; i < 32; i++)
      sprintf(hex + 2 * i, "%02x", hash[i]);
    ok = !strcmp(hex, digest->valuestring);
  }
  if (!ok) {
    free(data);
    goto fail;
  }
  memset(slot, 0, sizeof(*slot));
  snprintf(slot->identity, sizeof(slot->identity), "%s", package->identity);
  snprintf(slot->name, sizeof(slot->name), "%s", id);
  memcpy(slot->hash, requested_hash, 32);
  slot->data = data;
  slot->bytes = n;
  slot->info = info;
  slot->access = ++access_counter;
  generation = (generation + 1) & 0x00ffffff;
  if (!generation)
    generation = 1;
  slot->token = (generation << 8) | (unsigned)(slot - cache + 1);
#ifdef ESP_PLATFORM
  _Static_assert(LV_CACHE_DEF_SIZE == 0 && LV_IMAGE_HEADER_CACHE_DEF_CNT == 0,
                 "Owned asset cache requires native caches disabled");
  _Static_assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 5,
                 "Asset descriptors require LVGL 9.5");
  static const lv_color_format_t formats[] = {
      LV_COLOR_FORMAT_UNKNOWN, LV_COLOR_FORMAT_A1, LV_COLOR_FORMAT_A4,
      LV_COLOR_FORMAT_I4,      LV_COLOR_FORMAT_I8, LV_COLOR_FORMAT_RGB565};
  slot->descriptor = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                                 .cf = formats[info.format],
                                                 .w = info.width,
                                                 .h = info.height,
                                                 .stride = info.stride},
                                      .data_size = info.payload_size,
                                      .data = data + 24};
#endif
  metrics.current += n;
  if (metrics.current > metrics.peak)
    metrics.peak = metrics.current;
  metrics.loads++;
  uint64_t elapsed = clock_us() - started;
  metrics.load_us += elapsed;
  if (elapsed > metrics.max_load_us)
    metrics.max_load_us = elapsed;
  return slot->token;
fail:
  metrics.failures++;
  return 0;
}
const void *ps_asset_lvgl(uint32_t token) {
#ifdef ESP_PLATFORM
  entry *e = find(token);
  return e ? &e->descriptor : NULL;
#else
  (void)token;
  return NULL;
#endif
}
