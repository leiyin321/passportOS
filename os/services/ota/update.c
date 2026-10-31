#include "update.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "network.h"
#include "nvs.h"
#include "package.h"
#include "trusted_key.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ps_update_info info;
static char manifest_url[512], image_url[512];
static char expected_hash[65];
static unsigned char signature[256];
static unsigned char actual_hash[32];
static mbedtls_sha256_context sha;
static esp_ota_handle_t ota_handle;
static const esp_partition_t *target_partition;
static bool ota_open;

static void set_error(const char *reason) {
  if (ota_open) {
    esp_ota_abort(ota_handle);
    ota_open = false;
  }
  info.state = PS_UPDATE_ERROR;
  snprintf(info.error, sizeof(info.error), "%s", reason);
}

static bool version_valid(const char *s) {
  if (!s || !*s || strlen(s) > 31) return false;
  unsigned dots = 0;
  bool digit = false;
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9') digit = true;
    else if (*s == '.' && digit && dots < 2) {
      dots++;
      digit = false;
    } else return false;
  }
  return dots == 2 && digit;
}

static bool hex_decode(const char *s, unsigned char *out, size_t bytes) {
  if (!s || strlen(s) != bytes * 2) return false;
  for (size_t i = 0; i < bytes; i++) {
    for (int j = 0; j < 2; j++)
      if (!((s[i * 2 + j] >= '0' && s[i * 2 + j] <= '9') ||
            (s[i * 2 + j] >= 'a' && s[i * 2 + j] <= 'f')))
        return false;
    unsigned value;
    if (sscanf(s + i * 2, "%2x", &value) != 1) return false;
    out[i] = (unsigned char)value;
  }
  return true;
}

static bool image_url_from_manifest(const char *name) {
  if (!name || !*name || strlen(name) > 96 || !strcmp(name, ".") ||
      !strcmp(name, ".."))
    return false;
  for (const char *p = name; *p; p++)
    if (!( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
           (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-'))
      return false;
  const char *slash = strrchr(manifest_url, '/');
  if (!slash || (size_t)(slash - manifest_url) + 1 + strlen(name) >=
                    sizeof(image_url))
    return false;
  size_t base = (size_t)(slash - manifest_url) + 1;
  memcpy(image_url, manifest_url, base);
  strcpy(image_url + base, name);
  return true;
}

static bool manifest_verify(const char *data, size_t size) {
  cJSON *root = ps_json_parse(data, size);
  if (!root || !cJSON_IsObject(root) || cJSON_GetArraySize(root) != 8) {
    cJSON_Delete(root);
    return false;
  }
  cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
  cJSON *chip = cJSON_GetObjectItemCaseSensitive(root, "chip");
  cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "version");
  cJSON *build = cJSON_GetObjectItemCaseSensitive(root, "build");
  cJSON *length = cJSON_GetObjectItemCaseSensitive(root, "size");
  cJSON *hash = cJSON_GetObjectItemCaseSensitive(root, "sha256");
  cJSON *image = cJSON_GetObjectItemCaseSensitive(root, "image");
  cJSON *sig = cJSON_GetObjectItemCaseSensitive(root, "signature");
  bool fields = cJSON_IsString(format) &&
                !strcmp(format->valuestring, "passport.firmware/1") &&
                cJSON_IsString(chip) && !strcmp(chip->valuestring, "esp32c3") &&
                cJSON_IsString(version) && version_valid(version->valuestring) &&
                cJSON_IsNumber(build) && build->valuedouble >= 1 &&
                build->valuedouble <= 2147483647.0 &&
                (double)build->valueint == build->valuedouble &&
                cJSON_IsNumber(length) && length->valuedouble >= 4096 &&
                length->valuedouble <= 2621440.0 &&
                (double)(size_t)length->valuedouble == length->valuedouble &&
                cJSON_IsString(hash) && strlen(hash->valuestring) == 64 &&
                cJSON_IsString(image) && strlen(image->valuestring) <= 96 &&
                cJSON_IsString(sig) && strlen(sig->valuestring) == 512;
  if (!fields) {
    cJSON_Delete(root);
    return false;
  }
  info.build = (uint32_t)build->valueint;
  info.image_size = (size_t)length->valuedouble;
  snprintf(info.version, sizeof(info.version), "%s", version->valuestring);
  snprintf(expected_hash, sizeof(expected_hash), "%s", hash->valuestring);
  bool lower_hex = true;
  for (size_t i = 0; i < 64; i++)
    if (!((expected_hash[i] >= '0' && expected_hash[i] <= '9') ||
          (expected_hash[i] >= 'a' && expected_hash[i] <= 'f')))
      lower_hex = false;
  char image_name[97];
  snprintf(image_name, sizeof(image_name), "%s", image->valuestring);
  char sig_hex[513];
  if (strlen(sig->valuestring) != 512) {
    cJSON_Delete(root);
    return false;
  }
  memcpy(sig_hex, sig->valuestring, 513);
  cJSON_Delete(root);
  if (!lower_hex || !hex_decode(sig_hex, signature, sizeof(signature)) ||
      !image_url_from_manifest(image_name))
    return false;

  target_partition = esp_ota_get_next_update_partition(NULL);
  if (!target_partition || info.image_size > target_partition->size)
    return false;
  char canonical[512];
  int n = snprintf(canonical, sizeof(canonical),
                   "passport-firmware/1\nchip=esp32c3\nversion=%s\nbuild=%u\n"
                   "size=%u\nsha256=%s\nimage=%s\n",
                   info.version, (unsigned)info.build,
                   (unsigned)info.image_size,
                   expected_hash, image_name);
  if (n <= 0 || n >= sizeof(canonical)) return false;
  unsigned char digest[32];
  if (mbedtls_sha256((const unsigned char *)canonical, (size_t)n, digest, 0))
    return false;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int rc = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)ps_trusted_key,
                                       sizeof(ps_trusted_key));
  if (!rc && (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA) ||
              mbedtls_pk_get_bitlen(&pk) != 2048))
    rc = -1;
  if (!rc)
    rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, digest, 0, signature,
                           sizeof(signature));
  mbedtls_pk_free(&pk);
  return rc == 0;
}

