#pragma once
#include <stdbool.h>
#include <stdint.h>
bool ps_asset_probe(uint32_t token, char hash[65], unsigned *rows,
                    unsigned *scratch);
bool ps_icon_fonts_ready(void);
