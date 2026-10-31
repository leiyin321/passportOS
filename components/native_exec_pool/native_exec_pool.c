/* ESP32-C3 / ESP-IDF 5.5.3-specific, trusted Native experiment.
 * Keep ordinary IRAM RX and the heap NX. Only this reserved arena is RWX.
 * Configure it before startup locks PMS; never change permissions at runtime. */
#include <stdint.h>
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_log.h"
#include "esp_memprot.h"
#include "esp_private/esp_memprot_internal.h"
#include "native_exec_pool.h"

#define POOL_BYTES (CONFIG_NATIVE_EXEC_POOL_KIB * 1024u)
static uint32_t arena[POOL_BYTES / 4] __attribute__((section(".iram1.native_exec_arena"), aligned(512), used));
static size_t used_bytes;
static size_t peak_bytes;
static bool ready;
extern esp_err_t __real_esp_mprot_set_prot(const esp_memp_config_t *config);

esp_err_t __wrap_esp_mprot_set_prot(const esp_memp_config_t *config)
{
    if (!config || !config->lock_feature || esp_cpu_dbgr_is_attached()) return ESP_ERR_NOT_SUPPORTED;
    void *main_split = NULL;
    esp_err_t err = esp_mprot_get_default_main_split_addr(MEMPROT_TYPE_IRAM0_SRAM, &main_split);
    if (err) return err;
    if (((uintptr_t)arena & 511u) || (uintptr_t)(arena + POOL_BYTES / 4) > (uintptr_t)main_split) return ESP_ERR_INVALID_SIZE;
    esp_memp_config_t initial = *config;
    initial.lock_feature = false;
    err = __real_esp_mprot_set_prot(&initial);
    if (err) return err;
#define CHECK(call) do { err = (call); if (err) return err; } while (0)
    CHECK(esp_mprot_set_split_addr(MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_SPLIT_ADDR_IRAM0_LINE_0, arena, 0));
    CHECK(esp_mprot_set_split_addr(MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_SPLIT_ADDR_IRAM0_LINE_1, arena + POOL_BYTES / 4, 0));
    CHECK(esp_mprot_set_pms_area(MEMPROT_PMS_AREA_IRAM0_1, MEMPROT_OP_READ | MEMPROT_OP_WRITE | MEMPROT_OP_EXEC, 0));
    const esp_mprot_mem_t types[] = {MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_TYPE_DRAM0_SRAM, MEMPROT_TYPE_IRAM0_RTCFAST};
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        CHECK(esp_mprot_set_split_addr_lock(types[i], 0));
        CHECK(esp_mprot_set_pms_lock(types[i], 0));
        CHECK(esp_mprot_set_monitor_lock(types[i], 0));
    }
    ready = true;
    return ESP_OK;
}

esp_err_t native_exec_verify(void)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err;
    void *begin = NULL, *end = NULL;
    uint32_t flags;
    bool value;
    CHECK(esp_mprot_get_split_addr(MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_SPLIT_ADDR_IRAM0_LINE_0, &begin, 0));
    CHECK(esp_mprot_get_split_addr(MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_SPLIT_ADDR_IRAM0_LINE_1, &end, 0));
    if (begin != arena || end != arena + POOL_BYTES / 4) return ESP_ERR_INVALID_STATE;
    const esp_mprot_pms_area_t areas[] = {MEMPROT_PMS_AREA_IRAM0_0,MEMPROT_PMS_AREA_IRAM0_1,MEMPROT_PMS_AREA_IRAM0_2,MEMPROT_PMS_AREA_IRAM0_3};
    const uint32_t permissions[] = {5,7,5,0};
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(esp_mprot_get_pms_area(areas[i], &flags, 0));
        if (flags != permissions[i]) return ESP_ERR_INVALID_STATE;
    }
    const esp_mprot_pms_area_t dram_areas[] = {MEMPROT_PMS_AREA_DRAM0_0,MEMPROT_PMS_AREA_DRAM0_1,MEMPROT_PMS_AREA_DRAM0_2,MEMPROT_PMS_AREA_DRAM0_3};
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(esp_mprot_get_pms_area(dram_areas[i], &flags, 0));
        if (flags != (i ? 3u : 0u)) return ESP_ERR_INVALID_STATE;
    }
    const esp_mprot_mem_t types[] = {MEMPROT_TYPE_IRAM0_SRAM, MEMPROT_TYPE_DRAM0_SRAM, MEMPROT_TYPE_IRAM0_RTCFAST};
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(esp_mprot_get_split_addr_lock(types[i], &value, 0)); if (!value) return ESP_ERR_INVALID_STATE;
        CHECK(esp_mprot_get_pms_lock(types[i], &value, 0)); if (!value) return ESP_ERR_INVALID_STATE;
        CHECK(esp_mprot_get_monitor_lock(types[i], &value, 0)); if (!value) return ESP_ERR_INVALID_STATE;
        CHECK(esp_mprot_get_monitor_en(types[i], &value, 0)); if (!value) return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

void *native_exec_alloc(size_t size)
{
    if (native_exec_verify() != ESP_OK || used_bytes || !size || size > POOL_BYTES) return NULL;
    used_bytes = (size + 3u) & ~3u;
    if (used_bytes > peak_bytes) peak_bytes = used_bytes;
    return arena;
}
bool native_exec_owns(const void *ptr) { return ptr == arena; }
void native_exec_sync(void) { __asm__ volatile("fence.i" ::: "memory"); }
void native_exec_free(void *ptr)
{
    if (ptr != arena || !used_bytes) return;
    volatile uint32_t *words = arena;
    for (size_t i = 0; i < used_bytes / 4; ++i) words[i] = 0;
    native_exec_sync();
    used_bytes = 0;
}
size_t native_exec_capacity(void) { return POOL_BYTES; }
size_t native_exec_used(void) { return used_bytes; }
size_t native_exec_peak(void) { return peak_bytes; }
void native_exec_dump(void)
{
    ESP_LOGI("native_pool", "ready=%d verify=%d begin=%p end=%p capacity=%u used=%u peak=%u", ready, native_exec_verify(), arena, arena + POOL_BYTES / 4, (unsigned)POOL_BYTES, (unsigned)used_bytes, (unsigned)peak_bytes);
}

bool native_exec_is_clear(void)
{
    const volatile uint32_t *words = arena;
    for (size_t i = 0; i < POOL_BYTES / 4; ++i) if (words[i]) return false;
    return true;
}
