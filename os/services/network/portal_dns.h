#pragma once
#include <stddef.h>
#include <stdint.h>
/* One ordinary IN question; A -> setup AP, AAAA/other types -> NODATA.
 * Compressed names, malformed/oversized/response packets rejected. In-place OK. */
size_t ps_portal_dns_reply(const uint8_t *query,size_t size,uint8_t *out,size_t capacity);
