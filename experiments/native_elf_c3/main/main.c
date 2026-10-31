#include <inttypes.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "esp_elf.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#if CONFIG_NATIVE_EXEC_POOL
#include "native_exec_pool.h"
#include "soc/soc.h"
#endif

extern const uint8_t native_app_elf_start[] asm("_binary_native_app_elf_start");
extern const uint8_t native_app_elf_end[] asm("_binary_native_app_elf_end");
static const char *TAG = "native_poc";

static void memory(const char *phase, unsigned cycle)
{
#if CONFIG_NATIVE_EXEC_POOL
    native_exec_dump();
#endif
    ESP_LOGI(TAG, "MEM %s cycle=%u exec_free=%u exec_min=%u exec_largest=%u 8bit_free=%u 8bit_min=%u 8bit_largest=%u",
             phase, cycle,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_EXEC),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_EXEC),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_EXEC),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static void unsupported_relocation(void)
{
    const size_t size = (size_t)(native_app_elf_end - native_app_elf_start);
    const size_t exec_before = heap_caps_get_free_size(MALLOC_CAP_EXEC);
    const size_t data_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uint8_t *payload = malloc(size);
    if (!payload) { ESP_LOGE(TAG, "NEGATIVE allocation failed"); return; }
    memcpy(payload, native_app_elf_start, size);
    elf32_hdr_t *header = (void *)payload;
    bool patched = false;
    if (header->shoff <= size && header->shnum <= (size - header->shoff) / sizeof(elf32_shdr_t)) {
        elf32_shdr_t *sections = (void *)(payload + header->shoff);
        for (unsigned i = 0; i < header->shnum; ++i) {
            if (sections[i].type == SHT_RELA && sections[i].size >= sizeof(elf32_rela_t) &&
                sections[i].offset <= size - sizeof(elf32_rela_t)) {
                elf32_rela_t *relocation = (void *)(payload + sections[i].offset);
                relocation->info = (relocation->info & ~255u) | 2u; /* R_RISCV_64: unsupported. */
                patched = true;
                break;
            }
        }
    }
    esp_elf_t elf = {0};
    int rc = esp_elf_init(&elf);
    if (!rc && patched) rc = esp_elf_relocate(&elf, payload);
    esp_elf_deinit(&elf);
    free(payload);
    const int exec_drift = (int)heap_caps_get_free_size(MALLOC_CAP_EXEC) - (int)exec_before;
    const int data_drift = (int)heap_caps_get_free_size(MALLOC_CAP_8BIT) - (int)data_before;
    ESP_LOGI(TAG, "NEGATIVE type=R_RISCV_64 patched=%d rc=%d exec_drift=%d 8bit_drift=%d result=%s",
             patched, rc, exec_drift, data_drift,
             patched && rc == -EINVAL && !exec_drift && !data_drift ? "PASS" : "FAIL");
}

