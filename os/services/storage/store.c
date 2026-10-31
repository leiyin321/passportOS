#include "store.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static nvs_handle_t registry;
typedef struct {
  char id[64];
  uint32_t used;
} access_record;
static access_record access_log[12];
static uint32_t access_clock, access_writes;
static size_t cache_budget;
void ps_store_cache_budget(size_t bytes) { cache_budget = bytes; }
static void touch(const char *id) {
  unsigned slot = 0;
  uint32_t oldest = UINT32_MAX;
  bool found = false;
  for (unsigned i = 0; i < 12; i++) {
    if (!strcmp(access_log[i].id, id)) {
      slot = i;
      found = true;
      break;
    }
    if (access_log[i].used < oldest) {
      oldest = access_log[i].used;
      slot = i;
    }
  }
  (void)found;
  snprintf(access_log[slot].id, sizeof(access_log[slot].id), "%s", id);
  access_log[slot].used = ++access_clock;
  if (++access_writes % 32 == 0) {
    nvs_set_blob(registry, "lru", access_log, sizeof(access_log));
    nvs_commit(registry);
  }
}
static uint32_t last_access(const char *id) {
  for (unsigned i = 0; i < 12; i++)
    if (!strcmp(access_log[i].id, id))
      return access_log[i].used;
  return 0;
}

