#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
extern const uint8_t ps_hid_report_map[];
extern const size_t ps_hid_report_map_size;
bool ps_hid_report_valid(unsigned id,const void *data,size_t bytes);
