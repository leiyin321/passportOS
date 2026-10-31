#include "network.h"
#include "headers.h"
#include "response.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "policy.h"
#include "redirect.h"
#include "wifi.h"
#include "ws_transport.h"
#include <stdatomic.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
typedef struct {
  char url[512], host[128], scheme[8], ip[16];
  unsigned port;
  char path[384];
} endpoint;
typedef struct {
  endpoint ep;
  uint32_t generation;
  char requested[512], domains[4][128];
  bool system, websocket, custom_http;
  char method[5];
  ps_headers headers;
  char *body;
  size_t body_size;
  bool download;
  bool stream, media, public_audio_http;
  ps_net_stream_done_fn stream_done;
  size_t expected_size;
  ps_net_stream_write_fn stream_write;
  void *stream_context;
} job;
_Static_assert(sizeof(job) <= 4096, "Network job must fit fragmented application heap");
static void job_free(job *j) {
  if (j) { free(j->body); free(j); }
}
static QueueHandle_t jobs, results, ws_events;
static EventGroupHandle_t done;
static atomic_bool cancelled, busy;
static uint32_t local_ip;
static unsigned local_http, local_ws;
static char trusted_store[128];
static char trusted_update[128];
static uint32_t trusted_update_lan_ip;
static unsigned trusted_update_lan_port;
static esp_websocket_client_handle_t ws;
static ps_ws_transport ws_transport;
static char ws_ip[16], ws_host[128];
static endpoint ws_endpoint;
static atomic_uint ws_rx_bytes;
static unsigned ws_tx;
static unsigned ws_tx_bytes, ws_rx_second;
static char ws_headers[1025];
static char ws_rx_buffer[2049];
static size_t ws_rx_size, ws_rx_frame_size, ws_rx_frame_offset;
static int ws_rx_opcode;
static bool ws_rx_active;
static atomic_bool ws_violation;
typedef struct { size_t size; bool binary; char data[]; } ws_tx_job;
static QueueHandle_t ws_tx_jobs;
static EventGroupHandle_t ws_tx_done;
static TaskHandle_t ws_tx_task;
static atomic_bool ws_tx_running;
static atomic_uint ws_tx_pending_bytes;
static void ws_sender(void *arg) {
  (void)arg;
  for (;;) {
    ws_tx_job *j=NULL;
    if (xQueueReceive(ws_tx_jobs,&j,portMAX_DELAY)!=pdTRUE) continue;
    if (!j) break;
    if (!atomic_load(&ws_tx_running)) { atomic_fetch_sub(&ws_tx_pending_bytes,j->size);free(j); break; }
    int sent=j->binary ? esp_websocket_client_send_bin(ws,j->data,j->size,pdMS_TO_TICKS(1000))
                       : esp_websocket_client_send_text(ws,j->data,j->size,pdMS_TO_TICKS(1000));
    bool ok=sent==(int)j->size;
    atomic_fetch_sub(&ws_tx_pending_bytes,j->size);free(j);
    if (!ok) { atomic_store(&ws_violation,true); break; }
  }
  atomic_store(&ws_tx_running,false);
  ws_tx_job *j;
  while (xQueueReceive(ws_tx_jobs,&j,0)==pdTRUE) { if(j)atomic_fetch_sub(&ws_tx_pending_bytes,j->size);free(j); }
  xEventGroupSetBits(ws_tx_done,1);
  vTaskSuspend(NULL);
}
static bool ws_sender_start(void) {
  ws_tx_jobs=xQueueCreate(8,sizeof(ws_tx_job *));
  atomic_store(&ws_tx_pending_bytes,0);
  ws_tx_done=xEventGroupCreate();
  if (!ws_tx_jobs || !ws_tx_done) return false;
  atomic_store(&ws_tx_running,true);
  return xTaskCreate(ws_sender,"passport_ws_tx",3072,NULL,3,&ws_tx_task)==pdPASS;
}
static bool ws_sender_stop(void) {
  atomic_store(&ws_tx_running,false);
  if (ws_tx_task) {
    ws_tx_job *wake=NULL;
    (void)xQueueSend(ws_tx_jobs,&wake,0);
    if (!(xEventGroupWaitBits(ws_tx_done,1,false,true,pdMS_TO_TICKS(3000))&1)) {
      ESP_LOGE("passport_ws","TX task cleanup timeout"); return false;
    }
    vTaskDelete(ws_tx_task);ws_tx_task=NULL;
  }
  if (ws_tx_jobs) {
    ws_tx_job *j;while(xQueueReceive(ws_tx_jobs,&j,0)==pdTRUE){if(j)atomic_fetch_sub(&ws_tx_pending_bytes,j->size);free(j);}
    vQueueDelete(ws_tx_jobs);ws_tx_jobs=NULL;
  }
  if (ws_tx_done) { vEventGroupDelete(ws_tx_done);ws_tx_done=NULL; }
  return true;
}

