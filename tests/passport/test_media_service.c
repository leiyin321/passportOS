#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../os/services/audio/media.c"
#include "../fixtures/mp3_sine.h"
static ps_net_stream_write_fn writer;
static ps_net_stream_done_fn completion;
static void *owner;
static bool queued = true, audio_busy;
static unsigned formats, pcm_writes, deinits;
static atomic_bool reject_pcm, stop_in_pcm;
static unsigned free_heap = 80000;

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t changed;
  unsigned cap, count, head;
  size_t size;
  char data[];
} fake_queue;
typedef struct {
  pthread_t thread;
  TaskFunction_t fn;
  void *arg;
} fake_task;
static void unlock(void *p) { pthread_mutex_unlock(p); }
static int cond_pause(pthread_cond_t *c, pthread_mutex_t *m, int ticks) {
  if (!ticks)
    return 1;
  if (ticks == portMAX_DELAY)
    return pthread_cond_wait(c, m);
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  t.tv_sec += ticks / 1000;
  t.tv_nsec += (ticks % 1000) * 1000000L;
  if (t.tv_nsec >= 1000000000L) {
    t.tv_sec++;
    t.tv_nsec -= 1000000000L;
  }
  return pthread_cond_timedwait(c, m, &t);
}
QueueHandle_t xQueueCreate(unsigned cap, unsigned size) {
  fake_queue *q = calloc(1, sizeof(*q) + cap * size);
  assert(q);
  pthread_mutex_init(&q->lock, NULL);
  pthread_cond_init(&q->changed, NULL);
  q->cap = cap;
  q->size = size;
  return q;
}
int xQueueSend(QueueHandle_t ptr, const void *item, int ticks) {
  fake_queue *q = ptr;
  int result = 0;
  pthread_mutex_lock(&q->lock);
  pthread_cleanup_push(unlock, &q->lock);
  while (q->count == q->cap)
    if (cond_pause(&q->changed, &q->lock, ticks))
      goto done;
  memcpy(q->data + ((q->head + q->count) % q->cap) * q->size, item, q->size);
  q->count++;
  pthread_cond_broadcast(&q->changed);
  result = 1;
done:
  pthread_cleanup_pop(1);
  return result;
}
int xQueueReceive(QueueHandle_t ptr, void *item, int ticks) {
  fake_queue *q = ptr;
  int result = 0;
  pthread_mutex_lock(&q->lock);
  pthread_cleanup_push(unlock, &q->lock);
  while (!q->count)
    if (cond_pause(&q->changed, &q->lock, ticks))
      goto done;
  memcpy(item, q->data + q->head * q->size, q->size);
  q->head = (q->head + 1) % q->cap;
  q->count--;
  pthread_cond_broadcast(&q->changed);
  result = 1;
done:
  pthread_cleanup_pop(1);
  return result;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t ptr) {
  fake_queue *q = ptr;
  pthread_mutex_lock(&q->lock);
  unsigned n = q->count;
  pthread_mutex_unlock(&q->lock);
  return n;
}
void vQueueDelete(QueueHandle_t ptr) {
  fake_queue *q = ptr;
  pthread_mutex_destroy(&q->lock);
  pthread_cond_destroy(&q->changed);
  free(q);
}
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return xQueueCreate(1, 1); }
int xSemaphoreTake(SemaphoreHandle_t s, int ticks) {
  char c;
  return xQueueReceive(s, &c, ticks);
}
int xSemaphoreGive(SemaphoreHandle_t s) {
  char c = 1;
  return xQueueSend(s, &c, 0);
}
void vSemaphoreDelete(SemaphoreHandle_t s) { vQueueDelete(s); }
void vTaskDelay(int ticks) {
  struct timespec t = {ticks / 1000, (ticks % 1000) * 1000000L};
  nanosleep(&t, NULL);
}
static void *run_task(void *ptr) {
  fake_task *t = ptr;
  t->fn(t->arg);
  return NULL;
}
int xTaskCreate(TaskFunction_t fn, const char *n, int stack, void *arg,
                int priority, TaskHandle_t *out) {
  (void)n;
  assert(stack == 4096 && priority == 5);
  fake_task *t = calloc(1, sizeof(*t));
  assert(t);
  t->fn = fn;
  t->arg = arg;
  *out = t;
  assert(!pthread_create(&t->thread, NULL, run_task, t));
  return pdPASS;
}
void vTaskDelete(TaskHandle_t ptr) {
  fake_task *t = ptr;
  pthread_cancel(t->thread);
  pthread_join(t->thread, NULL);
  free(t);
}
void vTaskSuspend(TaskHandle_t ptr) {
  assert(!ptr);
  pthread_exit(NULL);
}
bool ps_audio_busy(void) { return audio_busy; }
bool ps_audio_output_claim(void) {
  if (audio_busy)
    return false;
  audio_busy = true;
  return true;
}
void ps_audio_output_release(void) { audio_busy = false; }
bool ps_net_media_stream(const ps_package *p, const char *u,
                         ps_net_stream_write_fn w, ps_net_stream_done_fn done,
                         void *ctx, uint32_t gen) {
  assert(p && u && gen == 7);
  if (!queued)
    return false;
  writer = w;
  completion = done;
  owner = ctx;
  return true;
}
void ps_net_cancel_pending(void) {}
void ps_net_cancel(void) {
  if (owner) {
    void *p = owner;
    owner = NULL;
    completion(-1, p);
  }
}
size_t heap_caps_get_free_size(int caps) {
  (void)caps;
  return free_heap;
}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t) {
  (void)t;
  return 2048;
}
int64_t esp_timer_get_time(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
int bsp_audio_init(void) { return 0; }
int bsp_audio_init_playback(void) { return 0; }
int bsp_audio_wake(void) { return 0; }
int bsp_audio_deinit(void) {
  deinits++;
  return 0;
}
void bsp_audio_set_volume(uint8_t v) { assert(v <= 100); }
int bsp_audio_set_format(int hz, int bits, int ch) {
  assert(hz == 16000 && bits == 16 && ch == 1);
  formats++;
  return 0;
}
int bsp_audio_write(const void *p, size_t n) {
  assert(p && n > 0 && n <= 480);
  pcm_writes++;
  if (atomic_load(&stop_in_pcm))
    ps_media_stop();
  return atomic_load(&reject_pcm) ? -1 : 0;
}
static void feed(void) {
  for (unsigned i = 0; i < sizeof(mp3_sine);) {
    unsigned n = sizeof(mp3_sine) - i;
    if (n > 512)
      n = 512;
    assert(writer(mp3_sine + i, n, owner));
    i += n;
  }
}
static void preroll(void) {
  for (unsigned i = 0; i < 8; i++)
    feed();
}
static void wait_state(unsigned expected) {
  for (unsigned i = 0; i < 500 && ps_media_state() != expected; i++)
    vTaskDelay(1);
  assert(ps_media_state() == expected);
}
static void eof(int status) {
  void *p = owner;
  owner = NULL;
  completion(status, p);
}
int main(void) {
  ps_package p = {.permissions = PS_FETCH | PS_AUDIO_PLAYBACK};
  audio_busy = true;
  assert(!ps_media_play(&p, "https://radio.example/live", 7));
  audio_busy = false;
  queued = false;
  assert(!ps_media_play(&p, "https://radio.example/live", 7) &&
         !ps_media_active());
  queued = true;
  for (unsigned i = 0; i < 100; i++) {
    assert(ps_media_play(&p, "https://radio.example/live", 7));
    assert(ps_media_active() && ps_media_state() == 1);
    assert(!ps_media_play(&p, "https://radio.example/other", 7));
    feed();
    if (!i) {
      vTaskDelay(20);
      assert(ps_media_state() == 1 && !pcm_writes);
    }
    preroll();
    wait_state(2);
    ps_media_stop();
    assert(ps_media_state() == 3);
    assert(!writer(mp3_sine, 1, owner));
    assert(ps_media_close() && !ps_media_active() && ps_media_state() == 0 &&
           !audio_busy);
  }
  assert(formats == 100 && pcm_writes >= 100 && deinits == 100);
  assert(ps_media_play(&p, "https://radio.example/live", 7));
  feed();
  eof(200);
  assert(ps_media_state() == 5 && !ps_media_active());
  assert(ps_media_play(&p, "https://radio.example/live", 7));
  atomic_store(&stop_in_pcm, true);
  feed();
  eof(-1);
  assert(ps_media_state() == 0 && !ps_media_active());
  atomic_store(&stop_in_pcm, false);
  assert(ps_media_play(&p, "https://radio.example/live", 7));
  atomic_store(&reject_pcm, true);
  feed();
  eof(-1);
  assert(ps_media_state() == 4 && !ps_media_active());
  atomic_store(&reject_pcm, false);
  assert(ps_media_play(&p, "https://radio.example/live", 7));
  preroll();
  wait_state(2);
  vTaskDelay(80);
  wait_state(1);
  assert(((media_session *)owner)->starves > 0);
  preroll();
  wait_state(2);
  ps_media_stop();
  assert(ps_media_close());
  /* Tiny/chunked transport reads must consume bytes, not whole pool blocks. */
  assert(ps_media_play(&p, "https://radio.example/live", 7));
  for (unsigned i = 0; i < sizeof(mp3_sine); i++)
    assert(writer(mp3_sine + i, 1, owner));
  media_session *packed = owner;
  assert(uxQueueMessagesWaiting(packed->queue) == sizeof(mp3_sine) / 512);
  assert(packed->pending && packed->pending->size == sizeof(mp3_sine) % 512);
  eof(200);
  assert(ps_media_state() == 5 && !audio_busy);
  p.permissions |= PS_AUDIO_HTTP;
  assert(ps_media_play(&p, "http://radio.example/live", 7));
  assert(((media_session *)owner)->depth == 32);
  feed();
  assert(ps_media_state() == 1);
  eof(200);
  assert(ps_media_state() == 5 && !audio_busy);
  assert(ps_media_play(&p, "http://radio.example/live", 7));
  for (unsigned i = 0; i < 32; i++)
    feed();
  wait_state(2);
  assert(((media_session *)owner)->depth == 64);
  ps_media_stop();
  assert(ps_media_close());
  free_heap = 14000;
  assert(ps_media_play(&p, "http://radio.example/live", 7));
  for (unsigned i = 0; i < 16; i++)
    feed();
  wait_state(2);
  assert(((media_session *)owner)->depth == 32);
  ps_media_stop();
  assert(ps_media_close());
  free_heap = 80000;
  printf("Media service PASS 100 threaded play/stop/reclaim cycles, preroll, "
         "rebuffer, short EOF, overlap/capture rejection, PCM failure and stop "
         "race; host session=%zu codec=%zu bytes\n",
         sizeof(media_session), ps_mp3_decoder_bytes());
}