static bool valid_id(const char *id) {
  if (!id || !strchr(id, '.') || strlen(id) > 63)
    return false;
  for (; *id; id++)
    if (!(*id >= 'a' && *id <= 'z') && !(*id >= '0' && *id <= '9') &&
        *id != '.' && *id != '-')
      return false;
  return true;
}
static void path(char *out, size_t n, const char *id, const char *suffix) {
  snprintf(out, n, "/cache/%s.%s", id, suffix);
}
size_t ps_store_package_size(const char *id) {
  if (!valid_id(id)) return 0;
  char file[96]; struct stat st;
  path(file, sizeof(file), id, "pap");
  return !stat(file, &st) && st.st_size > 0 ? (size_t)st.st_size : 0;
}
bool ps_store_init(void) {
  // Never autoformat an existing filesystem after an arbitrary mount error.
  esp_vfs_spiffs_conf_t cfg = {.base_path = "/cache",
                               .partition_label = "packages",
                               .max_files = 5,
                               .format_if_mount_failed = false};
  if (esp_vfs_spiffs_register(&cfg) != ESP_OK)
    return false;
  bool ok = nvs_flash_init_partition("appdata") == ESP_OK &&
            nvs_open_from_partition("appdata", "registry", NVS_READWRITE,
                                    &registry) == ESP_OK;
  if (ok) {
    size_t n = sizeof(access_log);
    nvs_get_blob(registry, "lru", access_log, &n);
    for (unsigned i = 0; i < 12; i++)
      if (access_log[i].used > access_clock)
        access_clock = access_log[i].used;
  }
  return ok;
}
static bool read_package(const char *file, ps_package *p) {
  return ps_package_file_lazy(file, p);
}
bool ps_store_is_pinned(const char *id) {
  char ids[768];
  size_t n = sizeof(ids);
  if (nvs_get_str(registry, "pins", ids, &n) != ESP_OK)
    return false;
  char needle[67];
  snprintf(needle, sizeof(needle), "|%s|", id);
  return strstr(ids, needle) != NULL;
}
int ps_store_list(char ids[][64], int cap) {
  int n = 0;
  DIR *dir = opendir("/cache");
  if (!dir)
    return 0;
  struct dirent *e;
  while ((e = readdir(dir)) && n < cap) {
    size_t l = strlen(e->d_name);
    if (l > 4 && l < 68 && !strcmp(e->d_name + l - 4, ".pap")) {
      memcpy(ids[n], e->d_name, l - 4);
      ids[n][l - 4] = 0;
      if (valid_id(ids[n]))
        n++;
    }
  }
  closedir(dir);
  return n;
}
bool ps_store_offload(const char *id) {
  if (!valid_id(id))
    return false;
  char f[90];
  bool removed = false;
  const char *suffixes[] = {"pap", "old", "new"};
  for (int i = 0; i < 3; i++) {
    path(f, sizeof(f), id, suffixes[i]);
    if (unlink(f) == 0)
      removed = true;
  }
  return removed;
}
bool ps_store_install_file(const char *source, const char *expected_id,
                           char id[64]) {
  struct stat source_stat;
  if (stat(source, &source_stat) || source_stat.st_size < 16 ||
      source_stat.st_size > PS_PAP_MAX)
    return false;
  size_t n = source_stat.st_size;
  ps_package p;
  if (!ps_package_file(source, &p))
    return false;
  if (expected_id && strcmp(expected_id, p.id)) {
    ps_package_free(&p);
    return false;
  }
  // An enrolled publisher cannot take over an ID whose entry survives offload.
  nvs_iterator_t owner = NULL;
  esp_err_t next = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &owner);
  bool owned_by_other = false;
  unsigned entries = 0;
  bool existing_identity = false;
  while (next == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(owner, &info);
    char registered[64];
    size_t length = sizeof(registered);
    if (strlen(info.key) == 14 &&
        nvs_get_str(registry, info.key, registered, &length) == ESP_OK &&
        valid_id(registered)) {
      entries++;
      if (!strcmp(info.key, p.identity))
        existing_identity = true;
      if (!strcmp(registered, p.id) && strcmp(info.key, p.identity))
        owned_by_other = true;
    }
    next = nvs_entry_next(&owner);
  }
  nvs_release_iterator(owner);
  if (owned_by_other || (!existing_identity && entries >= 12)) {
    ESP_LOGW("passport", "Registry ownership or entry limit denied");
    ps_package_free(&p);
    return false;
  }
  strcpy(id, p.id);
  char final[90], staging[90], old[90];
  path(final, sizeof(final), id, "pap");
  path(staging, sizeof(staging), id, "new");
  path(old, sizeof(old), id, "old");
  // Keep data partition untouched; explicit format is a factory provisioning
  // step.
  size_t total, used;
  if (esp_spiffs_info("packages", &total, &used) != ESP_OK) {
    ps_package_free(&p);
    return false;
  }
  char cached[12][64];
  int count = ps_store_list(cached, 12);
  size_t logical = 0;
  for (int i = 0; i < count; i++) {
    char f[90];
    struct stat st;
    path(f, sizeof(f), cached[i], "pap");
    if (!stat(f, &st))
      logical += st.st_size;
    path(f, sizeof(f), cached[i], "old");
    if (!stat(f, &st))
      logical += st.st_size;
  }
  while (total - used < 8192 || (cache_budget && logical + n > cache_budget)) {
    int oldest = -1;
    uint32_t stamp = UINT32_MAX;
    for (int i = 0; i < count; i++)
      if (*cached[i] && strcmp(cached[i], id) &&
          !ps_store_is_pinned(cached[i]) && last_access(cached[i]) <= stamp) {
        oldest = i;
        stamp = last_access(cached[i]);
      }
    if (oldest < 0) {
      ps_package_free(&p);
      return false;
    }
    char f[90];
    struct stat st;
    path(f, sizeof(f), cached[oldest], "pap");
    if (!stat(f, &st))
      logical -= st.st_size;
    path(f, sizeof(f), cached[oldest], "old");
    if (!stat(f, &st))
      logical -= st.st_size;
    ESP_LOGI("passport", "CACHE auto-offload %s", cached[oldest]);
    ps_store_offload(cached[oldest]);
    cached[oldest][0] = 0;
    esp_spiffs_info("packages", &total, &used);
  }
  unlink(staging);
  bool ok = rename(source, staging) == 0;
  if (ok) {
    unlink(old);
    struct stat existing;
    if (!stat(final, &existing) && rename(final, old))
      ok = false;
    if (ok && rename(staging, final))
      ok = false;
  }
  if (ok) { // Entry survives offload in independent registry; two-version code
            // recovery.
    ok = nvs_set_str(registry, p.identity, id) == ESP_OK;
    cJSON *icon = cJSON_GetObjectItemCaseSensitive(p.manifest, "icon");
    if (cJSON_IsString(icon) && strlen(icon->valuestring) <= 64) {
      char key[16];
      snprintf(key, sizeof(key), "i%.14s", p.identity);
      if (nvs_set_str(registry, key, icon->valuestring) != ESP_OK)
        ok = false;
    }
    if (ok)
      ok = nvs_commit(registry) == ESP_OK;
  }
  if (!ok) {
    ESP_LOGW("passport", "Store transaction failed errno=%d", errno);
    unlink(staging);
  }
  if (ok)
    touch(id);
  ps_package_free(&p);
  return ok;
}
bool ps_store_load(const char *id, ps_package *p) {
  if (!valid_id(id))
    return false;
  char f[90];
  path(f, sizeof(f), id, "pap");
  bool ok = read_package(f, p);
  if (!ok) {
    path(f, sizeof(f), id, "old");
    ok = read_package(f, p);
    if (!ok) {
      path(f, sizeof(f), id, "new");
      ok = read_package(f, p);
    }
  }
  if (ok)
    touch(id);
  return ok;
}
int ps_store_registered(char ids[][64], int capacity) {
  int count = 0;
  nvs_iterator_t it = NULL;
  esp_err_t rc = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &it);
  while (rc == ESP_OK && count < capacity) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    size_t n = 64;
    if (strlen(info.key) == 14 &&
        nvs_get_str(registry, info.key, ids[count], &n) == ESP_OK &&
        valid_id(ids[count]))
      count++;
    rc = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return count;
}
bool ps_store_pin(const char *id, bool pin) {
  if (!valid_id(id))
    return false;
  char pins[768] = "", needle[67];
  size_t n = sizeof(pins);
  nvs_get_str(registry, "pins", pins, &n);
  snprintf(needle, sizeof(needle), "|%s|", id);
  char *pos = strstr(pins, needle);
  if (pos)
    memmove(pos, pos + strlen(needle), strlen(pos + strlen(needle)) + 1);
  if (pin) {
    if (strlen(pins) + strlen(needle) >= sizeof(pins))
      return false;
    strcat(pins, needle);
  }
  return nvs_set_str(registry, "pins", pins) == ESP_OK &&
         nvs_commit(registry) == ESP_OK;
}
static bool key_valid(const char *key) {
  if (!key || !*key || strlen(key) > 15)
    return false;
  for (; *key; key++)
    if (!((*key >= 'a' && *key <= 'z') || (*key >= '0' && *key <= '9') ||
          *key == '_'))
      return false;
  return true;
}
bool ps_data_get(const ps_package *p, const char *key, char *out, size_t cap) {
  if (!(p->permissions & PS_STORAGE) || !key_valid(key))
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("appdata", p->identity, NVS_READONLY, &h) !=
      ESP_OK)
    return false;
  bool ok = nvs_get_str(h, key, out, &cap) == ESP_OK;
  nvs_close(h);
  return ok;
}
bool ps_data_set(const ps_package *p, const char *key, const char *value) {
  if (!(p->permissions & PS_STORAGE) || !key_valid(key) || strlen(value) > 1024)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("appdata", p->identity, NVS_READWRITE, &h) !=
      ESP_OK)
    return false;
  // Conservatively charge NVS metadata and replaced entries against 4KiB
  // logical quota.
  size_t used = 0;
  nvs_entry_info_t info;
  nvs_iterator_t it = NULL;
  esp_err_t r = nvs_entry_find("appdata", p->identity, NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info(it, &info);
    size_t len = 0;
    if (nvs_get_str(h, info.key, NULL, &len) != ESP_OK)
      len = 128;
    if (strcmp(info.key, key))
      used += len + 64;
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  cJSON *limits = cJSON_GetObjectItemCaseSensitive(p->manifest, "limits");
  int quota =
      cJSON_GetObjectItemCaseSensitive(limits, "persistent_kib")->valueint *
      1024;
  bool ok = used + strlen(value) + 65 <= (size_t)quota &&
            nvs_set_str(h, key, value) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}
