#pragma once
#include <stdbool.h>
#include <stdint.h>
// IPv4 is passed in host byte order. IPv6 and noncanonical URLs fail closed in
// v1.
bool ps_ip_public(uint32_t ip);
bool ps_ip_lan(uint32_t ip);
bool ps_endpoint_allowed(const char *scheme, const char *host, unsigned port,
                         uint32_t ip, const char *domain, uint32_t lan_ip,
                         unsigned lan_port);

/* Explicit public broadcast exception, never used by ordinary fetch/WS. */
bool ps_public_audio_http(const char *host,unsigned port,uint32_t ip,const char *domain);
