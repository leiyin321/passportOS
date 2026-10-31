#include "headers.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  ps_headers h;
  const char *valid =
      "Device-Id: device\r\nAuthorization: Bearer token\r\nX-Empty: \r\n";
  assert(sizeof(h) < 1100);
  assert(ps_headers_parse(&h, valid, strlen(valid), 4));
  assert(h.count == 3);
  assert(!strcmp(h.data + h.values[1], "Bearer token"));
  char out[1025];
  size_t n;
  assert(ps_headers_format(&h, out, sizeof(out), &n));
  assert(n == strlen(valid) && !strcmp(valid, out));
  assert(!ps_headers_format(&h, out, 8, &n));
  assert(!ps_headers_parse(&h, valid, strlen(valid), 2));
  const char *invalid[] = {
      "Host: attacker\r\n", "Content-Length: 1\r\n", "Sec-WebSocket-Key: a\r\n",
      "A: 1\r\na: 2\r\n",   "A: hello\nworld\r\n",   "A: hello\rworld\r\n",
      "Bad key: a\r\n",     ": value\r\n",           "A: 1",
      "A: 1\r\n\r\n"};
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    assert(!ps_headers_parse(&h, invalid[i], strlen(invalid[i]), 8));
  const char embedded[] = "X: abc\0def\r\n";
  assert(!ps_headers_parse(&h, embedded, sizeof(embedded) - 1, 8));
  assert(ps_headers_parse(&h, NULL, 0, 8));
  assert(ps_headers_format(&h, out, sizeof(out), &n) && n == 0);
  for (size_t i = 1; i < strlen(valid); i++) {
    bool ok = ps_headers_parse(&h, valid, i, 8);
    if (ok)
      assert(i >= 2 && valid[i - 2] == '\r' && valid[i - 1] == '\n');
  }
  printf("HTTP headers PASS compact_bytes=%zu "
         "bounds/roundtrip/injection/duplicates\n",
         sizeof(h));
}
