#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "network_stubs.h"
static void *bounded_malloc(size_t n);
static void *bounded_calloc(size_t a, size_t b);
static void *bounded_realloc(void *p, size_t n);
static void bounded_free(void *p);
#define malloc bounded_malloc
#define calloc bounded_calloc
#define realloc bounded_realloc
#define free bounded_free
#define getaddrinfo fake_dns
#define freeaddrinfo fake_dns_free
#include "../../os/services/network/network.c"
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef getaddrinfo
#undef freeaddrinfo
static size_t live_bytes, peak_bytes, block_limit = 8192;
static unsigned blocks, allocations;
static int fail_allocation;
typedef struct {
  size_t n;
} allocation;
static void *bounded_malloc(size_t n) {
  allocations++;
  if (n > block_limit ||
      (fail_allocation && allocations == (unsigned)fail_allocation))
    return NULL;
  allocation *p = malloc(sizeof(*p) + n);
  assert(p);
  p->n = n;
  blocks++;
  live_bytes += n;
  if (live_bytes > peak_bytes)
    peak_bytes = live_bytes;
  return p + 1;
}
static void *bounded_calloc(size_t a, size_t b) {
  void *p = bounded_malloc(a * b);
  if (p)
    memset(p, 0, a * b);
  return p;
}
static void bounded_free(void *p) {
  if (p) {
    allocation *a = (allocation *)p - 1;
    live_bytes -= a->n;
    blocks--;
    free(a);
  }
}
static void *bounded_realloc(void *p, size_t n) {
  void *q = bounded_malloc(n);
  if (!q)
    return NULL;
  if (p) {
    allocation *a = (allocation *)p - 1;
    memcpy(q, p, a->n < n ? a->n : n);
    bounded_free(p);
  }
  return q;
}
typedef struct {
  unsigned count, cap;
  size_t size;
  char data[];
} queue;
static jmp_buf worker_idle, tx_suspended;
static unsigned tx_sent;
static bool simulate_ws_consumer;
static int ws_wait;
QueueHandle_t xQueueCreate(unsigned cap, size_t size) {
  queue *q = calloc(1, sizeof(*q) + cap * size);
  q->cap = cap;
  q->size = size;
  return q;
}
int xQueueSend(QueueHandle_t v, const void *p, int wait) {
  queue *q = v;
  if (v == ws_events)
    ws_wait = wait;
  if (q->count == q->cap && wait && v == ws_events && simulate_ws_consumer) {
    q->count--;
    memmove(q->data, q->data + q->size, q->count * q->size);
  }
  if (q->count == q->cap)
    return 0;
  memcpy(q->data + q->count++ * q->size, p, q->size);
  return 1;
}
int xQueueReceive(QueueHandle_t v, void *p, int wait) {
  queue *q = v;
  if (!q->count) {
    if (wait == portMAX_DELAY)
      longjmp(worker_idle, 1);
    return 0;
  }
  memcpy(p, q->data, q->size);
  q->count--;
  memmove(q->data, q->data + q->size, q->count * q->size);
  return 1;
}
void xQueueReset(QueueHandle_t q) { ((queue *)q)->count = 0; }
void vQueueDelete(QueueHandle_t q) { free(q); }
EventGroupHandle_t xEventGroupCreate(void) {
  return calloc(1, sizeof(unsigned));
}
unsigned xEventGroupSetBits(EventGroupHandle_t g, unsigned b) {
  return *(unsigned *)g |= b;
}
unsigned xEventGroupClearBits(EventGroupHandle_t g, unsigned b) {
  return *(unsigned *)g &= ~b;
}
unsigned xEventGroupWaitBits(EventGroupHandle_t g, unsigned b, bool c, bool a,
                             int t) {
  (void)b;
  (void)c;
  (void)a;
  (void)t;
  return *(unsigned *)g;
}
void vTaskSuspend(TaskHandle_t t) {
  (void)t;
  longjmp(tx_suspended, 1);
}
void vTaskDelete(TaskHandle_t t) { (void)t; }
void vEventGroupDelete(EventGroupHandle_t g) { free(g); }
int xTaskCreate(void (*f)(void *), const char *n, unsigned s, void *a,
                unsigned p, void *t) {
  (void)f;
  (void)n;
  (void)s;
  (void)a;
  (void)p;
  if (t)
    *(TaskHandle_t *)t = (void *)1;
  return 1;
}
static int64_t clock_offset, open_delay;
int64_t esp_timer_get_time(void) {
  static int64_t t;
  t += 100;
  return t + clock_offset;
}
const char *esp_err_to_name(int r) { return r ? "FAIL" : "OK"; }
int esp_crt_bundle_attach(void *c) {
  (void)c;
  return 0;
}
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *n) {
  (void)n;
  return (void *)1;
}
int esp_netif_get_ip_info(esp_netif_t *n, esp_netif_ip_info_t *out) {
  (void)n;
  out->ip.addr = inet_addr("192.168.1.2");
  return 0;
}
int fake_dns(const char *n, const char *s, const struct addrinfo *h,
             struct addrinfo **out) {
  (void)n;
  (void)s;
  (void)h;
  static struct sockaddr_in ip;
  static struct addrinfo a;
  ip.sin_addr.s_addr = inet_addr("93.184.216.34");
  a.ai_addr = (struct sockaddr *)&ip;
  *out = &a;
  return 0;
}
void fake_dns_free(struct addrinfo *a) { (void)a; }
bool ps_wifi_online(void) { return true; }
static struct response {
  int status;
  int64_t length;
  bool chunked, trailer;
  const char *payload, *location;
  size_t size;
} responses[4];
struct fake_client {
  esp_http_client_config_t cfg;
  struct response *r;
  size_t pos;
  bool complete;
};
static struct fake_client clients[4];
static unsigned opened, reads, writes;
static wifi_ps_type_t current_ps = WIFI_PS_MIN_MODEM;
static unsigned ps_changes;
int esp_wifi_get_ps(wifi_ps_type_t *out) {
  *out = current_ps;
  return ESP_OK;
}
int esp_wifi_set_ps(wifi_ps_type_t value) {
  current_ps = value;
  ps_changes++;
  return ESP_OK;
}
static size_t written;
static const char *metadata_interval;
esp_http_client_handle_t
esp_http_client_init(const esp_http_client_config_t *c) {
  assert(opened < 4);
  struct fake_client *h = &clients[opened];
  *h = (struct fake_client){.cfg = *c, .r = &responses[opened++]};
  return h;
}
int esp_http_client_set_method(esp_http_client_handle_t h, int m) {
  (void)h;
  (void)m;
  return 0;
}
int esp_http_client_set_header(esp_http_client_handle_t h, const char *n,
                               const char *v) {
  (void)h;
  assert(n && v);
  return 0;
}
int esp_http_client_open(esp_http_client_handle_t h, int n) {
  clock_offset += open_delay;
  (void)h;
  (void)n;
  return 0;
}
int esp_http_client_write(esp_http_client_handle_t h, const char *b, int n) {
  (void)h;
  assert(b);
  writes++;
  int take = n > 2 ? 2 : n;
  written += take;
  return take;
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) {
  if (metadata_interval) {
    esp_http_client_event_t ev = {.event_id = HTTP_EVENT_ON_HEADER,
                                  .header_key = "Icy-MetaInt",
                                  .header_value = metadata_interval,
                                  .user_data = h->cfg.user_data};
    h->cfg.event_handler(&ev);
  }
  if (h->r->location) {
    esp_http_client_event_t ev = {.event_id = HTTP_EVENT_ON_HEADER,
                                  .header_key = "Location",
                                  .header_value = h->r->location,
                                  .user_data = h->cfg.user_data};
    h->cfg.event_handler(&ev);
  }
  return h->r->length;
}
bool esp_http_client_is_chunked_response(esp_http_client_handle_t h) {
  return h->r->chunked;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) {
  return h->r->status;
}
int esp_http_client_read(esp_http_client_handle_t h, char *out, int n) {
  reads++;
  size_t take = h->r->size - h->pos;
  if (take > (size_t)n)
    take = n;
  if (take)
    memcpy(out, h->r->payload + h->pos, take);
  h->pos += take;
  if (h->pos == h->r->size && (!h->r->trailer || !take))
    h->complete = true;
  return (int)take;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) {
  return h->complete;
}
int esp_http_client_close(esp_http_client_handle_t h) {
  (void)h;
  return 0;
}
int esp_http_client_cleanup(esp_http_client_handle_t h) {
  (void)h;
  return 0;
}
esp_websocket_client_handle_t
esp_websocket_client_init(const esp_websocket_client_config_t *c) {
  (void)c;
  return (void *)1;
}
int esp_websocket_register_events(esp_websocket_client_handle_t h, int e,
                                  void (*f)(void *, esp_event_base_t, int32_t,
                                            void *),
                                  void *a) {
  (void)h;
  (void)e;
  (void)f;
  (void)a;
  return 0;
}
int esp_websocket_client_start(esp_websocket_client_handle_t h) {
  (void)h;
  return 0;
}
int esp_websocket_client_stop(esp_websocket_client_handle_t h) {
  (void)h;
  return 0;
}
int esp_websocket_client_destroy(esp_websocket_client_handle_t h) {
  (void)h;
  return 0;
}
bool esp_websocket_client_is_connected(esp_websocket_client_handle_t h) {
  return h != NULL;
}
int esp_websocket_client_send_bin(esp_websocket_client_handle_t h,
                                  const char *d, int n, int t) {
  (void)h;
  (void)d;
  (void)t;
  tx_sent++;
  return n;
}
int esp_websocket_client_send_text(esp_websocket_client_handle_t h,
                                   const char *d, int n, int t) {
  return esp_websocket_client_send_bin(h, d, n, t);
}
static void run(void) {
  if (!setjmp(worker_idle))
    worker(NULL);
}
static void reset(void) {
  assert(!blocks);
  clock_offset = open_delay = 0;
  ps_changes = 0;
  metadata_interval = NULL;
  memset(responses, 0, sizeof(responses));
  opened = reads = writes = allocations = 0;
  written = 0;
  fail_allocation = 0;
  peak_bytes = 0;
  block_limit = 8192;
  responses[0] = (struct response){200, 2, false, false, "ok", NULL, 2};
}
static size_t sink_bytes;
static bool sink(const void *data, size_t size, void *context) {
  assert(data);
  sink_bytes += size;
  return context == NULL;
}
static unsigned media_finishes;
static int media_status;
static void media_done(int status, void *context) {
  assert(current_ps == WIFI_PS_MIN_MODEM);
  (void)context;
  media_finishes++;
  media_status = status;
}
static ps_net_result request(ps_package *p, const char *method,
                             const char *body) {
  assert(ps_net_request(p, method, "https://api.example.com/start", NULL, 0,
                        body, body ? strlen(body) : 0, 7));
  run();
  ps_net_result r;
  assert(ps_net_poll(&r));
  assert(r.generation == 7);
  return r;
}
int main(void) {
  ps_net_init();
  ps_package p = {.permissions = PS_FETCH,
                  .manifest =
                      cJSON_Parse("{\"domains\":[\"api.example.com\"]}")};
  ps_net_result r;
  reset();
  r = request(&p, "POST", "hello");
  assert(r.status == 200 && r.size == 2 && written == 5 && writes == 3);
  assert(clients[0].cfg.tls_dyn_buf_strategy == 0);
  assert(current_ps == WIFI_PS_MIN_MODEM && !ps_changes);
  bounded_free(r.data);
  assert(!blocks);
  reset();
  responses[0] = (struct response){200, 0, true, false, "chunked", NULL, 7};
  r = request(&p, "GET", NULL);
  assert(r.status == 200 && r.size == 7);
  bounded_free(r.data);
  assert(!blocks);
  static char payload[8193];
  memset(payload, 'x', sizeof(payload));
  reset();
  block_limit = 9000;
  responses[0] = (struct response){200, 0, true, true, payload, NULL, 8192};
  r = request(&p, "GET", NULL);
  assert(r.status == 200 && r.size == 8192);
  bounded_free(r.data);
  assert(!blocks);
  reset();
  block_limit = 9000;
  responses[0] = (struct response){200, 0, true, false, payload, NULL, 8193};
  r = request(&p, "GET", NULL);
  assert(r.status == -1 && !r.data);
  assert(!blocks);
  reset();
  responses[0].length = -1;
  r = request(&p, "GET", NULL);
  assert(r.status == -1 && reads == 0);
  assert(!blocks);
  reset();
  responses[0].length = 3;
  r = request(&p, "GET", NULL);
  assert(r.status == -1);
  assert(!blocks);
  reset();
  responses[0] =
      (struct response){302, 8, false, false, "redirect", "/next", 8};
  responses[1] = (struct response){200, 2, false, false, "ok", NULL, 2};
  r = request(&p, "GET", NULL);
  assert(opened == 2 && r.status == 200 && r.size == 2 &&
         !memcmp(r.data, "ok", 2));
  bounded_free(r.data);
  assert(!blocks);
  reset();
  responses[0] =
      (struct response){302, 8, false, false, "redirect", "/next", 8};
  r = request(&p, "POST", NULL);
  assert(opened == 1 && r.status == 302);
  bounded_free(r.data);
  assert(!blocks);
  for (int i = 1; i <= 2; i++) {
    reset();
    fail_allocation = i;
    assert(!ps_net_request(&p, "POST", "https://api.example.com/start", NULL, 0,
                           "hi", 2, 7));
    assert(!blocks);
  }
  reset();
  fail_allocation = 2;
  r = request(&p, "GET", NULL);
  assert(r.status == -1 && !r.data);
  assert(!blocks);
  reset();
  atomic_store(&busy, true);
  assert(!ps_net_request(&p, "GET", "https://api.example.com/start", NULL, 0,
                         NULL, 0, 7));
  atomic_store(&busy, false);
  assert(!blocks);
  reset();
  assert(!ps_net_request(&p, "GET", "https://api.example.com/start",
                         "Host: evil\r\n", 12, NULL, 0, 7));
  assert(!blocks);
  reset();
  assert(ps_net_set_update_source("https://api.example.com/latest.json"));
  assert(
      ps_net_fetch_stream("https://api.example.com/app.bin", 2, sink, NULL, 8));
  run();
  assert(ps_net_poll(&r));
  assert(r.status == 200 && r.size == 2 && sink_bytes == 2 && !blocks);
  assert(current_ps == WIFI_PS_MIN_MODEM && ps_changes == 2);
  reset();
  assert(ps_net_fetch_stream("https://api.example.com/app.bin", 2, sink,
                             (void *)1, 8));
  run();
  assert(ps_net_poll(&r));
  assert(r.status == -1 && !blocks);
  reset();
  assert(current_ps == WIFI_PS_MIN_MODEM && ps_changes == 0);
  reset();
  assert(ps_net_set_store("https://api.example.com/index.json"));
  open_delay = 9000000;
  assert(ps_net_fetch(NULL, "https://api.example.com/start", 12, true));
  run();
  assert(ps_net_poll(&r));
  assert(r.status == 200 && r.size == 2);
  bounded_free(r.data);
  assert(!blocks);
  reset();
  open_delay = 9000000;
  r = request(&p, "GET", NULL);
  assert(r.status == -1 && !blocks);
  reset();
  job *dummy = NULL;
  assert(xQueueSend(jobs, &dummy, 0));
  assert(!ps_net_request(&p, "GET", "https://api.example.com/start", NULL, 0,
                         NULL, 0, 7));
  assert(!blocks);
  assert(xQueueReceive(jobs, &dummy, 0));
  for (unsigned i = 0; i < 500; i++) {
    reset();
    r = request(&p, "POST", "hello");
    assert(r.status == 200);
    bounded_free(r.data);
    assert(!blocks && live_bytes == 0);
  }
  reset();
  assert(!ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                              media_done, NULL, 9));
  p.permissions |= PS_AUDIO_PLAYBACK;
  reset();
  sink_bytes = 0;
  responses[0] = (struct response){200, 0, false, false, "stream", NULL, 6};
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_finishes == 1 && media_status == 200 && sink_bytes == 6 &&
         !blocks && !ps_net_poll(&r));
  assert(clients[0].cfg.tls_dyn_buf_strategy == HTTP_TLS_DYN_BUF_RX_STATIC);
  assert(current_ps == WIFI_PS_MIN_MODEM && ps_changes == 2);
  reset();
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, (void *)1, 9));
  run();
  assert(media_status == -1 && !blocks);
  reset();
  responses[0] = (struct response){
      302, 8, false, false, "redirect", "https://evil.example/radio", 8};
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == -1 && opened == 1 && !blocks);
  reset();
  responses[0] =
      (struct response){302, 8, false, false, "redirect", "/next", 8};
  responses[1] = (struct response){200, 2, false, false, "ok", NULL, 2};
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == 200 && opened == 2 && !blocks);
  reset();
  responses[0].status = 404;
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == -1 && !blocks);
  reset();
  metadata_interval = "16000";
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == -1 && reads == 0 && !blocks);
  reset();
  metadata_interval = "0";
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == 200 && !blocks);
  cJSON_Delete(p.manifest);
  p.manifest =
      cJSON_Parse("{\"domains\":[\"api.example.com\",\"stream.example.com\"]}");
  reset();
  responses[0] = (struct response){
      302, 8, false, false, "redirect", "https://stream.example.com/next", 8};
  responses[1] = (struct response){200, 2, false, false, "ok", NULL, 2};
  assert(ps_net_media_stream(&p, "https://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == 200 && opened == 2 && !blocks);
  reset();
  assert(!ps_net_media_stream(&p, "http://api.example.com/radio", sink,
                              media_done, NULL, 9));
  p.permissions |= PS_AUDIO_HTTP;
  reset();
  assert(ps_net_media_stream(&p, "http://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == 200 && !blocks);
  reset();
  responses[0] = (struct response){
      302, 8, false, false, "redirect", "https://stream.example.com/next", 8};
  responses[1] = (struct response){200, 2, false, false, "ok", NULL, 2};
  assert(ps_net_media_stream(&p, "http://api.example.com/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == 200 && opened == 2 && !blocks);
  reset();
  assert(ps_net_media_stream(&p, "http://api.example.com/radio?token=private",
                             sink, media_done, NULL, 9));
  run();
  assert(media_status == -1 && opened == 0 && !blocks);
  reset();
  assert(ps_net_media_stream(&p, "http://api.example.com:81/radio", sink,
                             media_done, NULL, 9));
  run();
  assert(media_status == -1 && opened == 0 && !blocks);
  reset();
  assert(ps_net_media_stream(&p, "http://192.168.1.2/radio", sink, media_done,
                             NULL, 9));
  run();
  assert(media_status == -1 && opened == 0 && !blocks);
  ws = (void *)1;
  assert(ws_sender_start());
  char sample[] = "audio-frame";
  unsigned sent_before = tx_sent;
  assert(ps_net_ws_send(sample, strlen(sample), true));
  sample[0] = 'X';
  assert(tx_sent == sent_before);
  ws_tx_job *head = *(ws_tx_job **)((queue *)ws_tx_jobs)->data;
  assert(head->data[0] == 'a');
  ws_tx_job *end = NULL;
  assert(xQueueSend(ws_tx_jobs, &end, 0));
  if (!setjmp(tx_suspended))
    ws_sender(NULL);
  assert(tx_sent == sent_before + 1);
  assert(ws_sender_stop());
  assert(!blocks && !ws_tx_jobs && !ws_tx_task);
  ws = NULL;
  ws_events = xQueueCreate(2, sizeof(ws_message));
  ws_message queued = {.opcode = 2, .size = 1};
  assert(xQueueSend(ws_events, &queued, 0));
  assert(xQueueSend(ws_events, &queued, 0));
  esp_websocket_event_data_t event = {.op_code = 2,
                                      .payload_len = 1,
                                      .data_len = 1,
                                      .data_ptr = "x",
                                      .payload_offset = 0,
                                      .fin = 1};
  simulate_ws_consumer = true;
  ws_rx_active = false;
  atomic_store(&ws_rx_bytes, 0);
  atomic_store(&ws_violation, false);
  ws_event(NULL, 0, WEBSOCKET_EVENT_DATA, &event);
  assert(ws_wait == 1000 && !atomic_load(&ws_violation));
  simulate_ws_consumer = false;
  ws_event(NULL, 0, WEBSOCKET_EVENT_DATA, &event);
  assert(atomic_load(&ws_violation));
  vQueueDelete(ws_events);
  ws_events = NULL;
  atomic_store(&ws_violation, false);
  cJSON_Delete(p.manifest);
  vQueueDelete(jobs);
  vQueueDelete(results);
  free(done);
  printf("OS HTTP worker PASS fragmented heap, partial POST, "
         "chunked/trailer/oversize, timeout/truncation, redirects, "
         "OOM/busy/queue/header cleanup, OTA sink, 500 reclaimed requests; "
         "host job=%zu\n",
         sizeof(job));
}

bool ps_ws_transport_open(ps_ws_transport *t, const char *ip, const char *host,
                          bool tls, const char *path, const char *headers) {
  (void)t;
  (void)ip;
  (void)host;
  (void)tls;
  (void)path;
  (void)headers;
  return true;
}
void ps_ws_transport_free(ps_ws_transport *t) { (void)t; }
