#pragma once
#include "esp_transport.h"
#include <stdbool.h>
typedef struct {
  esp_transport_list_handle_t list;
  esp_transport_handle_t socket, websocket;
  connect_func connect_socket;
  char ip[16];
} ps_ws_transport;
bool ps_ws_transport_open(ps_ws_transport *t, const char *ip, const char *host,
                          bool tls, const char *path, const char *headers);
void ps_ws_transport_free(ps_ws_transport *t);
