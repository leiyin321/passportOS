#include "response.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  assert(ps_http_response_allowed(200, 0, true, 8192, true, 0));
  assert(ps_http_response_complete(0, true, 512, 8192, true, false));
  assert(ps_http_response_complete(0, true, 8192, 8192, true, false));
  assert(!ps_http_response_complete(0, true, 8193, 8192, true, false));
  assert(!ps_http_response_allowed(200, -1, true, 8192, true, 0));
  assert(!ps_http_response_allowed(200, -0x7007, false, 8192, true, 0));
  assert(!ps_http_response_allowed(0, 0, true, 8192, true, 0));
  assert(ps_http_response_allowed(429, 20, false, 8192, true, 0));
  assert(!ps_http_response_allowed(429, 20, false, 8192, false, 0));
  assert(!ps_http_response_complete(20, false, 19, 8192, true, false));
  assert(!ps_http_response_complete(20, false, 20, 8192, false, false));
  assert(!ps_http_response_complete(20, false, 20, 8192, true, true));
  assert(ps_http_response_complete(0, false, 0, 8192, true, false));
  assert(!ps_http_response_allowed(200, 0, true, 2000000, false, 2000000));
  assert(!ps_http_response_allowed(200, 2000001, false, 2000000, false, 2000000));
  assert(ps_http_response_allowed(200, 2000000, false, 2000000, false, 2000000));
  puts("HTTP response PASS chunked/EOF/zero/truncation/timeout/limit/OTA exact length");
}
