#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { PS_STORAGE_OS, PS_STORAGE_OTA, PS_STORAGE_PACKAGES,
       PS_STORAGE_DATA, PS_STORAGE_SYSTEM, PS_STORAGE_FREE, PS_STORAGE_PARTS };
typedef struct { uint32_t total, bytes[PS_STORAGE_PARTS]; } ps_storage_layout;
bool ps_storage_layout_make(uint32_t flash, uint32_t slots, uint32_t image,
                            uint32_t package_partition, uint32_t package_total,
                            uint32_t package_used, uint32_t data_partition,
                            ps_storage_layout *out);
unsigned ps_storage_layout_edge(const ps_storage_layout *layout, unsigned count, unsigned width);
