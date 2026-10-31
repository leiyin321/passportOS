#include "policy.h"
#include <string.h>
bool ps_ip_lan(uint32_t ip) {
  return (ip >> 24) == 10 || (ip >> 20) == 0xac1 || (ip >> 16) == 0xc0a8;
}
bool ps_ip_public(uint32_t ip) {
  unsigned a = ip >> 24, b = (ip >> 16) & 255;
  if (a == 0 || a == 10 || a == 127 || a >= 224 ||
      (a == 100 && b >= 64 && b <= 127) || (a == 169 && b == 254) ||
      (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168))
    return false;
  if ((ip >> 8) == 0xc00000 || (ip >> 8) == 0xc00002 || (ip >> 8) == 0xc63364 ||
      (ip >> 8) == 0xcb0071 || (a == 198 && (b == 18 || b == 19)))
    return false;
  return true;
}
bool ps_endpoint_allowed(const char *scheme, const char *host, unsigned port,
                         uint32_t ip, const char *domain, uint32_t lan_ip,
                         unsigned lan_port) {
  if (!scheme || !host || !port || port > 65535)
    return false;
  bool secure = !strcmp(scheme, "https") || !strcmp(scheme, "wss");
  bool local = !strcmp(scheme, "http") || !strcmp(scheme, "ws");
  if (!secure && !local)
    return false;
  if (lan_ip && ip == lan_ip && port == lan_port && ps_ip_lan(ip))
    return true;
  return secure && port == 443 && domain && *domain && !strcmp(host, domain) &&
         ps_ip_public(ip);
}

bool ps_public_audio_http(const char *host,unsigned port,uint32_t ip,const char *domain) {
  return host && domain && *domain && !strcmp(host,domain) && port==80 && ps_ip_public(ip);
}