static int64_t ws_second;
#include "esp_timer.h"
static bool allowed_internal(const char domains[4][128], const char *url, bool system,
                    endpoint *ep,bool audio_http) {
  if (!url || strlen(url) > 500 || strchr(url, '@') || strchr(url, '#') ||
      strchr(url, '\\') || strchr(url, '\r') || strchr(url, '\n'))
    return false;
  const char *split = strstr(url, "://");
  if (!split || split - url > 5)
    return false;
  memset(ep, 0, sizeof(*ep));
  memcpy(ep->scheme, url, split - url);
  const char *start = split + 3, *path = strchr(start, '/');
  if (!path)
    path = start + strlen(start);
  size_t n = path - start;
  if (!n || n >= 128)
    return false;
  char authority[128];
  memcpy(authority, start, n);
  authority[n] = 0;
  char *port = strchr(authority, ':');
  ep->port =
      (!strcmp(ep->scheme, "https") || !strcmp(ep->scheme, "wss")) ? 443 : 80;
  if (port) {
    *port++ = 0;
    if (!*port)
      return false;
    char *end;
    unsigned long v = strtoul(port, &end, 10);
    if (*end || !v || v > 65535)
      return false;
    ep->port = v;
  }
  if (!*authority || strchr(authority, '[') || strchr(authority, '%'))
    return false;
  for (char *p = authority; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' ||
          *p == '-'))
      return false;
  strcpy(ep->host, authority);
  snprintf(ep->path, sizeof(ep->path), "%s", *path ? path : "/");
  // Reject names before DNS; only approved domain or literal local endpoint may
  // resolve.
  struct in_addr literal;
  unsigned chosen_port =
      (!strcmp(ep->scheme, "ws") || !strcmp(ep->scheme, "wss")) ? local_ws
                                                                : local_http;
  bool local_literal = inet_pton(AF_INET, ep->host, &literal) == 1 &&
                       ntohl(literal.s_addr) == local_ip &&
                       ep->port == chosen_port;
  bool local_update = system && trusted_update_lan_ip &&
                      !strcmp(ep->scheme, "http") &&
                      inet_pton(AF_INET, ep->host, &literal) == 1 &&
                      ntohl(literal.s_addr) == trusted_update_lan_ip &&
                      ep->port == trusted_update_lan_port;
  const char *domain = NULL;
  for (int i = 0; i < 4; i++)
    if (*domains[i] && !strcmp(ep->host, domains[i]))
      domain = domains[i];
  bool plain=audio_http && !strcmp(ep->scheme,"http");
  if(plain && (!domain || ep->port!=80 || strchr(ep->path,'?')))return false;
  bool named = domain && *domain && !strcmp(ep->host, domain) &&
               ep->port == 443 &&
               (!strcmp(ep->scheme, "https") || !strcmp(ep->scheme, "wss"));
  named=named || (plain && domain && ep->port==80);
  if (!named && !local_literal && !local_update)
    return false;
  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM},
                  *addresses = NULL;
  if (getaddrinfo(ep->host, NULL, &hints, &addresses))
    return false;
  uint32_t ip =
      ntohl(((struct sockaddr_in *)addresses->ai_addr)->sin_addr.s_addr);
  freeaddrinfo(addresses);
  esp_netif_ip_info_t own;
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif && esp_netif_get_ip_info(netif, &own) == ESP_OK &&
      ip == ntohl(own.ip.addr))
    return false;
  unsigned lan_port = (!strcmp(ep->scheme, "ws") || !strcmp(ep->scheme, "wss"))
                          ? local_ws
                          : local_http;
  uint32_t allowed_lan_ip = local_update ? trusted_update_lan_ip : local_ip;
  unsigned allowed_lan_port =
      local_update ? trusted_update_lan_port : lan_port;
  if(plain && !ps_public_audio_http(ep->host,ep->port,ip,domain))return false;
  if (!plain && !ps_endpoint_allowed(ep->scheme, ep->host, ep->port, ip, domain,
                           allowed_lan_ip, allowed_lan_port))
    return false;
  if (system && ip != local_ip && !local_update &&
      (!domain || (strcmp(domain, trusted_store) &&
                   strcmp(domain, trusted_update))))
    return false;
  snprintf(ep->ip, sizeof(ep->ip), "%u.%u.%u.%u", (unsigned)(ip >> 24),
           (unsigned)((ip >> 16) & 255), (unsigned)((ip >> 8) & 255),
           (unsigned)(ip & 255));
  snprintf(ep->url, sizeof(ep->url), "%s://%s:%u%s", ep->scheme, ep->ip,
           ep->port, ep->path);
  return true;
}
bool ps_net_local_grant(const char *ip, unsigned hp, unsigned wp) {
  struct in_addr a;
  if (!ip || inet_pton(AF_INET, ip, &a) != 1 || !ps_ip_lan(ntohl(a.s_addr)) ||
      !hp || !wp || hp > 65535 || wp > 65535)
    return false;
  local_ip = ntohl(a.s_addr);
  local_http = hp;
  local_ws = wp;
  return true;
}
typedef struct {
  char location[512];
  bool valid;
  bool icy_metadata;
} redirect_target;
static esp_err_t header_event(esp_http_client_event_t *ev) {
  redirect_target *target = ev->user_data;
  if (ev->event_id == HTTP_EVENT_ON_HEADER && ev->header_key &&
      ev->header_value && !strcasecmp(ev->header_key, "Location")) {
    size_t n = strlen(ev->header_value);
    target->valid = n > 0 && n <= 500;
    if (target->valid)
      memcpy(target->location, ev->header_value, n + 1);
  }
  if(ev->event_id==HTTP_EVENT_ON_HEADER && ev->header_key && ev->header_value &&
     !strcasecmp(ev->header_key,"icy-metaint") && strcmp(ev->header_value,"0"))
    target->icy_metadata=true;
  return ESP_OK;
}
bool ps_net_set_store(const char *url) {
  if (!url || strlen(url) > 250 || strncmp(url, "https://", 8))
    return false;
  const char *end = strchr(url + 8, '/');
  if (!end)
    return false;
  size_t n = end - url - 8;
  if (!n || n >= sizeof(trusted_store))
    return false;
  for (size_t i = 0; i < n; i++) {
    char c = url[8 + i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
          c == '-'))
      return false;
  }
  if (!memchr(url + 8, '.', n))
    return false;
  memcpy(trusted_store, url + 8, n);
  trusted_store[n] = 0;
  return true;
}
bool ps_net_set_update_source(const char *url) {
  if (!url || strlen(url) > 500 || strchr(url, '@') || strchr(url, '#') ||
      strchr(url, '?') || strchr(url, '\\'))
    return false;
  if (!strncmp(url, "http://", 7)) {
    const char *authority = url + 7;
    const char *path = strchr(authority, '/');
    const char *colon = strchr(authority, ':');
    if (!path || !colon || colon >= path || colon == authority ||
        (size_t)(colon - authority) >= INET_ADDRSTRLEN)
      return false;
    char host[INET_ADDRSTRLEN];
    memcpy(host, authority, colon - authority);
    host[colon - authority] = 0;
    char port_text[6];
    size_t port_len = path - colon - 1;
    if (!port_len || port_len >= sizeof(port_text)) return false;
    memcpy(port_text, colon + 1, port_len);
    port_text[port_len] = 0;
    char *end = NULL;
    unsigned long port = strtoul(port_text, &end, 10);
    struct in_addr address;
    if (*end || !port || port > 65535 || inet_pton(AF_INET, host, &address) != 1)
      return false;
    uint32_t ip = ntohl(address.s_addr);
    if (!ps_ip_lan(ip)) return false;
    trusted_update_lan_ip = ip;
    trusted_update_lan_port = (unsigned)port;
    snprintf(trusted_update, sizeof(trusted_update), "%s", host);
    return true;
  }
  if (strncmp(url, "https://", 8)) return false;
  const char *host = url + 8;
  const char *path = strchr(host, '/');
  if (!path || path == host || (size_t)(path - host) >= sizeof(trusted_update))
    return false;
  size_t n = (size_t)(path - host);
  bool dot = false;
  for (size_t i = 0; i < n; i++) {
    char c = host[i];
    if (c == '.') dot = true;
    else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
      return false;
  }
  if (!dot || host[n - 1] == '.' || host[n - 1] == '-') return false;
  memcpy(trusted_update, host, n);
  trusted_update[n] = 0;
  trusted_update_lan_ip = 0;
  trusted_update_lan_port = 0;
  return true;
}
void ps_net_clear_update_source(void) {
  trusted_update[0] = 0;
  trusted_update_lan_ip = 0;
  trusted_update_lan_port = 0;
}
static bool start_ws(const endpoint *ep, const ps_headers *headers);
static void worker(void *arg) {
  (void)arg;
  job *j = NULL;
  for (;;) {
    if (xQueueReceive(jobs, &j, portMAX_DELAY) != pdTRUE)
      continue;
    if (!j)
      continue;
    wifi_ps_type_t saved_ps=WIFI_PS_NONE;
    bool restore_ps=(j->media || j->stream || j->download) && esp_wifi_get_ps(&saved_ps)==ESP_OK &&
                    saved_ps!=WIFI_PS_NONE && esp_wifi_set_ps(WIFI_PS_NONE)==ESP_OK;
    ps_net_result result = {.generation = j->generation, .status = -1};
    const char *failure_stage = "policy";
    esp_err_t open_error = ESP_FAIL;
    int response_status = 0;
    int64_t content_length = -1;
    size_t response_bytes = 0;
    bool permitted = !atomic_load(&cancelled) &&
                     allowed_internal(j->domains, j->requested, j->system, &j->ep,j->public_audio_http) &&
                     !atomic_load(&cancelled);
    if (permitted && j->websocket) {
      if (!strcmp(j->ep.scheme, "ws") || !strcmp(j->ep.scheme, "wss")) {
        result.status = start_ws(&j->ep, &j->headers) ? 101 : -1;
      }
    } else if (permitted && (!strcmp(j->ep.scheme, "http") ||
                             !strcmp(j->ep.scheme, "https"))) {
      int64_t deadline = esp_timer_get_time() + (j->media ? 86400000000LL : j->stream ? 900000000 : j->system ? 120000000 : 8000000);
      size_t limit = j->stream ? j->expected_size : j->download ? PS_PAP_MAX :
                     j->custom_http ? 8192 : j->system ? 12288 : 4096;
      // Keep response storage out of the TLS handshake peak. Grow it only
      // when payload arrives, bounded by the per-operation response limit.
      char *body = NULL;
      size_t body_capacity = 0;
      failure_stage = "client allocation";
      FILE *download = j->download ? fopen("/cache/download.pap", "wb") : NULL;
      unsigned char block[512];
      for (unsigned hop = 0;
           hop <= PS_REDIRECT_MAX && !atomic_load(&cancelled) &&
           esp_timer_get_time() < deadline;
           hop++) {
        redirect_target target = {0};
        esp_http_client_config_t cfg = {.url = j->ep.url,
#if CONFIG_MBEDTLS_DYNAMIC_BUFFER
                                        /* Long playback must retain its full RX
                                         * record before lazy codec allocations
                                         * fragment the remaining internal heap. */
                                        .tls_dyn_buf_strategy = j->media ? HTTP_TLS_DYN_BUF_RX_STATIC : 0,
#endif
                                        .event_handler = header_event,
                                        .user_data = &target,
                                        .common_name = j->ep.host,
                                        .crt_bundle_attach =
                                            esp_crt_bundle_attach,
                                        .timeout_ms = j->media ? 3000 : j->stream ? 60000
                                                               : j->system ? 15000 : 3000,
                                        .disable_auto_redirect = true,
                                        .buffer_size = 1024,
                                        .buffer_size_tx = 768};
        esp_http_client_handle_t h =
            (!j->download || download) ? esp_http_client_init(&cfg) : NULL;
        bool follow = false;
        if (h) {
          failure_stage = "open";
          esp_http_client_set_method(h, !strcmp(j->method, "POST")
                                           ? HTTP_METHOD_POST : HTTP_METHOD_GET);
          esp_http_client_set_header(h, "Host", j->ep.host);
          if(j->media) {
            esp_http_client_set_header(h,"Icy-MetaData","0");
            esp_http_client_set_header(h,"User-Agent","PassportOS-Media/1");
          }
          for (unsigned hi = 0; hi < j->headers.count; hi++)
            esp_http_client_set_header(h, j->headers.data + j->headers.names[hi],
                                       j->headers.data + j->headers.values[hi]);
          if (!atomic_load(&cancelled) &&
              (open_error = esp_http_client_open(h, j->body_size)) == ESP_OK) {
            failure_stage = "write";
            size_t written = 0;
            while (written < j->body_size && !atomic_load(&cancelled) &&
                   esp_timer_get_time() < deadline) {
              int n = esp_http_client_write(h, j->body + written, j->body_size - written);
              if (n <= 0) break;
              written += (size_t)n;
            }
            bool request_written = written == j->body_size;
            if (request_written) failure_stage = "headers";
            int64_t length = esp_http_client_fetch_headers(h);
            bool unknown_length = esp_http_client_is_chunked_response(h) || (j->media && length==0);
            content_length = unknown_length ? -1 : length;
            int status = esp_http_client_get_status_code(h);
            response_status = status;
            size_t size = 0;
            if (request_written && !strcmp(j->method, "GET") && ps_redirect_status(status) &&
                target.valid && hop < PS_REDIRECT_MAX) {
              char next[512];
              if (ps_redirect_url(j->requested, target.location, next,
                                  sizeof(next))) {
                snprintf(j->requested, sizeof(j->requested), "%s", next);
                follow = true;
              }
            }
            if (!follow && request_written && !(j->media && target.icy_metadata) && ps_http_response_allowed(status, length,
                    unknown_length, limit, j->custom_http,
                    j->stream && !j->media ? j->expected_size : 0)) {
              bool read_failed = false;
              unsigned idle_reads=0;
              failure_stage = "read";
              while (!atomic_load(&cancelled) &&
                     esp_timer_get_time() < deadline && size < limit) {
                int r = esp_http_client_read(
                    h, (char *)block,
                    limit - size > 512 ? 512 : limit - size);
                if(j->media && r==-ESP_ERR_HTTP_EAGAIN && idle_reads++<3)continue;
                if(r>0)idle_reads=0;
                if (r < 0) {
                  read_failed = true;
                  if (j->stream)
                    ESP_LOGE("passport_http",
                             "HTTP stream read failed rc=%d after=%u/%u",
                             r, (unsigned)size, (unsigned)limit);
                  break;
                }
                if (r == 0) {
                  if (j->stream && size < limit)
                    ESP_LOGE("passport_http",
                             "HTTP stream closed after=%u/%u",
                             (unsigned)size, (unsigned)limit);
                  break;
                }
                if (!j->download && !j->stream) {
                  size_t needed = size + (size_t)r + 1;
                  if (needed > body_capacity) {
                    size_t capacity = (needed + 1023) & ~(size_t)1023;
                    if (capacity > limit + 1) capacity = limit + 1;
                    char *grown = realloc(body, capacity);
                    if (!grown) {
                      failure_stage = "response allocation";
                      read_failed = true;
                      break;
                    }
                    body = grown;
                    body_capacity = capacity;
                  }
                  memcpy(body + size, block, (size_t)r);
                  body[size + (size_t)r] = 0;
                }
                if (download && fwrite(block, 1, r, download) != (size_t)r) {
                  read_failed = true;
                  break;
                }
                if (j->stream && !j->stream_write(block, (size_t)r,
                                                j->stream_context)) {
                  read_failed = true;
                  ESP_LOGE("passport_http",
                           "HTTP stream sink rejected after=%u/%u",
                           (unsigned)size, (unsigned)limit);
                  break;
                }
                size += r;
              }
              // Consume a final chunk trailer without allowing limit+1 payload bytes.
              if (size == limit && !read_failed && !atomic_load(&cancelled) &&
                  !esp_http_client_is_complete_data_received(h) &&
                  esp_timer_get_time() < deadline) {
                char extra;
                read_failed = esp_http_client_read(h, &extra, 1) != 0;
              }
              response_bytes = size;
              if (!atomic_load(&cancelled) &&
                  ps_http_response_complete(length, unknown_length, size, limit,
                      esp_http_client_is_complete_data_received(h), read_failed)) {
                result.status = j->custom_http ? status : 200;
                result.size = size;
                failure_stage = "complete";
                if (j->download) {
                  result.file = true;
                  result.data = strdup("/cache/download.pap");
                } else if (!j->stream) {
                  result.data = body;
                  body = NULL;
                }
              }
            }
          }
          esp_http_client_close(h);
          esp_http_client_cleanup(h);
        }
        if (!follow)
          break;
        char previous_host[128], previous_scheme[8];
        unsigned previous_port = j->ep.port;
        snprintf(previous_host, sizeof(previous_host), "%s", j->ep.host);
        snprintf(previous_scheme, sizeof(previous_scheme), "%s", j->ep.scheme);
        if (!allowed_internal(j->domains, j->requested, j->system, &j->ep,j->public_audio_http))
          break;
        if (j->custom_http && j->headers.count &&
            (strcmp(previous_host, j->ep.host) || previous_port != j->ep.port ||
             strcmp(previous_scheme, j->ep.scheme)))
          break;
      }
      if (download) {
        if (fclose(download) && result.status == 200) {
          free(result.data);
          result.data = NULL;
          result.status = -1;
        }
        if (result.status != 200)
          unlink("/cache/download.pap");
      }
      free(body);
    }
    if (j->custom_http) {
      if (result.status < 0)
        ESP_LOGW("passport_http",
                 "MiniApp HTTPS failed stage=%s open=%s status=%d length=%lld received=%u permitted=%d",
                 failure_stage, esp_err_to_name(open_error), response_status,
                 (long long)content_length, (unsigned)response_bytes, permitted);
      else
        ESP_LOGI("passport_http", "MiniApp HTTPS complete status=%d bytes=%u",
                 result.status, (unsigned)result.size);
    }
    if(restore_ps)(void)esp_wifi_set_ps(saved_ps);
    if(j->media) {
      j->stream_done(result.status,j->stream_context);
      free(result.data);
    } else if (xQueueSend(results, &result, 0) != pdTRUE)
      free(result.data);
    job_free(j);
    xEventGroupSetBits(done, 1);
    atomic_store(&busy, false);
  }
}
static void job_domains(job *j, const ps_package *app, bool system) {
  if (app) {
    cJSON *domains = cJSON_GetObjectItemCaseSensitive(app->manifest, "domains");
    int i = 0;
    for (cJSON *d = domains ? domains->child : NULL; d && i < 4; d = d->next)
      if (cJSON_IsString(d)) snprintf(j->domains[i++], 128, "%s", d->valuestring);
  }
  if (system) {
    if (*trusted_store) snprintf(j->domains[0], 128, "%s", trusted_store);
    if (*trusted_update && strcmp(trusted_store, trusted_update))
      snprintf(j->domains[1], 128, "%s", trusted_update);
  }
}
static bool submit_job(job *j) {
  // Ownership transfers here: the queue/worker consumes accepted jobs, while
  // every rejected submission is freed before returning.
  if (!j) return false;
  bool online = ps_wifi_online();
  bool websocket_active = ws != NULL;
  bool worker_busy = atomic_load(&busy);
  if (!online || websocket_active || worker_busy) {
    if (j->custom_http)
      ESP_LOGW("passport_http",
               "MiniApp HTTPS not queued wifi=%d websocket=%d worker=%d",
               online, websocket_active, worker_busy);
    job_free(j);
    return false;
  }
  atomic_store(&cancelled, false);
  atomic_store(&busy, true);
  xEventGroupClearBits(done, 1);
  if (xQueueSend(jobs, &j, 0) != pdTRUE) {
    xEventGroupSetBits(done, 1);
    atomic_store(&busy, false);
    job_free(j);
    return false;
  }
  return true;
}
static bool enqueue(const ps_package *app, const char *url, uint32_t generation,
                    bool system, bool websocket, bool stream,
                    size_t expected_size, ps_net_stream_write_fn write,
                    void *context) {
  if (!url || strlen(url) > 500 || (stream && (!system || !expected_size || !write)))
    return false;
  job *j = calloc(1, sizeof(*j));
  if (!j) return false;
  *j = (job){.generation = generation,
           .system = system,
           .websocket = websocket,
           .download = system && generation == PS_NET_PACKAGE_GENERATION,
           .stream = stream,
           .expected_size = expected_size,
           .stream_write = write,
           .stream_context = context};
  snprintf(j->requested, sizeof(j->requested), "%s", url);
  snprintf(j->method, sizeof(j->method), "GET");
  job_domains(j, app, system);
  return submit_job(j);
}
void ps_net_init(void) {
  ESP_LOGI("passport_http", "Network job bytes=%u headers=%u",
           (unsigned)sizeof(job), (unsigned)sizeof(ps_headers));
  jobs = xQueueCreate(1, sizeof(job *));
  results = xQueueCreate(1, sizeof(ps_net_result));
  done = xEventGroupCreate();
  configASSERT(jobs && results && done);
  xEventGroupSetBits(done, 1);
  configASSERT(xTaskCreate(worker, "passport_http", 6144, NULL, 3, NULL) ==
               pdPASS);
}
bool ps_net_fetch(const ps_package *app, const char *url, uint32_t generation,
                  bool system) {
  if (!system && (!app || !(app->permissions & PS_FETCH)))
    return false;
  return enqueue(app, url, generation, system, false, false, 0, NULL, NULL);
}
bool ps_net_request(const ps_package *app, const char *method, const char *url,
                    const char *headers, size_t headers_size,
                    const char *body, size_t body_size, uint32_t generation) {
  if (!app || !(app->permissions & PS_FETCH)) {
    ESP_LOGW("passport_http", "MiniApp HTTPS rejected reason=permission");
    return false;
  }
  if (!method || !url || strncmp(url, "https://", 8) || strlen(url) > 500 ||
      (strcmp(method, "GET") && strcmp(method, "POST")) || body_size > 2048 ||
      (body_size && (!body || strcmp(method, "POST"))) || headers_size > 1024) {
    ESP_LOGW("passport_http", "MiniApp HTTPS rejected reason=arguments");
    return false;
  }
  job *j = calloc(1, sizeof(*j));
  if (!j) {
    ESP_LOGW("passport_http", "MiniApp HTTPS rejected reason=job-memory");
    return false;
  }
  j->generation = generation;
  j->custom_http = true;
  snprintf(j->method, sizeof(j->method), "%s", method);
  snprintf(j->requested, sizeof(j->requested), "%s", url);
  j->body_size = body_size;
  if (body_size) {
    j->body = malloc(body_size);
    if (!j->body) {
      ESP_LOGW("passport_http", "MiniApp HTTPS rejected reason=body-memory");
      job_free(j); return false;
    }
    memcpy(j->body, body, body_size);
  }
  if (!ps_headers_parse(&j->headers, headers, headers_size, 8)) {
    ESP_LOGW("passport_http", "MiniApp HTTPS rejected reason=headers");
    job_free(j);
    return false;
  }
  job_domains(j, app, false);
  bool queued = submit_job(j);
  if (queued)
    ESP_LOGI("passport_http", "MiniApp HTTPS queued method=%s", method);
  return queued;
}
bool ps_net_fetch_stream(const char *url, size_t expected_size,
                         ps_net_stream_write_fn write, void *context,
                         uint32_t generation) {
  return enqueue(NULL, url, generation, true, false, true, expected_size,
                 write, context);
}