void ps_update_init(void) {
  memset(&info, 0, sizeof(info));
  info.state = PS_UPDATE_IDLE;
}

bool ps_update_check(const char *url) {
  if (!url || strlen(url) >= sizeof(manifest_url)) return false;
  if (info.state == PS_UPDATE_CHECKING || info.state == PS_UPDATE_DOWNLOADING ||
      info.state == PS_UPDATE_READY || info.state == PS_UPDATE_INSTALLING)
    return false;
  snprintf(manifest_url, sizeof(manifest_url), "%s", url);
  info.error[0] = 0;
  info.version[0] = 0;
  info.downloaded = info.image_size = 0;
  info.state = PS_UPDATE_CHECKING;
  if (!ps_net_fetch(NULL, manifest_url, PS_UPDATE_MANIFEST_GENERATION, true)) {
    set_error("Network busy or offline");
    return false;
  }
  return true;
}

bool ps_update_accept_manifest(int status, const char *data, size_t size) {
  if (info.state != PS_UPDATE_CHECKING) return false;
  if (status != 200) {
    set_error("Manifest HTTPS request failed");
    return false;
  }
  if (!data || !size || size > 4096 || !manifest_verify(data, size)) {
    set_error("Manifest or signature invalid");
    return false;
  }
  nvs_handle_t nvs;
  uint32_t installed = 0;
  if (nvs_open("system_update", NVS_READONLY, &nvs) == ESP_OK) {
    nvs_get_u32(nvs, "installed", &installed);
    nvs_close(nvs);
  }
  if (info.build <= installed) {
    info.state = PS_UPDATE_CURRENT;
    return true;
  }
  info.state = PS_UPDATE_AVAILABLE;
  return true;
}

static bool write_image(const void *data, size_t size, void *context) {
  (void)context;
  if (!ota_open || !data || !size ||
      __atomic_load_n(&info.downloaded, __ATOMIC_RELAXED) + size > info.image_size)
    return false;
  esp_err_t rc = esp_ota_write(ota_handle, data, size);
  if (rc != ESP_OK) {
    ESP_LOGE("passport_ota", "esp_ota_write failed offset=%u rc=%s",
             (unsigned)info.downloaded, esp_err_to_name(rc));
    return false;
  }
  if (mbedtls_sha256_update(&sha, data, size)) return false;
  __atomic_fetch_add(&info.downloaded, size, __ATOMIC_RELAXED);
  return true;
}

