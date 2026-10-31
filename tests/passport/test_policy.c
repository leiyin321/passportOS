#include "policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  const char *d = "api.open-meteo.com";
  assert(ps_endpoint_allowed("https", d, 443, 0x08080808, d, 0, 0));
  assert(!ps_endpoint_allowed("http", d, 80, 0x08080808, d, 0, 0));
  unsigned blocked[] = {0,          0x7f000001, 0x0a000001, 0xac100001,
                        0xc0a80101, 0xa9fe0101, 0x64400001, 0xe0000001,
                        0xc0000201, 0xc6120001};
  for (unsigned i = 0; i < sizeof(blocked) / sizeof(*blocked); i++)
    assert(!ps_endpoint_allowed("https", d, 443, blocked[i], d, 0, 0));
  assert(ps_endpoint_allowed("ws", "192.168.1.10", 8041, 0xc0a8010a, NULL,
                             0xc0a8010a, 8041));
  assert(!ps_endpoint_allowed("ws", "192.168.1.10", 8042, 0xc0a8010a, NULL,
                              0xc0a8010a, 8041));
  assert(!ps_endpoint_allowed("ws", "192.168.1.11", 8041, 0xc0a8010b, NULL,
                              0xc0a8010a, 8041));
  assert(!ps_endpoint_allowed("https", "evil", 443, 0x08080808, d, 0, 0));
  assert(
      !ps_endpoint_allowed("https", d, 443, 0x7f000001, d, 0xc0a8010a, 8041));
  assert(ps_public_audio_http(d, 80, 0x08080808, d));
  assert(!ps_public_audio_http(d, 443, 0x08080808, d));
  assert(!ps_public_audio_http(d, 80, 0x08080808, "evil"));
  for (unsigned i = 0; i < sizeof(blocked) / sizeof(*blocked); i++)
    assert(!ps_public_audio_http(d, 80, blocked[i], d));
  puts("N00-N06 PASS public TLS, fixed LAN grant, forbidden addresses, changed "
       "DNS/port/domain denied");
}