bool ps_net_poll(ps_net_result *out) {
  return xQueueReceive(results, out, 0) == pdTRUE;
}
void ps_net_cancel(void) {
  atomic_store(&cancelled, true);
  if (atomic_load(&busy))
    xEventGroupWaitBits(done, 1, false, true, pdMS_TO_TICKS(16000));
  ps_net_result r;
  while (ps_net_poll(&r)) {
    if (r.file && r.data)
      unlink(r.data);
    free(r.data);
  }
  ps_net_ws_close();
}
typedef struct {
  int opcode;
  size_t size;
  char data[2049];
} ws_message;
static uint8_t ws_rx_frame_opcode;
static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)arg;
  (void)base;
  if (id != WEBSOCKET_EVENT_DATA)
    return;
  esp_websocket_event_data_t *e = data;
  if (e->data_len < 0 || e->payload_len < 0 || e->payload_offset < 0 ||
      (e->data_len > 0 && !e->data_ptr) ||
      e->data_len > e->payload_len || e->payload_offset > e->payload_len ||
      e->data_len > e->payload_len - e->payload_offset) {
    atomic_store(&ws_violation, true);
    return;
  }
  unsigned second = (unsigned)(esp_timer_get_time() / 1000000);
  if (second != ws_rx_second) { ws_rx_second = second; atomic_store(&ws_rx_bytes, 0); }
  if (atomic_fetch_add(&ws_rx_bytes, e->data_len) + (unsigned)e->data_len > 32768) {
    atomic_store(&ws_violation, true);
    return;
  }
  if (e->payload_offset == 0) {
    ws_rx_frame_opcode = e->op_code;
    ws_rx_frame_size = (size_t)e->payload_len;
    ws_rx_frame_offset = 0;
    if (e->op_code == 1 || e->op_code == 2) {
      if (ws_rx_active) { atomic_store(&ws_violation, true); return; }
      ws_rx_active = true;
      ws_rx_size = 0;
      ws_rx_opcode = e->op_code;
    } else if (e->op_code != 0 || !ws_rx_active) {
      if (e->op_code == 9 || e->op_code == 10) return;
      atomic_store(&ws_violation, true); return;
    }
    if (ws_rx_frame_size > sizeof(ws_rx_buffer) - 1 - ws_rx_size) {
      atomic_store(&ws_violation, true); ws_rx_active = false; return;
    }
  } else if (!ws_rx_active || e->op_code != ws_rx_frame_opcode ||
             (size_t)e->payload_offset != ws_rx_frame_offset) {
    atomic_store(&ws_violation, true); ws_rx_active = false; return;
  }
  if (!ws_rx_active || (size_t)e->payload_offset != ws_rx_frame_offset ||
      ws_rx_size + (size_t)e->data_len > sizeof(ws_rx_buffer) - 1) {
    atomic_store(&ws_violation, true); ws_rx_active = false; return;
  }
  if (e->data_len) memcpy(ws_rx_buffer + ws_rx_size, e->data_ptr, e->data_len);
  ws_rx_size += (size_t)e->data_len;
  ws_rx_frame_offset += (size_t)e->data_len;
  if (ws_rx_frame_offset == ws_rx_frame_size && e->fin) {
    ws_message m = {.opcode = ws_rx_opcode, .size = ws_rx_size};
    memcpy(m.data, ws_rx_buffer, ws_rx_size);
    m.data[ws_rx_size] = 0;
    ws_rx_active = false;
    /* Worker callback only: bounded TCP backpressure instead of disconnecting
     * on a normal TTS/message burst. No LVGL or button callback is blocked. */
    if (xQueueSend(ws_events, &m, pdMS_TO_TICKS(1000)) != pdTRUE) {
      ESP_LOGW("passport_ws","receive queue stalled for 1000ms");
      atomic_store(&ws_violation, true);
    }
  }
}
bool ps_net_ws_open(const ps_package *app, const char *url,
                    const char *headers, size_t headers_size,
                    uint32_t generation) {
  if (!app || !(app->permissions & PS_WS) || !url || strlen(url) > 500 ||
      strncmp(url, "wss://", 6))
    return false;
  if (ws) {
    if (esp_websocket_client_is_connected(ws)) return false;
    ps_net_ws_close();
    if (ws || ws_tx_task) return false;
  }
  job *j = calloc(1, sizeof(*j));
  if (!j) return false;
  j->generation = generation;
  j->websocket = true;
  snprintf(j->method, sizeof(j->method), "GET");
  snprintf(j->requested, sizeof(j->requested), "%s", url);
  job_domains(j, app, false);
  if (!ps_headers_parse(&j->headers, headers, headers_size, 4)) {
    job_free(j);
    return false;
  }
  return submit_job(j);
}
static bool start_ws(const endpoint *ep, const ps_headers *headers) {
  ws_endpoint = *ep;
  strcpy(ws_ip, ws_endpoint.ip);
  strcpy(ws_host, ws_endpoint.host);
  atomic_store(&ws_rx_bytes, 0);
  atomic_store(&ws_violation, false);
  ws_rx_size = ws_rx_frame_size = ws_rx_frame_offset = 0;
  ws_rx_active = false;
  ws_tx = 0;
  ws_tx_bytes = 0;
  if (!ws_events)
  ws_events = xQueueCreate(2, sizeof(ws_message));
  if (!ws_events)
    return false;
  xQueueReset(ws_events);
  size_t headers_size;
  if (!ps_headers_format(headers, ws_headers, sizeof(ws_headers), &headers_size))
    return false;
  if (!ps_ws_transport_open(&ws_transport, ws_ip, ws_host,
                            !strcmp(ws_endpoint.scheme, "wss"),
                            ws_endpoint.path, ws_headers)) return false;
  esp_websocket_client_config_t cfg = {
      .host = ws_host,
      .ext_transport = ws_transport.websocket,
      .port = ws_endpoint.port,
      .path = ws_endpoint.path,
      .cert_common_name = ws_host,
      .transport = !strcmp(ws_endpoint.scheme, "wss")
                       ? WEBSOCKET_TRANSPORT_OVER_SSL
                       : WEBSOCKET_TRANSPORT_OVER_TCP,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .disable_auto_reconnect = true,
      .network_timeout_ms = 5000,
      .task_stack = 4096,
      .buffer_size = 1024,
      .headers = ws_headers};
  ws = esp_websocket_client_init(&cfg);
  if (!ws) {
    ps_ws_transport_free(&ws_transport);
    return false;
  }
  esp_websocket_register_events(ws, WEBSOCKET_EVENT_ANY, ws_event, NULL);
  if (!ws_sender_start()) { ps_net_ws_close(); return false; }
  if (esp_websocket_client_start(ws) != ESP_OK) {
    ps_net_ws_close();
    return false;
  }
  return true;
}
bool ps_net_ws_send(const char *data, size_t n, bool binary) {
  int64_t second = esp_timer_get_time() / 1000000;
  if (second != ws_second) {
    ws_second = second;
    ws_tx = 0;
    ws_tx_bytes = 0;
  }
  if (atomic_load(&busy) || !ws || n > 2048 || ws_tx >= 50 ||
      ws_tx_bytes + n > 16384 ||
      !esp_websocket_client_is_connected(ws))
    return false;
  if (!ws_tx_jobs || !atomic_load(&ws_tx_running) || (n && !data)) return false;
  unsigned pending=atomic_fetch_add(&ws_tx_pending_bytes,n);
  if (pending+n>4096) { atomic_fetch_sub(&ws_tx_pending_bytes,n);return false; }
  ws_tx_job *j=malloc(sizeof(*j)+n);
  if (!j) { atomic_fetch_sub(&ws_tx_pending_bytes,n);return false; }
  j->size=n;j->binary=binary;
  if(n)memcpy(j->data,data,n);
  if (xQueueSend(ws_tx_jobs,&j,pdMS_TO_TICKS(40))!=pdTRUE) { atomic_fetch_sub(&ws_tx_pending_bytes,n);free(j);return false; }
  ws_tx++;ws_tx_bytes+=n;
  return true;
}
bool ps_net_ws_connected(void) {
  return ws && esp_websocket_client_is_connected(ws);
}
bool ps_net_ws_poll(char *data, size_t cap, size_t *size, int *opcode) {
  if (atomic_load(&ws_violation)) {
    ps_net_ws_close();
    return false;
  }
  ws_message m;
  if (!ws_events || xQueueReceive(ws_events, &m, 0) != pdTRUE)
    return false;
  if (cap < m.size + 1)
    return false;
  memcpy(data, m.data, m.size);
  data[m.size] = 0;
  *size = m.size;
  *opcode = m.opcode;
  return true;
}
void ps_net_ws_close(void) {
  if (!ws_sender_stop()) return;
  if (ws) {
    esp_websocket_client_stop(ws);
    esp_websocket_client_destroy(ws);
    ws = NULL;
  }
  ps_ws_transport_free(&ws_transport);
  if (ws_events)
    xQueueReset(ws_events);
  ws_rx_active = false;
  ws_rx_size = ws_rx_frame_size = ws_rx_frame_offset = 0;
}

void ps_net_local_revoke(void) {
  ps_net_cancel();
  local_ip = 0;
  local_http = local_ws = 0;
}

bool ps_net_media_stream(const ps_package *app,const char *url,
                         ps_net_stream_write_fn write,ps_net_stream_done_fn finish,
                         void *context,uint32_t generation) {
  if(!app || !(app->permissions&PS_FETCH) || !(app->permissions&PS_AUDIO_PLAYBACK) ||
     !url || (strncmp(url,"https://",8) && (!(app->permissions&PS_AUDIO_HTTP) || strncmp(url,"http://",7))) || strlen(url)>500 || !write || !finish)return false;
  job *j=calloc(1,sizeof(*j));if(!j)return false;
  j->generation=generation;j->stream=true;j->media=true;j->public_audio_http=(app->permissions&PS_AUDIO_HTTP)!=0;
  j->expected_size=512u*1024u*1024u;j->stream_write=write;
  j->stream_done=finish;j->stream_context=context;
  snprintf(j->requested,sizeof(j->requested),"%s",url);
  snprintf(j->method,sizeof(j->method),"GET");job_domains(j,app,false);
  return submit_job(j);
}

void ps_net_cancel_pending(void){atomic_store(&cancelled,true);}
