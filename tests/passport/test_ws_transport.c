#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../os/services/network/ws_transport.c"
struct list {
  esp_transport_handle_t item[2];
  int n;
};
static int step, fail_at, live;
static char dial[32], authority[128], common_name[128];
static bool verified;
static bool fails(void) { return ++step == fail_at; }
static int dial_socket(esp_transport_handle_t t, const char *host, int port,
                       int timeout) {
  (void)t;
  assert(port == 443 && timeout == 5000);
  snprintf(dial, sizeof(dial), "%s", host);
  return 0;
}
static int handshake(esp_transport_handle_t t, const char *host, int port,
                     int timeout) {
  snprintf(authority, sizeof(authority), "%s", host);
  return t->parent->_connect(t->parent, host, port, timeout);
}
esp_transport_list_handle_t esp_transport_list_init(void) {
  if (fails())
    return NULL;
  struct list *l = calloc(1, sizeof(*l));
  live++;
  return l;
}
int esp_transport_list_add(esp_transport_list_handle_t l,
                           esp_transport_handle_t t, const char *name) {
  (void)name;
  if (fails())
    return -1;
  l->item[l->n++] = t;
  return 0;
}
int esp_transport_destroy(esp_transport_handle_t t) {
  free(t);
  live--;
  return 0;
}
int esp_transport_list_destroy(esp_transport_list_handle_t l) {
  for (int i = 0; i < l->n; i++)
    esp_transport_destroy(l->item[i]);
  free(l);
  live--;
  return 0;
}
esp_transport_handle_t esp_transport_ssl_init(void) {
  if (fails())
    return NULL;
  esp_transport_handle_t t = calloc(1, sizeof(*t));
  t->_connect = dial_socket;
  live++;
  return t;
}
esp_transport_handle_t esp_transport_tcp_init(void) {
  return esp_transport_ssl_init();
}
void esp_transport_ssl_set_common_name(esp_transport_handle_t t,
                                       const char *h) {
  (void)t;
  snprintf(common_name, sizeof(common_name), "%s", h);
}
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t t,
                                         int (*cb)(void *)) {
  (void)t;
  verified = cb == esp_crt_bundle_attach;
}
int esp_crt_bundle_attach(void *c) {
  (void)c;
  return 0;
}
esp_transport_handle_t esp_transport_ws_init(esp_transport_handle_t parent) {
  if (fails())
    return NULL;
  esp_transport_handle_t t = calloc(1, sizeof(*t));
  t->_connect = handshake;
  t->parent = parent;
  live++;
  return t;
}
int esp_transport_ws_set_config(esp_transport_handle_t t,
                                const esp_transport_ws_config_t *c) {
  (void)t;
  assert(!strcmp(c->ws_path, "/socket"));
  assert(c->propagate_control_frames);
  return fails() ? -1 : 0;
}
int main(void) {
  for (int failure = 0; failure <= 6; failure++) {
    step = 0;
    fail_at = failure;
    ps_ws_transport t;
    bool ok = ps_ws_transport_open(&t, "8.8.8.8", "service.example", true,
                                   "/socket", "");
    assert(ok == (failure == 0));
    if (ok) {
      assert(t.websocket->_connect(t.websocket, "service.example", 443, 5000) ==
             0);
      assert(!strcmp(dial, "8.8.8.8"));
      assert(!strcmp(authority, "service.example"));
      assert(!strcmp(common_name, "service.example") && verified);
      ps_ws_transport other = {0};
      assert(!ps_ws_transport_open(&other, "1.1.1.1", "other", true, "/socket",
                                   ""));
    }
    ps_ws_transport_free(&t);
    assert(live == 0 && active == NULL);
  }
  puts("Pinned WebSocket PASS: authority/TLS identity/IP separation, failure "
       "cleanup, singleton");
}