bool ps_update_start_download(void) {
  if (info.state != PS_UPDATE_AVAILABLE || !target_partition) return false;
  info.downloaded = 0;
  mbedtls_sha256_init(&sha);
  if (mbedtls_sha256_starts(&sha, 0) ||
      esp_ota_begin(target_partition, info.image_size, &ota_handle) != ESP_OK) {
    mbedtls_sha256_free(&sha);
    set_error("Could not start OTA write");
    return false;
  }
  ota_open = true;
  info.state = PS_UPDATE_DOWNLOADING;
  if (!ps_net_fetch_stream(image_url, info.image_size, write_image, NULL,
                           PS_UPDATE_IMAGE_GENERATION)) {
    set_error("Network busy or offline");
    mbedtls_sha256_free(&sha);
    return false;
  }
  return true;
}

bool ps_update_finish_download(int status, size_t received) {
  if (info.state != PS_UPDATE_DOWNLOADING) return false;
  if (status != 200 || received != info.image_size ||
      info.downloaded != info.image_size) {
    char reason[sizeof(info.error)];
    snprintf(reason, sizeof(reason), "Transfer %u/%u bytes, HTTP %d",
             (unsigned)info.downloaded, (unsigned)info.image_size, status);
    mbedtls_sha256_free(&sha);
    set_error(reason);
    return false;
  }
  if (mbedtls_sha256_finish(&sha, actual_hash)) {
    mbedtls_sha256_free(&sha);
    set_error("Image hash failed");
    return false;
  }
  mbedtls_sha256_free(&sha);
  char hash[65];
  for (int i = 0; i < 32; i++) snprintf(hash + i * 2, 3, "%02x", actual_hash[i]);
  if (strcmp(hash, expected_hash)) {
    set_error("Image hash mismatch");
    return false;
  }
  esp_err_t rc = esp_ota_end(ota_handle);
  ota_open = false;
  if (rc != ESP_OK) {
    set_error("Image hash or ESP image invalid");
    return false;
  }
  ota_open = false;
  info.state = PS_UPDATE_READY;
  return true;
}

bool ps_update_install(void) {
  if (info.state != PS_UPDATE_READY || !target_partition) return false;
  nvs_handle_t nvs;
  if (nvs_open("system_update", NVS_READWRITE, &nvs) != ESP_OK) return false;
  esp_err_t rc = nvs_set_u32(nvs, "candidate", info.build);
  if (rc == ESP_OK) rc = nvs_commit(nvs);
  nvs_close(nvs);
  if (rc == ESP_OK) rc = esp_ota_set_boot_partition(target_partition);
  if (rc != ESP_OK) {
    ps_update_clear_pending();
    set_error("Could not select update slot");
    return false;
  }
  info.state = PS_UPDATE_INSTALLING;
  return true;
}

const ps_update_info *ps_update_get(void) { return &info; }
void ps_update_read(ps_update_info *out) {
  if (!out) return;
  *out = info;
  out->downloaded = __atomic_load_n(&info.downloaded, __ATOMIC_RELAXED);
}

uint32_t ps_update_pending_build(void) {
  nvs_handle_t nvs;
  uint32_t build = 0;
  if (nvs_open("system_update", NVS_READONLY, &nvs) == ESP_OK) {
    nvs_get_u32(nvs, "candidate", &build);
    nvs_close(nvs);
  }
  return build;
}

bool ps_update_confirm_pending(void) {
  uint32_t build = ps_update_pending_build();
  if (!build) return true;
  nvs_handle_t nvs;
  if (nvs_open("system_update", NVS_READWRITE, &nvs) != ESP_OK) return false;
  esp_err_t rc = nvs_set_u32(nvs, "installed", build);
  if (rc == ESP_OK) rc = nvs_erase_key(nvs, "candidate");
  if (rc == ESP_OK) rc = nvs_commit(nvs);
  nvs_close(nvs);
  return rc == ESP_OK;
}

void ps_update_clear_pending(void) {
  nvs_handle_t nvs;
  if (nvs_open("system_update", NVS_READWRITE, &nvs) != ESP_OK) return;
  esp_err_t rc = nvs_erase_key(nvs, "candidate");
  if (rc == ESP_OK || rc == ESP_ERR_NVS_NOT_FOUND) nvs_commit(nvs);
  nvs_close(nvs);
}
