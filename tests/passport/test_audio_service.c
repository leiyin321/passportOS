#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "../../os/services/audio/audio.c"
static jmp_buf finished;
static TaskFunction_t task_fn;
static int live, reads, writes, encoder_opens, decoder_opens, fail_init,
    fail_task, last_stack, last_caps;
static int next_after = -1;
static void *owned(size_t n) {
  if (next_after == 0)
    return NULL;
  if (next_after > 0)
    next_after--;
  void *p = calloc(1, n);
  if (p)
    live++;
  return p;
}
static void released(void *p) {
  if (p) {
    live--;
    free(p);
  }
}
typedef struct {
  unsigned cap, size, count;
  char data[];
} queue;
QueueHandle_t xQueueCreate(unsigned cap, unsigned size) {
  queue *q = owned(sizeof(*q) + cap * size);
  if (q) {
    q->cap = cap;
    q->size = size;
  }
  return q;
}
int xQueueSend(QueueHandle_t v, const void *p, int t) {
  (void)t;
  queue *q = v;
  if (q->count == q->cap)
    return 0;
  memcpy(q->data + q->count++ * q->size, p, q->size);
  return 1;
}
int xQueueReceive(QueueHandle_t v, void *p, int t) {
  (void)t;
  queue *q = v;
  if (!q->count)
    return 0;
  memcpy(p, q->data, q->size);
  q->count--;
  memmove(q->data, q->data + q->size, q->count * q->size);
  return 1;
}
void xQueueReset(QueueHandle_t v) { ((queue *)v)->count = 0; }
void vQueueDelete(QueueHandle_t v) { released(v); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return owned(sizeof(int)); }
int xSemaphoreTake(SemaphoreHandle_t p, int t) {
  (void)t;
  int n = *(int *)p;
  *(int *)p = 0;
  return n;
}
int xSemaphoreGive(SemaphoreHandle_t p) {
  *(int *)p = 1;
  return 1;
}
void vSemaphoreDelete(SemaphoreHandle_t p) { released(p); }
int xTaskCreate(TaskFunction_t fn, const char *n, int s, void *a, int pri,
                TaskHandle_t *out) {
  (void)n;
  (void)a;
  (void)pri;
  last_stack = s;
  if (fail_task)
    return 0;
  *out = owned(1);
  task_fn = fn;
  return *out != NULL;
}
int xTaskCreateWithCaps(TaskFunction_t fn, const char *n, int s, void *a,
                        int pri, TaskHandle_t *out, unsigned caps) {
  last_caps = (int)caps;
  return xTaskCreate(fn, n, s, a, pri, out);
}
void vTaskDelete(TaskHandle_t p) { released(p); }
void vTaskDeleteWithCaps(TaskHandle_t p) { vTaskDelete(p); }
void vTaskSuspend(TaskHandle_t p) {
  (void)p;
  longjmp(finished, 1);
}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t p) {
  (void)p;
  return 1024;
}
void taskYIELD(void) { atomic_store(&running, false); }
int64_t esp_timer_get_time(void) {
  static int64_t t;
  return ++t;
}
size_t heap_caps_get_free_size(int c) {
  (void)c;
  return 100000;
}
size_t heap_caps_get_largest_free_block(int c) {
  (void)c;
  return 50000;
}
const char *esp_err_to_name(int c) {
  (void)c;
  return "stub";
}
int bsp_audio_init(void) { return fail_init; }
static unsigned deinit_count, last_volume = 101;
int bsp_audio_deinit(void) {
  deinit_count++;
  return 0;
}
void bsp_audio_set_volume(uint8_t value) {
  assert(value <= 100);
  last_volume = value;
}
int bsp_audio_wake(void) { return 0; }
int bsp_audio_sleep(void) { return 0; }
int bsp_audio_set_format(int r, int b, int c) {
  assert(r == 16000 && b == 16 && c == 1);
  return 0;
}
int bsp_audio_read(void *p, size_t n) {
  memset(p, 0, n);
  reads++;
  return 0;
}
int bsp_audio_write(const void *p, size_t n) {
  assert(p && n);
  writes++;
  return 0;
}
int esp_opus_enc_get_frame_info_by_cfg(void *cfg,
                                       esp_audio_enc_frame_info_t *info) {
  (void)cfg;
  info->in_frame_size = 640;
  info->out_frame_size = 160;
  return 0;
}
int esp_opus_enc_open(const void *c, unsigned n, void **p) {
  (void)c;
  (void)n;
  encoder_opens++;
  *p = owned(sizeof(int));
  if (*p)
    *(int *)*p = ((const esp_opus_enc_config_t *)c)->frame_duration;
  return *p ? 0 : 1;
}
int esp_opus_dec_open(const void *c, unsigned n, void **p) {
  (void)c;
  (void)n;
  decoder_opens++;
  *p = owned(1);
  return *p ? 0 : 1;
}
void esp_opus_enc_close(void *p) { released(p); }
void esp_opus_dec_close(void *p) { released(p); }
int esp_opus_enc_process(void *p, const esp_audio_enc_in_frame_t *i,
                         esp_audio_enc_out_frame_t *o) {
  assert(p && i->len == (*(int *)p == 20 ? 640 : 1920));
  assert(o->len >= 160);
  memset(o->buffer, 0, 60);
  o->buffer[0] = 0xf8;
  o->encoded_bytes = 60;
  return 0;
}
int esp_opus_dec_decode(void *p, const esp_audio_dec_in_raw_t *i,
                        esp_audio_dec_out_frame_t *o, esp_audio_dec_info_t *f) {
  (void)f;
  assert(p && i->len);
  o->decoded_size = 1920;
  return 0;
}
static void run(void) {
  if (!setjmp(finished))
    task_fn(NULL);
}
int main(void) {
  assert(ps_audio_output_claim());
  assert(ps_audio_busy());
  assert(!ps_audio_output_claim());
  assert(!ps_audio_start() && !ps_audio_stream_start() &&
         !ps_audio_stream_prepare());
  ps_audio_output_release();
  assert(!ps_audio_busy());
  assert(ps_audio_start());
  run();
  assert(reads == 1 && encoder_opens == 0);
  assert(ps_audio_stop());
  assert(live == 0);
  for (int i = 0; i < 5; i++) {
    next_after = i;
    assert(!ps_audio_stream_start());
    assert(live == 0);
  }
  next_after = -1;
  fail_task = 1;
  assert(!ps_audio_start());
  assert(live == 0);
  fail_task = 0;
  assert(ps_audio_stream_prepare());
  assert(last_stack == 24576);
  assert(last_caps == (MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT));
  assert(ps_audio_stream_prepared());
  int prepared_live = live;
  assert(ps_audio_stream_prepare());
  assert(live == prepared_live);
  assert(ps_audio_stream_start());
  assert(!ps_audio_stream_prepared());
  assert(ps_audio_stream_active());
  assert(ps_audio_stream_start());
  assert(!ps_audio_start());
  unsigned char packet = 1;
  assert(ps_audio_stream_play_opus(&packet, 1));
  assert(ps_audio_stream_stop());
  run();
  assert(reads == 1 && writes == 1 && decoder_opens == 1);
  assert(ps_audio_stop());
  assert(live == 0);
  assert(ps_audio_stream_prepare());
  assert(ps_audio_stream_prepared());
  assert(ps_audio_stop());
  assert(live == 0);
  assert(ps_audio_stream_start());
  run();
  assert(reads == 2);
  assert(ps_audio_stop());
  assert(live == 0);
  ps_audio_volume_set(false, 0);
  int captures = reads;
  int prior_encoders = encoder_opens;
  assert(ps_audio_stream_half_duplex(true));
  assert(ps_audio_stream_playback_start());
  assert(!atomic_load(&stream_capture));
  assert(ps_audio_stream_play_opus(&packet, 1));
  run();
  assert(reads == captures && encoder_opens == prior_encoders);
  assert(last_volume == 0);
  assert(ps_audio_stop());
  assert(live == 0);
  ps_audio_volume_set(false, 80);
  assert(ps_audio_stream_half_duplex(true));
  assert(ps_audio_stream_start());
  run();
  assert(last_volume == 80);
  assert(encoder_opens == prior_encoders + 1);
  assert(deinit_count == 2);
  assert(ps_audio_stop());
  assert(live == 0);
  fail_init = 1;
  assert(ps_audio_start());
  run();
  assert(!ps_audio_start());
  assert(ps_audio_stop());
  assert(live == 0);
  fail_init = 0;
  assert(ps_audio_stream_prepare());
  assert(ps_audio_start());
  run();
  assert(reads == 4);
  assert(ps_audio_stop());
  assert(live == 0);
  puts("OS audio PASS "
       "RMS/stream/playback/partial-allocation/task-failure/teardown");
}
