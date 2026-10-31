#pragma once
#include <stddef.h>
#include <stdint.h>
#define PS_OPUS_PART_MAX 256
/* RFC 6716 section 3.2.5: join three equal-size single 20ms CBR Opus frames
 * into one 60ms code-3 packet. Input TOC/config must match. No codec state or
 * borrowed pointers are retained. Returns output size, zero on rejection. */
size_t ps_opus_join_20ms(const uint8_t packets[3][PS_OPUS_PART_MAX], const uint16_t sizes[3],
                        uint8_t *output, size_t capacity);
