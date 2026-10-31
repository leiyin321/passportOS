#include "package.h"
#include "native_elf.h"
#include "display_layout.h"
#include "asset_format.h"
#include "esp_log.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "trust.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned le32(const unsigned char *p) {
  return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}
static const char *str(const cJSON *m, const char *key) {
  const cJSON *j = cJSON_GetObjectItemCaseSensitive(m, key);
  return cJSON_IsString(j) ? j->valuestring : "";
}
static bool integer(const cJSON *o, const char *k, int max) {
  const cJSON *j = cJSON_GetObjectItemCaseSensitive(o, k);
  return cJSON_IsNumber(j) && j->valuedouble == j->valueint &&
         j->valueint > 0 && j->valueint <= max;
}
void ps_package_free(ps_package *p) {
  cJSON_Delete(p->manifest);
  free(p->code);
  memset(p, 0, sizeof(*p));
}
typedef struct {
  const unsigned char *memory;
  FILE *file;
  size_t size, position;
  bool full_assets;
} reader;
static bool read_at(reader *r, size_t offset, void *out, size_t n) {
  if (offset > r->size || n > r->size - offset)
    return false;
  if (r->memory) {
    memcpy(out, r->memory + offset, n);
    return true;
  }
  if (r->position != offset && fseek(r->file, offset, SEEK_SET))
    return false;
  size_t got = fread(out, 1, n, r->file);
  r->position = offset + got;
  return got == n;
}
static bool hash_range(reader *r, size_t offset, size_t size,
                       unsigned char digest[32]) {
  unsigned char block[512];
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  bool ok = true;
  while (size) {
    size_t n = size > sizeof(block) ? sizeof(block) : size;
    if (!read_at(r, offset, block, n)) {
      ok = false;
      break;
    }
    mbedtls_sha256_update(&sha, block, n);
    offset += n;
    size -= n;
  }
  if (ok)
    mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  return ok;
}
static bool unsigned_field(const cJSON *j, const char *key, unsigned max,
                           unsigned *out) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, key);
  if (!cJSON_IsNumber(v) || v->valuedouble < 0 || v->valuedouble > max ||
      v->valuedouble != v->valueint)
    return false;
  *out = v->valueint;
  return true;
}
static bool parse_source(reader *r, ps_package *p) {
  const char *stage = "header";
  memset(p, 0, sizeof(*p));
  size_t size = r->size;
  unsigned char header[16];
  if (size < 16 || size > PS_PAP_MAX || !read_at(r, 0, header, 16))
    return false;
  bool assets = !memcmp(header, "PAP2", 4);
  if (!assets && memcmp(header, "PAP1", 4))
    return false;
  unsigned ml = le32(header + 4), pl = le32(header + 8), sl = le32(header + 12);
  if (!ml || ml > (assets ? 12288 : 4096) || !pl || sl != 256 ||
      (uint64_t)size != 16ull + ml + pl + sl)
    return false;
  unsigned code_size = pl, code_offset = 16 + ml;
  if (assets) {
    unsigned char length[4];
    if (pl < 4 || !read_at(r, 16 + ml, length, 4))
      return false;
    code_size = le32(length);
    code_offset += 4;
  }
  if (!code_size || code_size > PS_NATIVE_FILE_MAX ||
      code_offset + code_size > 16 + ml + pl)
    return false;
  p->asset_base = code_offset + code_size;
  p->asset_bytes = 16 + ml + pl - p->asset_base;
  char *raw = malloc(ml + 1);
  if (!raw)
    return false;
  if (!read_at(r, 16, raw, ml)) {
    free(raw);
    return false;
  }
  raw[ml] = 0;
  stage = "manifest JSON";
  p->manifest = ps_json_parse_budget(raw, ml, assets ? 4096 : 512);
  free(raw);
  if (!p->manifest)
    goto fail;
  stage = "publisher";
  char public_key[512];
  if (!ps_trust_lookup(str(p->manifest, "publisher_key_id"), public_key,
                       sizeof(public_key)))
    goto fail;
  stage = "signature";
  unsigned char hash[32];
  if (!hash_range(r, 0, size - sl, hash))
    goto fail;
  memcpy(p->package_hash, hash, 32);
  unsigned char signature[256];
  if (!read_at(r, size - sl, signature, sl))
    goto fail;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int rc = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)public_key,
                                       strlen(public_key) + 1);
  if (!rc)
    rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sl);
  unsigned char der[512];
  int dl = mbedtls_pk_write_pubkey_der(&pk, der, sizeof(der));
  mbedtls_pk_free(&pk);
  if (rc || dl <= 0)
    goto fail;
  unsigned char publisher[32];
  mbedtls_sha256(der + sizeof(der) - dl, dl, publisher, 0);
  char kid[17];
  for (int i = 0; i < 8; i++)
    sprintf(kid + i * 2, "%02x", publisher[i]);
  if (!p->manifest || !ps_json_safe(p->manifest, 0))
    goto fail;
  stage = "manifest fields";
  const cJSON *m = p->manifest;
  const char *id = str(m, "id"), *runtime = str(m, "runtime"),
             *entry = str(m, "entry"), *version = str(m, "version");
  if (strcmp(str(m, "format"), "passport.app/1") ||
      strcmp(str(m, "publisher_key_id"), kid) || strlen(id) > 63 ||
      !islower((unsigned char)*id) || !strchr(id, '.'))
    goto fail;
  for (const char *c = id; *c; c++)
    if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) &&
        *c != '.' && *c != '-')
      goto fail;
  unsigned va, vb, vc;
  char extra;
  if (sscanf(version, "%u.%u.%u%c", &va, &vb, &vc, &extra) != 3)
    goto fail;
  bool native = !strcmp(runtime, "native-elf");
  if ((strcmp(runtime, "lua") && strcmp(runtime, "declarative") && !native) ||
      strcmp(entry, native ? "app.app.elf" : (!strcmp(runtime, "lua") ? "main.lua" : "ui.json")) ||
      (!native && code_size > PS_CODE_MAX)) goto fail;
  if (!native && (cJSON_GetObjectItemCaseSensitive(m, "target") ||
      cJSON_GetObjectItemCaseSensitive(m, "abi") || cJSON_GetObjectItemCaseSensitive(m, "trusted"))) goto fail;
  if (native && (strcmp(str(m, "target"), "esp32c3") || !integer(m, "abi", 2) ||
      !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(m, "trusted")) ||
      !ps_trust_native_allowed(str(m, "publisher_key_id")))) goto fail;
  if (!integer(m, "min_os_api", 1) || !integer(m, "data_schema", 1))
    goto fail;
  stage = "display";
  ps_display_layout display_config;
  if (!ps_display_layout_read(m, &display_config)) goto fail;
  stage = "limits";
  const cJSON *limits = cJSON_GetObjectItemCaseSensitive(m, "limits");
  if (!(native ? integer(limits, "native_exec_kib", 16) : integer(limits, "script_heap_kib", 64)) ||
      !integer(limits, "fps", 20) || !integer(limits, "persistent_kib", 4))
    goto fail;
  if (cJSON_GetObjectItemCaseSensitive(limits, native ? "script_heap_kib" : "native_exec_kib")) goto fail;
  if (cJSON_GetObjectItemCaseSensitive(limits, "image_cache_kib") &&
      !integer(limits, "image_cache_kib", 16))
    goto fail;
  stage = "permissions";
  const cJSON *perms = cJSON_GetObjectItemCaseSensitive(m, "permissions");
  if (!cJSON_IsArray(perms))
    goto fail;
  static const char *names[] = {"ui",
                                "graphics",
                                "input",
                                "timer",
                                "storage.private",
                                "network.fetch",
                                "network.websocket",
                                "audio.features",
                                "audio.stream",
                                "device.identity", "bluetooth.hid", "audio.playback", "audio.http"};
  for (const cJSON *v = perms->child; v; v = v->next) {
    if (!cJSON_IsString(v))
      goto fail;
    unsigned bit = 0;
    for (int i = 0; i < 13; i++)
      if (!strcmp(v->valuestring, names[i]))
        bit = 1u << i;
    if (!bit || (p->permissions & bit))
      goto fail;
    p->permissions |= bit;
  }
  if (native && cJSON_GetObjectItemCaseSensitive(m, "abi")->valueint == 1 && (p->permissions & ~(PS_UI | PS_GRAPHICS | PS_INPUT | PS_TIMER))) goto fail;
  if ((p->permissions & (PS_BLE_HID | PS_AUDIO_PLAYBACK)) && (!native || cJSON_GetObjectItemCaseSensitive(m,"abi")->valueint!=2)) goto fail;
  if ((p->permissions & PS_AUDIO_HTTP) &&
      (!native || cJSON_GetObjectItemCaseSensitive(m,"abi")->valueint!=2 ||
       (p->permissions&(PS_FETCH|PS_AUDIO_PLAYBACK))!=(PS_FETCH|PS_AUDIO_PLAYBACK))) goto fail;
  stage = "domains";
  const cJSON *domains = cJSON_GetObjectItemCaseSensitive(m, "domains");
  if (domains && (!cJSON_IsArray(domains) || cJSON_GetArraySize(domains) > 4))
    goto fail;
  for (const cJSON *d = domains ? domains->child : NULL; d; d = d->next) {
    if (!cJSON_IsString(d) || strlen(d->valuestring) > 127 ||
        !strchr(d->valuestring, '.'))
      goto fail;
    bool alpha = false;
    for (const char *c = d->valuestring; *c; c++) {
      if (*c >= 'a' && *c <= 'z')
        alpha = true;
      else if (!(*c >= '0' && *c <= '9') && *c != '.' && *c != '-')
        goto fail;
    }
    if (!alpha)
      goto fail;
    for (const cJSON *other = d->next; other; other = other->next)
      if (cJSON_IsString(other) && !strcmp(d->valuestring, other->valuestring))
        goto fail;
  }
  stage = "content hash";
  if (!hash_range(r, code_offset, code_size, hash))
    goto fail;
  char hex[65];
  for (int i = 0; i < 32; i++)
    sprintf(hex + i * 2, "%02x", hash[i]);
  if (strcmp(str(m, "sha256"), hex))
    goto fail;
  stage = "asset index";
  const cJSON *index = cJSON_GetObjectItemCaseSensitive(m, "assets");
  if (assets) {
    if (!cJSON_IsArray(index) || cJSON_GetArraySize(index) > 32)
      goto fail;
    unsigned expected = 0;
    for (const cJSON *a = index->child; a; a = a->next) {
      const char *rid = str(a, "id");
      if (!ps_asset_id_valid(rid))
        goto fail;
      for (const cJSON *other = a->next; other; other = other->next)
        if (!strcmp(rid, str(other, "id")))
          goto fail;
      unsigned offset, length;
      if (!unsigned_field(a, "offset", p->asset_bytes, &offset) ||
          !unsigned_field(a, "size", PS_ASSET_MAX_BYTES, &length) ||
          offset != expected || length < 24 || length > p->asset_bytes - offset)
        goto fail;
      if (r->full_assets) {
        unsigned char *image = malloc(length);
        if (!image)
          goto fail;
        ps_asset_info info;
        bool valid = read_at(r, p->asset_base + offset, image, length) &&
                     ps_asset_validate(image, length, &info);
        if (valid) {
          mbedtls_sha256(image, length, hash, 0);
          for (int i = 0; i < 32; i++)
            sprintf(hex + i * 2, "%02x", hash[i]);
          valid = !strcmp(hex, str(a, "sha256"));
        }
        free(image);
        if (!valid)
          goto fail;
        static const char *formats[] = {"", "A1", "A4", "I4", "I8", "RGB565"};
        unsigned w, h, stride, colors;
        if (!unsigned_field(a, "width", 96, &w) ||
            !unsigned_field(a, "height", 96, &h) ||
            !unsigned_field(a, "stride", 192, &stride) ||
            !unsigned_field(a, "palette_count", 256, &colors) ||
            w != info.width || h != info.height || stride != info.stride ||
            colors != info.colors ||
            strcmp(str(a, "format"), formats[info.format]))
          goto fail;
      } else {
        unsigned w, h, stride, colors;
        if (!unsigned_field(a, "width", 96, &w) || !w ||
            !unsigned_field(a, "height", 96, &h) || !h ||
            !unsigned_field(a, "stride", 192, &stride) || !stride ||
            !unsigned_field(a, "palette_count", 256, &colors) ||
            strlen(str(a, "sha256")) != 64)
          goto fail;
      }
      expected += length;
    }
    if (expected != p->asset_bytes)
      goto fail;
  } else if (index && cJSON_GetArraySize(index))
    goto fail;
  stage = "code allocation";
  p->code = malloc(code_size + 1);
  if (!p->code || !read_at(r, code_offset, p->code, code_size))
    goto fail;
  if (native) {
    if (!ps_native_elf_validate(p->code, code_size,
        cJSON_GetObjectItemCaseSensitive(limits, "native_exec_kib")->valueint * 1024u, NULL)) goto fail;
  } else if (memchr(p->code, 0, code_size)) goto fail;
  p->code_size = code_size;
  p->code[code_size] = 0;
  strcpy(p->id, id);
  strcpy(p->runtime, runtime);
  // Identity incorporates trusted publisher and app ID; cache never owns data.
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  mbedtls_sha256_update(&sha, publisher, 32);
  mbedtls_sha256_update(&sha, (const unsigned char *)id, strlen(id));
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);
  for (int i = 0; i < 7; i++)
    sprintf(p->identity + i * 2, "%02x", hash[i]);
  return true;