bool ps_store_remove(const char *id, bool delete_data) {
  if (!valid_id(id))
    return false;
  char identity[16] = "";
  nvs_iterator_t it = NULL;
  esp_err_t rc = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &it);
  while (rc == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    char value[64];
    size_t n = sizeof(value);
    if (strlen(info.key) == 14 &&
        nvs_get_str(registry, info.key, value, &n) == ESP_OK &&
        !strcmp(value, id)) {
      snprintf(identity, sizeof(identity), "%s", info.key);
      break;
    }
    rc = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  if (!*identity)
    return false;
  bool ok = true;
  if (delete_data) {
    nvs_handle_t h;
    if (nvs_open_from_partition("appdata", identity, NVS_READWRITE, &h) ==
        ESP_OK) {
      ok = nvs_erase_all(h) == ESP_OK && nvs_commit(h) == ESP_OK;
      nvs_close(h);
    }
    if (ok) {
      char icon_key[16];
      snprintf(icon_key, sizeof(icon_key), "i%.14s", identity);
      nvs_erase_key(registry, icon_key);
      nvs_erase_key(registry, identity);
      nvs_commit(registry);
    }
  }
  ps_store_pin(id, false);
  ps_store_offload(id);
  return ok;
}

bool ps_store_install(const void *data, size_t n, char id[64]) {
  if (!data || n > PS_PAP_MAX)
    return false;
  const char *path = "/cache/memory-upload.pap";
  FILE *f = fopen(path, "wb");
  if (!f)
    return false;
  bool ok = fwrite(data, 1, n, f) == n;
  if (fclose(f))
    ok = false;
  if (ok)
    ok = ps_store_install_file(path, NULL, id);
  unlink(path);
  return ok;
}

