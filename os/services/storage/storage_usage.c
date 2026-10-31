#include "storage_usage.h"
#include "store.h"
#include "esp_spiffs.h"
#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
static atomic_int state;
static ps_storage_usage snapshot;
static void collect(void *unused) {
  (void)unused;
  memset(&snapshot, 0, sizeof(snapshot));
  snapshot.packages_valid = esp_spiffs_info("packages", &snapshot.total, &snapshot.used) == ESP_OK;
  nvs_stats_t stats;
  snapshot.data_valid = nvs_get_stats("appdata", &stats) == ESP_OK;
  if (snapshot.data_valid) {
    snapshot.data_used = stats.used_entries;
    snapshot.data_available = stats.available_entries;
    snapshot.data_total = stats.total_entries;
  }
  uint32_t flash=0,slots=0;
  esp_partition_iterator_t parts=esp_partition_find(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,NULL);
  esp_partition_iterator_t cursor=parts;
  while(cursor){slots+=esp_partition_get(cursor)->size;cursor=esp_partition_next(cursor);}
  esp_partition_iterator_release(cursor);
  const esp_partition_t *packages=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"packages");
  const esp_partition_t *data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_NVS,"appdata");
  const esp_partition_t *running=esp_ota_get_running_partition();
  esp_image_metadata_t image={0};
  if(running && packages && data && snapshot.packages_valid && esp_flash_get_size(NULL,&flash)==ESP_OK) {
    esp_partition_pos_t position={.offset=running->address,.size=running->size};
    if(esp_image_get_metadata(&position,&image)==ESP_OK)
      snapshot.layout_valid=ps_storage_layout_make(flash,slots,image.image_len,packages->size,snapshot.total,snapshot.used,data->size,&snapshot.layout);
  }
  char ids[12][64];
  snapshot.count = ps_store_registered(ids, 12);
  nvs_handle_t registry;
  bool registry_open = nvs_open_from_partition("appdata", "registry", NVS_READONLY, &registry) == ESP_OK;
  for (unsigned i = 0; i < snapshot.count; ++i) {
    ps_storage_app_usage *app = &snapshot.apps[i];
    snprintf(app->id, sizeof(app->id), "%.63s", ids[i]);
    app->package = ps_store_package_size(ids[i]);
    char file[96]; struct stat st;
    snprintf(file, sizeof(file), "/cache/%.63s.old", ids[i]);
    if (!stat(file, &st) && st.st_size > 0) app->previous = st.st_size;
    if (!registry_open) continue;
    nvs_iterator_t it = NULL;
    esp_err_t rc = nvs_entry_find("appdata", "registry", NVS_TYPE_STR, &it);
    while (rc == ESP_OK) {
      nvs_entry_info_t info; nvs_entry_info(it, &info);
      char value[64]; size_t length = sizeof(value);
      if (strlen(info.key) == 14 && nvs_get_str(registry, info.key, value, &length) == ESP_OK && !strcmp(value, ids[i])) {
        nvs_handle_t data;
        esp_err_t opened = nvs_open_from_partition("appdata", info.key, NVS_READONLY, &data);
        if (opened == ESP_ERR_NVS_NOT_FOUND) app->data_valid = true;
        else if (opened == ESP_OK) {
          app->data_valid = nvs_get_used_entry_count(data, &app->data_entries) == ESP_OK;
          nvs_close(data);
        }
        break;
      }
      rc = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
  }
  if (registry_open) nvs_close(registry);
  atomic_store(&state, 2);
  vTaskDelete(NULL);
}
bool ps_storage_usage_request(void) {
  int idle = 0;
  if (!atomic_compare_exchange_strong(&state, &idle, 1)) return false;
  if (xTaskCreate(collect, "storage_usage", 4096, NULL, 2, NULL) != pdPASS) {
    atomic_store(&state, 0); return false;
  }
  return true;
}
bool ps_storage_usage_take(ps_storage_usage *out) {
  if (!out || atomic_load(&state) != 2) return false;
  *out = snapshot;
  atomic_store(&state, 0);
  return true;
}