#if CONFIG_NATIVE_EXEC_POOL
static int IRAM_ATTR protected_text_probe(void) { return 42; }
#endif
void app_main(void)
{
#if CONFIG_NATIVE_EXEC_POOL
    native_exec_dump();
    ESP_ERROR_CHECK(native_exec_verify());
    if (CONFIG_NATIVE_EXEC_PROBE == 1) {
        uint32_t *code = heap_caps_malloc(8, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        code[0] = 0x02a00513; code[1] = 0x00008067;
        native_exec_sync();
        ESP_LOGI(TAG, "PROBE heap_execute expect=MEMPROT_PANIC target=%p", (void *)MAP_DRAM_TO_IRAM((uintptr_t)code));
        ((int (*)(void))MAP_DRAM_TO_IRAM((uintptr_t)code))();
        ESP_LOGE(TAG, "PROBE heap_execute UNEXPECTED_SUCCESS");
        abort();
    }
    if (CONFIG_NATIVE_EXEC_PROBE == 2) {
        volatile uint32_t *code = (volatile uint32_t *)(uintptr_t)protected_text_probe;
        ESP_LOGI(TAG, "PROBE text_write expect=MEMPROT_PANIC target=%p", (void *)code);
        *code = *code;
        ESP_LOGE(TAG, "PROBE text_write UNEXPECTED_SUCCESS");
        abort();
    }
    ESP_LOGI(TAG, "POOL bounds zero_rejected=%d oversize_rejected=%d", native_exec_alloc(0) == NULL, native_exec_alloc(native_exec_capacity() + 1) == NULL);
#endif
#ifdef CONFIG_ESP_SYSTEM_MEMPROT_FEATURE
    const char *protection = "enabled";
#else
    const char *protection = "disabled";
#endif
    ESP_LOGI(TAG, "BEGIN elf_bytes=%u memprot=%s experiment_only", (unsigned)(native_app_elf_end - native_app_elf_start), protection);
    for (unsigned cycle = 0; cycle < 2; ++cycle) {
        memory("before", cycle);
        const size_t exec_before = heap_caps_get_free_size(MALLOC_CAP_EXEC);
        const size_t data_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        esp_elf_t elf = {0};
        int rc = esp_elf_init(&elf);
        if (!rc) rc = esp_elf_relocate(&elf, native_app_elf_start);
        ESP_LOGI(TAG, "relocate cycle=%u rc=%d entry=%p segment=%p vaddr=0x%08" PRIx32,
                 cycle, rc, (void *)elf.entry, (void *)elf.psegment, elf.svaddr);
        memory("loaded", cycle);
        if (rc == -ENOMEM && !exec_before) {
            esp_elf_deinit(&elf);
            ESP_LOGI(TAG, "PROTECTED executable_heap_unavailable result=EXPECTED_BLOCKED");
            ESP_LOGI(TAG, "END");
            return;
        }
        bool ok = false;
        if (!rc) {
            const elf32_hdr_t *header = (const void *)native_app_elf_start;
            const elf32_phdr_t *segments = (const void *)(native_app_elf_start + header->phoff);
            for (unsigned i = 0; i < header->phnum; ++i) {
                if (segments[i].type == PT_LOAD) {
                    ESP_LOGI(TAG, "SEG %u addr=%p filesz=%u memsz=%u flags=%u", i,
                             elf.psegment + segments[i].vaddr - elf.svaddr,
                             (unsigned)segments[i].filesz, (unsigned)segments[i].memsz,
                             (unsigned)segments[i].flags);
                }
            }
#if CONFIG_NATIVE_EXEC_POOL
            ESP_LOGI(TAG, "POOL busy_rejected=%d", native_exec_alloc(4) == NULL);
#endif
            const int first = elf.entry(0, NULL);
            const int second = elf.entry(0, NULL);
            const int request_rc = esp_elf_request(&elf, 0, 0, NULL);
            ok = first == 42 && second == 43 && request_rc == 0;
            ESP_LOGI(TAG, "CALL cycle=%u first=%d second=%d request_rc=%d result=%s", cycle, first, second, request_rc, ok ? "PASS" : "FAIL");
        }
        esp_elf_deinit(&elf);
        memory("unloaded", cycle);
#if CONFIG_NATIVE_EXEC_POOL
        ESP_LOGI(TAG, "POOL after_unload used=%u all_zero=%d", (unsigned)native_exec_used(), native_exec_is_clear());
#endif
        const int exec_drift = (int)heap_caps_get_free_size(MALLOC_CAP_EXEC) - (int)exec_before;
        const int data_drift = (int)heap_caps_get_free_size(MALLOC_CAP_8BIT) - (int)data_before;
        ESP_LOGI(TAG, "CYCLE cycle=%u exec_drift=%d 8bit_drift=%d result=%s", cycle,
                 exec_drift, data_drift, ok && !exec_drift && !data_drift ? "PASS" : "FAIL");
        if (!ok) break;
    }
    unsupported_relocation();
    ESP_LOGI(TAG, "END");
}
