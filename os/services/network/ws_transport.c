#include "ws_transport.h"
#include "esp_crt_bundle.h"
#include "esp_transport_ssl.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ws.h"
// ESP-IDF 5.5.3 has no public dial-address override. Preserve the native
// socket/error/context callbacks and replace only connect on our own handle.
#include "esp_transport_internal.h"
#include <string.h>

static ps_ws_transport *active;
static int connect_pinned(esp_transport_handle_t t, const char *host, int port, int timeout) {
  (void)host;
  if (!active || active->socket != t) return -1;
  return active->connect_socket(t, active->ip, port, timeout);
}
void ps_ws_transport_free(ps_ws_transport *t) {
  if (t->list) esp_transport_list_destroy(t->list);
  if (active == t) active = NULL;
  memset(t, 0, sizeof(*t));
}
static bool own(ps_ws_transport *t, esp_transport_handle_t h, const char *name) {
  if (!h) return false;
  if (esp_transport_list_add(t->list, h, name) == ESP_OK) return true;
  esp_transport_destroy(h);
  return false;
}
bool ps_ws_transport_open(ps_ws_transport *t, const char *ip, const char *host,
                          bool tls, const char *path, const char *headers) {
  // OS permits one foreground WebSocket; never replace a live transport.
  if (active) return false;
  memset(t, 0, sizeof(*t));
  if (!ip || !*ip || strlen(ip) >= sizeof(t->ip)) return false;
  strcpy(t->ip, ip);
  t->list = esp_transport_list_init();
  if (!t->list) return false;
  t->socket = tls ? esp_transport_ssl_init() : esp_transport_tcp_init();
  if (!own(t, t->socket, "socket")) goto fail;
  if (tls) {
    esp_transport_ssl_set_common_name(t->socket, host);
    esp_transport_ssl_crt_bundle_attach(t->socket, esp_crt_bundle_attach);
  }
  t->connect_socket = t->socket->_connect;
  t->socket->_connect = connect_pinned;
  active = t;
  t->websocket = esp_transport_ws_init(t->socket);
  if (!own(t, t->websocket, "websocket")) goto fail;
  esp_transport_ws_config_t cfg = {.ws_path = path, .headers = headers,
                                   .propagate_control_frames = true};
  if (esp_transport_ws_set_config(t->websocket, &cfg) != ESP_OK) goto fail;
  return true;
fail:
  ps_ws_transport_free(t);
  return false;
}
