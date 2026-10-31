#pragma once
#include <stdbool.h>
#include <stddef.h>
#define ESP_OK 0
typedef int esp_err_t;
typedef struct transport *esp_transport_handle_t;
typedef struct list *esp_transport_list_handle_t;
typedef int (*connect_func)(esp_transport_handle_t,const char*,int,int);
struct transport {connect_func _connect; esp_transport_handle_t parent;};
esp_transport_list_handle_t esp_transport_list_init(void);
int esp_transport_list_add(esp_transport_list_handle_t,esp_transport_handle_t,const char*);
int esp_transport_list_destroy(esp_transport_list_handle_t);
int esp_transport_destroy(esp_transport_handle_t);
esp_transport_handle_t esp_transport_ssl_init(void);
esp_transport_handle_t esp_transport_tcp_init(void);
void esp_transport_ssl_set_common_name(esp_transport_handle_t,const char*);
void esp_transport_ssl_crt_bundle_attach(esp_transport_handle_t,int(*)(void*));
int esp_crt_bundle_attach(void*);
typedef struct {const char *ws_path,*headers; bool propagate_control_frames;} esp_transport_ws_config_t;
esp_transport_handle_t esp_transport_ws_init(esp_transport_handle_t);
int esp_transport_ws_set_config(esp_transport_handle_t,const esp_transport_ws_config_t*);