bool ps_store_icon(const char *id, char out[65]) {
  snprintf(out, 65, "system.app-default");
  nvs_iterator_t it = NULL;
  esp_err_t rc = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &it);
  bool found = false;
  while (rc == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    char registered[64];
    size_t n = sizeof(registered);
    if (strlen(info.key) == 14 &&
        nvs_get_str(registry, info.key, registered, &n) == ESP_OK &&
        !strcmp(registered, id)) {
      char key[16];
      snprintf(key, sizeof(key), "i%.14s", info.key);
      n = 65;
      found = nvs_get_str(registry, key, out, &n) == ESP_OK;
      break;
    }
    rc = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return found;
}

bool ps_store_data_hash(const char *id, const char *key, char hex[65]) {
  if (!valid_id(id) || !key_valid(key))
    return false;
  char identity[16] = "";
  nvs_iterator_t it = NULL;
  esp_err_t rc = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &it);
  while (rc == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    char value[64];
    size_t n = sizeof(value);
    if (strlen(info.key) == 14 &&
        nvs_get_str(registry, info.key, value, &n) == ESP_OK &&
        !strcmp(value, id)) {
      snprintf(identity, sizeof(identity), "%s", info.key);
      break;
    }
    rc = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  if (!*identity)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("appdata", identity, NVS_READONLY, &h) != ESP_OK)
    return false;
  char value[1025];
  size_t n = sizeof(value);
  bool ok = nvs_get_str(h, key, value, &n) == ESP_OK;
  nvs_close(h);
  if (!ok)
    return false;
  unsigned char hash[32];
  mbedtls_sha256((unsigned char *)value, n - 1, hash, 0);
  for (int i = 0; i < 32; i++)
    sprintf(hex + 2 * i, "%02x", hash[i]);
  return true;
}

// Trusted developer fault injection. No mini-app binding exposes this function.
bool ps_store_fault_fixture(const char *id, const char *mode) {
  if (!valid_id(id) || strncmp(id, "org.passport.", 13))
    return false;
  ps_package verified;
  if (!ps_store_load(id, &verified))
    return false;
  char active[90], old[90], staging[90];
  path(active, sizeof(active), id, "pap");
  path(old, sizeof(old), id, "old");
  path(staging, sizeof(staging), id, "new");
  bool ok = false;
  if (!strcmp(mode, "new-only")) {
    if (!strcmp(verified.package_path, staging))
      ok = true;
    else {
      unlink(staging);
      ok = rename(verified.package_path, staging) == 0;
    }
    if (ok) {
      unlink(active);
      unlink(old);
    }
  } else if (!strcmp(mode, "old-only")) {
    if (strcmp(verified.package_path, old)) {
      unlink(old);
      ok = rename(verified.package_path, old) == 0;
    } else
      ok = true;
    unlink(active);
    FILE *f = fopen(staging, "wb");
    if (f) {
      fwrite("partial", 1, 7, f);
      fclose(f);
    }
  } else if (!strcmp(mode, "partial")) {
    FILE *f = fopen(staging, "wb");
    if (f) {
      ok = fwrite("partial", 1, 7, f) == 7;
      if (fclose(f))
        ok = false;
    }
  }
  ps_package_free(&verified);
  return ok;
}
