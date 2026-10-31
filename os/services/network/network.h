#pragma once
#include "package.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
  uint32_t generation;
  int status;
  size_t size;
  char *data;
  bool file;
} ps_net_result;
typedef bool (*ps_net_stream_write_fn)(const void *data, size_t size,
                                       void *context);
void ps_net_init(void);
bool ps_net_local_grant(const char *ip, unsigned http_port, unsigned ws_port);
bool ps_net_fetch(const ps_package *app, const char *url, uint32_t generation,
                  bool system);
bool ps_net_request(const ps_package *app, const char *method, const char *url,
                    const char *headers, size_t headers_size,
                    const char *body, size_t body_size, uint32_t generation);
bool ps_net_poll(ps_net_result *out);
void ps_net_cancel(void);
bool ps_net_ws_open(const ps_package *app, const char *url,
                    const char *headers, size_t headers_size,
                    uint32_t generation);
bool ps_net_ws_send(const char *data, size_t size, bool binary);
bool ps_net_ws_connected(void);
bool ps_net_ws_poll(char *data, size_t capacity, size_t *size, int *opcode);
void ps_net_ws_close(void);

bool ps_net_set_store(const char *catalog_url);
bool ps_net_set_update_source(const char *manifest_url);
void ps_net_clear_update_source(void);
bool ps_net_fetch_stream(const char *url, size_t expected_size,
                         ps_net_stream_write_fn write, void *context,
                         uint32_t generation);

#define PS_NET_PACKAGE_GENERATION 0xfffffffeu

void ps_net_local_revoke(void);

/* Foreground MP3 media stream: no buffering entire response; OS sinks only.
 * Completion runs on network worker, after client teardown. Context remains
 * owned by OS until completion; no ELF pointer may be passed. */
typedef void (*ps_net_stream_done_fn)(int status, void *context);
bool ps_net_media_stream(const ps_package *app, const char *url,
                         ps_net_stream_write_fn write,
                         ps_net_stream_done_fn finish, void *context,
                         uint32_t generation);

void ps_net_cancel_pending(void); /* Request cancellation; does not join. */
