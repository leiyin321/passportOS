#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_LOGI(tag, ...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define configASSERT assert
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY (-1)
#define pdMS_TO_TICKS(x) (x)
typedef void *QueueHandle_t;
typedef void *TaskHandle_t;
typedef void *EventGroupHandle_t;
QueueHandle_t xQueueCreate(unsigned cap, size_t size);
int xQueueSend(QueueHandle_t q,const void *in,int wait);
int xQueueReceive(QueueHandle_t q,void *out,int wait);
void xQueueReset(QueueHandle_t q);
void vQueueDelete(QueueHandle_t q);
EventGroupHandle_t xEventGroupCreate(void);
unsigned xEventGroupSetBits(EventGroupHandle_t g,unsigned bits);
unsigned xEventGroupClearBits(EventGroupHandle_t g,unsigned bits);
unsigned xEventGroupWaitBits(EventGroupHandle_t g,unsigned bits,bool clear,bool all,int ticks);
int xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,void *arg,unsigned priority,void *task);
void vTaskSuspend(TaskHandle_t task);
void vTaskDelete(TaskHandle_t task);
void vEventGroupDelete(EventGroupHandle_t group);
int64_t esp_timer_get_time(void);
const char *esp_err_to_name(int rc);
int esp_crt_bundle_attach(void *conf);
typedef void esp_netif_t;
typedef struct {struct {uint32_t addr;} ip;} esp_netif_ip_info_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *name);
int esp_netif_get_ip_info(esp_netif_t *netif,esp_netif_ip_info_t *out);
int fake_dns(const char *name,const char *service,const struct addrinfo *hints,struct addrinfo **out);
void fake_dns_free(struct addrinfo *address);
typedef struct { int event_id; const char *header_key,*header_value; void *user_data; } esp_http_client_event_t;
#define HTTP_EVENT_ON_HEADER 1
#define HTTP_METHOD_POST 1
#define HTTP_METHOD_GET 0
#define ESP_ERR_HTTP_EAGAIN 0x7007
typedef int wifi_ps_type_t;
#define WIFI_PS_NONE 0
#define WIFI_PS_MIN_MODEM 1
int esp_wifi_get_ps(wifi_ps_type_t *);
int esp_wifi_set_ps(wifi_ps_type_t);
#define CONFIG_MBEDTLS_DYNAMIC_BUFFER 1
#define HTTP_TLS_DYN_BUF_RX_STATIC 1
typedef struct {
 int tls_dyn_buf_strategy;
 const char *url,*common_name;
 esp_err_t(*event_handler)(esp_http_client_event_t*);
 void *user_data;
 int(*crt_bundle_attach)(void*);
 int timeout_ms,buffer_size,buffer_size_tx;
 bool disable_auto_redirect;
} esp_http_client_config_t;
typedef struct fake_client *esp_http_client_handle_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg);
int esp_http_client_set_method(esp_http_client_handle_t h,int method);
int esp_http_client_set_header(esp_http_client_handle_t h,const char *name,const char *value);
int esp_http_client_open(esp_http_client_handle_t h,int size);
int esp_http_client_write(esp_http_client_handle_t h,const char *body,int size);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h);
bool esp_http_client_is_chunked_response(esp_http_client_handle_t h);
int esp_http_client_get_status_code(esp_http_client_handle_t h);
int esp_http_client_read(esp_http_client_handle_t h,char *out,int size);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h);
int esp_http_client_close(esp_http_client_handle_t h);
int esp_http_client_cleanup(esp_http_client_handle_t h);
typedef void *esp_websocket_client_handle_t;
typedef const char *esp_event_base_t;
typedef struct {int data_len,payload_len,payload_offset,op_code; const char *data_ptr; bool fin;} esp_websocket_event_data_t;
#define WEBSOCKET_EVENT_DATA 1
#define WEBSOCKET_EVENT_ANY (-1)
#define WEBSOCKET_TRANSPORT_OVER_SSL 1
#define WEBSOCKET_TRANSPORT_OVER_TCP 0
typedef struct {
 void *ext_transport;
 const char *host,*path,*cert_common_name,*headers;
 unsigned port;
 int transport,network_timeout_ms,task_stack,buffer_size;
 int(*crt_bundle_attach)(void*);
 bool disable_auto_reconnect;
} esp_websocket_client_config_t;
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *cfg);
int esp_websocket_register_events(esp_websocket_client_handle_t h,int event,void(*fn)(void*,esp_event_base_t,int32_t,void*),void *arg);
int esp_websocket_client_start(esp_websocket_client_handle_t h);
int esp_websocket_client_stop(esp_websocket_client_handle_t h);
int esp_websocket_client_destroy(esp_websocket_client_handle_t h);
bool esp_websocket_client_is_connected(esp_websocket_client_handle_t h);
int esp_websocket_client_send_bin(esp_websocket_client_handle_t h,const char *d,int n,int wait);
int esp_websocket_client_send_text(esp_websocket_client_handle_t h,const char *d,int n,int wait);