fail:
  ESP_LOGW("passport", "PAP rejected: %s", stage);
  ps_package_free(p);
  return false;
}

bool ps_package_parse(const void *data, size_t size, ps_package *out) {
  if (!data) {
    memset(out, 0, sizeof(*out));
    return false;
  }
  reader r = {.memory = data, .size = size, .full_assets = true};
  return parse_source(&r, out);
}
static bool file_package(const char *path, ps_package *out, bool full_assets) {
  memset(out, 0, sizeof(*out));
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return false;
  }
  long size = ftell(f);
  if (size < 16 || size > PS_PAP_MAX) {
    fclose(f);
    return false;
  }
  reader r = {
      .file = f, .size = size, .position = size, .full_assets = full_assets};
  bool ok = parse_source(&r, out);
  fclose(f);
  if (ok)
    snprintf(out->package_path, sizeof(out->package_path), "%s", path);
  return ok;
}
bool ps_package_file_hash(const char *path, char hex[65]) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  reader r = {.file = f, .size = n, .position = n};
  unsigned char hash[32];
  bool ok = n > 0 && n <= PS_PAP_MAX && hash_range(&r, 0, n, hash);
  fclose(f);
  if (ok)
    for (int i = 0; i < 32; i++)
      sprintf(hex + i * 2, "%02x", hash[i]);
  return ok;
}

bool ps_package_file(const char *path, ps_package *out) {
  return file_package(path, out, true);
}
bool ps_package_file_lazy(const char *path, ps_package *out) {
  return file_package(path, out, false);
}
