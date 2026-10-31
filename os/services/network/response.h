#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* fetch_length is ESP-IDF's fetch_headers result: negative means failure,
 * zero with unknown=true is chunked/EOF framing, not a zero-byte body. */
static inline bool ps_http_response_allowed(int status, int64_t fetch_length,
    bool unknown, size_t limit, bool custom, size_t expected) {
  return fetch_length >= 0 && status >= 100 && status <= 599 &&
         (custom || status == 200) &&
         (unknown || (uint64_t)fetch_length <= limit) &&
         (!expected || (!unknown && (uint64_t)fetch_length == expected));
}
static inline bool ps_http_response_complete(int64_t fetch_length,
    bool unknown, size_t received, size_t limit, bool parser_complete,
    bool read_failed) {
  return !read_failed && parser_complete && received <= limit &&
         (unknown || (fetch_length >= 0 && (uint64_t)fetch_length == received));
}
