#include "trust.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "trusted_key.h"
#include <stdio.h>
#include <string.h>
bool ps_trust_key_id(const char *pem, char out[17]) {
  if (!pem || strlen(pem) > 511)
    return false;
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  int rc = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)pem,
                                       strlen(pem) + 1);
  unsigned char der[512];
  int n = -1;
  if (!rc && mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA) &&
      mbedtls_pk_get_bitlen(&pk) == 2048)
    n = mbedtls_pk_write_pubkey_der(&pk, der, sizeof(der));
  mbedtls_pk_free(&pk);
  if (n <= 0)
    return false;
  unsigned char hash[32];
  mbedtls_sha256(der + sizeof(der) - n, n, hash, 0);
  for (int i = 0; i < 8; i++)
    sprintf(out + i * 2, "%02x", hash[i]);
  return true;
}
bool ps_trust_lookup(const char *id, char *pem, size_t cap) {
  if (!id || strlen(id) != 16)
    return false;
  char root[17];
  if (ps_trust_key_id(ps_trusted_key, root) && !strcmp(id, root)) {
    if (cap < sizeof(ps_trusted_key))
      return false;
    memcpy(pem, ps_trusted_key, sizeof(ps_trusted_key));
    return true;
  }
  nvs_handle_t h;
  if (nvs_open("publishers", NVS_READONLY, &h) != ESP_OK)
    return false;
  char key[16];
  snprintf(key, sizeof(key), "p%.14s", id);
  bool ok = nvs_get_str(h, key, pem, &cap) == ESP_OK;
  nvs_close(h);
  if (ok) {
    char actual[17];
    ok = ps_trust_key_id(pem, actual) && !strcmp(actual, id);
  }
  return ok;
}
bool ps_trust_add(const char *pem) {
  char id[17];
  if (!ps_trust_key_id(pem, id))
    return false;
  nvs_handle_t h;
  if (nvs_open("publishers", NVS_READWRITE, &h) != ESP_OK)
    return false;
  nvs_iterator_t it = NULL;
  unsigned count = 0;
  esp_err_t rc = nvs_entry_find("nvs", "publishers", NVS_TYPE_STR, &it);
  while (rc == ESP_OK) {
    count++;
    rc = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  char key[16];
  snprintf(key, sizeof(key), "p%.14s", id);
  size_t len = 0;
  bool existing = nvs_get_str(h, key, NULL, &len) == ESP_OK;
  bool ok = (existing || count < 4) && nvs_set_str(h, key, pem) == ESP_OK &&
            nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

bool ps_trust_native_allowed(const char *id) {
  char root[17];
  return id && ps_trust_key_id(ps_trusted_key, root) && !strcmp(id, root);
}
