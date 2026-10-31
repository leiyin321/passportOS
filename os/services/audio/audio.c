#include "audio.h"
#include "volume.h"
#include "opus_packet.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_opus_dec.h"
#include "esp_opus_enc.h"
#include "esp_timer.h"
#include "features.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define OPUS_PACKET_MAX 512
#define OPUS_RX_MAX 1024
#define OPUS_PCM_SAMPLES 960
#define OPUS_PCM_BYTES (OPUS_PCM_SAMPLES * sizeof(int16_t))
#define AUDIO_TASK_STACK_BYTES 24576
#define FEATURES_TASK_STACK_BYTES 8192
typedef struct { uint16_t size; uint8_t data[OPUS_PACKET_MAX]; } opus_packet;
typedef struct { uint16_t size; uint8_t data[OPUS_RX_MAX]; } opus_input;
typedef struct { int16_t pcm[960]; uint8_t parts[3][PS_OPUS_PART_MAX]; uint16_t sizes[3]; } audio_scratch;
static TaskHandle_t worker;
static SemaphoreHandle_t stopped, start_gate;
static QueueHandle_t encoded, playback;
static atomic_bool running, stream_mode, stream_capture, external_output;
static bool prepared, half_duplex;
static atomic_int level;

static void capture(void *arg) {
  (void)arg;
  /* The worker stack is the largest early allocation. Wait until start() has
   * created the stream queues so queue fragmentation cannot prevent the task
   * from being created in the first place. */
  (void)xSemaphoreTake(start_gate, portMAX_DELAY);
  if (!atomic_load(&running)) {
    xSemaphoreGive(stopped);
    vTaskSuspend(NULL);
    return;
  }
  audio_scratch *scratch=calloc(1,sizeof(*scratch));
  int16_t *pcm=scratch?scratch->pcm:NULL;
  uint8_t opus[OPUS_PACKET_MAX];
  bool streaming = atomic_load(&stream_mode);
  bool exclusive=half_duplex;
  size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  int16_t *decoded = NULL;
  float smoothed = 0;
  int64_t start = esp_timer_get_time(), first = 0, max_read = 0;
  unsigned frames = 0, encoded_frames = 0, decoded_frames = 0, packet_errors = 0;
  int max_level = 0;
  void *enc = NULL, *dec = NULL;
  esp_opus_enc_config_t ecfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
  ecfg.sample_rate = 16000; ecfg.channel = 1; ecfg.bits_per_sample = 16;
  ecfg.bitrate = 24000; ecfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS;
  ecfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP;
  ecfg.complexity = 0; ecfg.enable_dtx = false;
  {
    ecfg.application_mode=ESP_OPUS_ENC_APPLICATION_LOWDELAY;
    ecfg.frame_duration=ESP_OPUS_ENC_FRAME_DURATION_20_MS;
    ecfg.enable_vbr=false;
  }
  esp_opus_dec_cfg_t dcfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
  dcfg.sample_rate = 16000; dcfg.channel = 1;
  dcfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS;
  esp_audio_enc_frame_info_t frame_info={0};
  int layout=esp_opus_enc_get_frame_info_by_cfg(&ecfg,&frame_info);
  ESP_LOGI("passport","AUDIO encoder frame input=%d output_capacity=%d",frame_info.in_frame_size,frame_info.out_frame_size);
  esp_err_t init = scratch && layout==ESP_AUDIO_ERR_OK && frame_info.in_frame_size==640 && frame_info.out_frame_size<=PS_OPUS_PART_MAX ? ESP_OK:ESP_ERR_NO_MEM;
  if (init==ESP_OK && streaming && !exclusive) init=esp_opus_enc_open(&ecfg,sizeof(ecfg),&enc);
  if (init == ESP_AUDIO_ERR_OK && !exclusive) init = bsp_audio_init();
  bool audio_ready = false;
  unsigned applied_volume=101;
  size_t heap_ready = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  bool ok = init == ESP_OK;
  if (!ok) ESP_LOGE("passport", "AUDIO stream init failed rc=%d", (int)init);
  while (ok && atomic_load(&running)) {
    bool capturing = !streaming || atomic_load(&stream_capture);
    /* Manual voice sessions alternate capture and playback. Never retain both
     * Opus states on the no-PSRAM C3. Playback-only startup needs no encoder. */
    if (streaming && exclusive && capturing) {
      if (dec) { esp_opus_dec_close(dec); dec=NULL; }
      free(decoded); decoded=NULL;
      if (!enc) {
        int rc=esp_opus_enc_open(&ecfg,sizeof(ecfg),&enc);
        if (rc!=ESP_AUDIO_ERR_OK) {
          ESP_LOGE("passport","AUDIO encoder unavailable rc=%d free=%u largest=%u",rc,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
          break;
        }
      }
    } else if (exclusive && enc) { esp_opus_enc_close(enc); enc=NULL; }
    opus_input input;
    bool have_playback = streaming && playback &&
                         xQueueReceive(playback, &input, capturing ? 0 : pdMS_TO_TICKS(30)) == pdTRUE;
    if (!capturing && !have_playback) {
      if (audio_ready) { (void)bsp_audio_sleep(); audio_ready = false; }
      continue;
    }
    if (streaming && exclusive && capturing) have_playback=false;
    if (!audio_ready) {
      esp_err_t wake = exclusive ? bsp_audio_init() : ESP_OK;
      if (wake == ESP_OK) wake = bsp_audio_wake();
      if (wake == ESP_OK) wake = bsp_audio_set_format(16000, 16, 1);
      applied_volume=101;
      if (wake != ESP_OK) { ESP_LOGE("passport", "AUDIO wake failed rc=%s", esp_err_to_name(wake)); break; }
      audio_ready = true;
    }
    if (streaming && applied_volume!=ps_audio_volume(false)) {
      applied_volume=ps_audio_volume(false);
      bsp_audio_set_volume(applied_volume);
      ESP_LOGI("passport","AUDIO media volume=%u",applied_volume);
    }
    if (capturing) {
      int64_t read_start = esp_timer_get_time();
      if (bsp_audio_read(pcm, OPUS_PCM_BYTES) != ESP_OK) break;
      int64_t elapsed = esp_timer_get_time() - read_start;
      if (elapsed > max_read) max_read = elapsed;
      if (!first) first = esp_timer_get_time() - start;
      frames++;
      float rms = ps_audio_rms_percent(pcm, 960);
      smoothed = .65f * smoothed + .35f * rms;
      atomic_store(&level, (int)(smoothed + .5f));
      if ((int)(smoothed + .5f) > max_level) max_level = (int)(smoothed + .5f);
      if (streaming && enc && atomic_load(&stream_capture)) {
        size_t packet_size=0;
        bool encoded_ok=true;
        for (unsigned i=0;i<3;i++) {
          esp_audio_enc_in_frame_t in={.buffer=(uint8_t *)(pcm+i*320),.len=640};
          esp_audio_enc_out_frame_t out={.buffer=scratch->parts[i],.len=PS_OPUS_PART_MAX};
          int encode_rc=esp_opus_enc_process(enc,&in,&out);
          if (encode_rc!=ESP_AUDIO_ERR_OK || !out.encoded_bytes || out.encoded_bytes>PS_OPUS_PART_MAX) {
            if (packet_errors++<3) ESP_LOGW("passport","AUDIO packet encode rc=%d bytes=%u capacity=%u",encode_rc,(unsigned)out.encoded_bytes,(unsigned)out.len);
            encoded_ok=false;break;
          }
          scratch->sizes[i]=out.encoded_bytes;
        }
        if (encoded_ok) {
          packet_size=ps_opus_join_20ms(scratch->parts,scratch->sizes,opus,sizeof(opus));
          if (!packet_size && packet_errors++<3) ESP_LOGW("passport","AUDIO packet format sizes=%u/%u/%u toc=%02x/%02x/%02x",scratch->sizes[0],scratch->sizes[1],scratch->sizes[2],scratch->parts[0][0],scratch->parts[1][0],scratch->parts[2][0]);
        }
        if (packet_size && packet_size<=sizeof(opus)) {
          opus_packet p = {.size = (uint16_t)packet_size};
          memcpy(p.data, opus, p.size);
          if (xQueueSend(encoded, &p, 0) != pdTRUE) {
            opus_packet discarded;
            (void)xQueueReceive(encoded, &discarded, 0);
            (void)xQueueSend(encoded, &p, 0);
          }
          encoded_frames++;
        }
      }
    }
    if (have_playback) {
      if (!decoded) decoded=malloc(OPUS_PCM_BYTES);
      if (!decoded) break;
      if (!dec && esp_opus_dec_open(&dcfg, sizeof(dcfg), &dec) != ESP_AUDIO_ERR_OK)
        continue;
      esp_audio_dec_in_raw_t raw = {.buffer = input.data, .len = input.size};
      esp_audio_dec_out_frame_t out = {.buffer = (uint8_t *)decoded, .len = OPUS_PCM_BYTES};
      esp_audio_dec_info_t info = {0};
      if (esp_opus_dec_decode(dec, &raw, &out, &info) == ESP_AUDIO_ERR_OK &&
          out.decoded_size && out.decoded_size <= OPUS_PCM_BYTES &&
          bsp_audio_write(decoded, out.decoded_size) == ESP_OK)
        decoded_frames++;
    }
    while (streaming && (!exclusive || !capturing) && playback && xQueueReceive(playback, &input, 0) == pdTRUE) {
      if (!decoded) decoded=malloc(OPUS_PCM_BYTES);
      if (!decoded) break;
      if (!dec && esp_opus_dec_open(&dcfg, sizeof(dcfg), &dec) != ESP_AUDIO_ERR_OK)
        continue;
      esp_audio_dec_in_raw_t raw = {.buffer = input.data, .len = input.size};
      esp_audio_dec_out_frame_t out = {.buffer = (uint8_t *)decoded, .len = OPUS_PCM_BYTES};
      esp_audio_dec_info_t info = {0};
      if (esp_opus_dec_decode(dec, &raw, &out, &info) == ESP_AUDIO_ERR_OK &&
          out.decoded_size && out.decoded_size <= OPUS_PCM_BYTES &&
          bsp_audio_write(decoded, out.decoded_size) == ESP_OK)
        decoded_frames++;
    }
    taskYIELD();
  }
  if (enc) esp_opus_enc_close(enc);
  if (dec) esp_opus_dec_close(dec);
  free(decoded);
  free(scratch);
  atomic_store(&level, 0);
  atomic_store(&running, false);
  esp_err_t rc = exclusive ? bsp_audio_deinit() : bsp_audio_sleep();
  size_t heap_recovered = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  ESP_LOGI("passport",
           "AUDIO stopped sleep=%d frames=%u opus_tx=%u opus_rx=%u elapsed_us=%lld first_us=%lld max_read_us=%lld max_rms=%d stack=%u heap_before=%u heap_ready=%u heap_after_codec_close=%u largest8=%u",
           rc, frames, encoded_frames, decoded_frames,
           (long long)(esp_timer_get_time() - start), (long long)first,
           (long long)max_read, max_level,
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)heap_before,
           (unsigned)heap_ready, (unsigned)heap_recovered,
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  xSemaphoreGive(stopped);
  vTaskSuspend(NULL);
}

static bool create_worker(unsigned stack_bytes) {
  if (!stopped) stopped = xSemaphoreCreateBinary();
  if (!start_gate) start_gate = xSemaphoreCreateBinary();
  if (!stopped || !start_gate) {
    ESP_LOGE("passport", "AUDIO start failed stage=semaphore free=%u largest8=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (start_gate) { vSemaphoreDelete(start_gate); start_gate = NULL; }
    if (stopped) { vSemaphoreDelete(stopped); stopped = NULL; }
    return false;
  }
  atomic_store(&running, false);
  if (xTaskCreateWithCaps(capture, "passport_audio", stack_bytes,
                          NULL, 5, &worker,
                          MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT) != pdPASS) {
    worker = NULL;
    ESP_LOGE("passport", "AUDIO start failed stage=task-stack-retention bytes=%u free=%u largest8=%u retention=%u",
             stack_bytes,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT));
    vSemaphoreDelete(start_gate); start_gate = NULL;
    vSemaphoreDelete(stopped); stopped = NULL;
    return false;
  }
  return true;
}

bool ps_audio_stream_prepare(void) {
  if(atomic_load(&external_output))return false;
  if (worker) return atomic_load(&stream_mode);
  if (!create_worker(AUDIO_TASK_STACK_BYTES)) return false;
  atomic_store(&stream_mode, true);
  atomic_store(&stream_capture, false);
  prepared = true;
  ESP_LOGI("passport", "AUDIO stream prepared stack=%u microphone=idle free=%u largest8=%u retention=%u",
           AUDIO_TASK_STACK_BYTES,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT));
  return true;
}

bool ps_audio_stream_prepared(void) { return worker && prepared; }
bool ps_audio_stream_active(void) {
  return worker && !prepared && atomic_load(&stream_mode) && atomic_load(&running);
}

static void discard_blocked_worker(void) {
  if (worker) { vTaskDeleteWithCaps(worker); worker = NULL; }
  if (encoded) { vQueueDelete(encoded); encoded = NULL; }
  if (playback) { vQueueDelete(playback); playback = NULL; }
  if (start_gate) { vSemaphoreDelete(start_gate); start_gate = NULL; }
  if (stopped) { vSemaphoreDelete(stopped); stopped = NULL; }
  atomic_store(&running, false);
  atomic_store(&stream_mode, false);
  atomic_store(&stream_capture, false);
  prepared = false;
}

static bool start(bool streaming, bool capturing) {
  if(atomic_load(&external_output))return false;
  if (worker && !prepared) {
    if (streaming != atomic_load(&stream_mode)) return false;
    if (streaming && atomic_load(&running)) {
      bool was=atomic_exchange(&stream_capture,capturing);
      if (capturing && !was) {
        if (encoded) xQueueReset(encoded);
        if (playback) xQueueReset(playback);
      }
    }
    return atomic_load(&running);
  }
  if (!worker) {
    if (streaming) {
      if (!ps_audio_stream_prepare()) return false;
    } else {
      if (!create_worker(FEATURES_TASK_STACK_BYTES)) return false;
      atomic_store(&stream_mode, false);
      atomic_store(&stream_capture, false);
    }
  }
  if (streaming && !encoded) encoded = xQueueCreate(2, sizeof(opus_packet));
  if (streaming && !playback) playback = xQueueCreate(2, sizeof(opus_input));
  if (streaming && (!encoded || !playback)) {
    ESP_LOGE("passport", "AUDIO start failed stage=stream-queue encoded=%d playback=%d free=%u largest8=%u",
             encoded != NULL, playback != NULL,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    discard_blocked_worker();
    return false;
  }
  if (encoded) xQueueReset(encoded);
  if (playback) xQueueReset(playback);
  atomic_store(&stream_mode, streaming);
  atomic_store(&stream_capture, streaming && capturing);
  atomic_store(&running, true);
  prepared = false;
  xSemaphoreGive(start_gate);
  ESP_LOGI("passport", "AUDIO requested stream=%d", streaming);
  return true;
}
bool ps_audio_start(void) { return start(false,true); }
bool ps_audio_stream_start(void) { return start(true,true); }
bool ps_audio_stream_playback_start(void) { return start(true,false); }
bool ps_audio_stop(void) {
  if (!worker) { half_duplex=false; return true; }
  if (prepared) {
    discard_blocked_worker();
    half_duplex=false;
    ESP_LOGI("passport", "AUDIO prepared worker reclaimed free_heap=%u largest8=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return true;
  }
  atomic_store(&running, false);
  if (xSemaphoreTake(stopped, pdMS_TO_TICKS(2500)) != pdTRUE) return false;
  vTaskDeleteWithCaps(worker); worker = NULL;
  if (encoded) { vQueueDelete(encoded); encoded = NULL; }
  if (playback) { vQueueDelete(playback); playback = NULL; }
  if (start_gate) { vSemaphoreDelete(start_gate); start_gate = NULL; }
  vSemaphoreDelete(stopped); stopped = NULL;
  atomic_store(&stream_mode, false);
  atomic_store(&stream_capture, false);
  half_duplex=false;
  ESP_LOGI("passport", "AUDIO reclaimed free_heap=%u largest8=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  return true;
}
bool ps_audio_stream_stop(void) {
  if (!worker || !atomic_load(&stream_mode)) return true;
  if (prepared) return true;
  atomic_store(&stream_capture, false);
  return true;
}
bool ps_audio_stream_poll(uint8_t *data, size_t cap, size_t *size) {
  if (!data || !size || !encoded) return false;
  opus_packet p;
  if (xQueueReceive(encoded, &p, 0) != pdTRUE || cap < p.size) return false;
  memcpy(data, p.data, p.size); *size = p.size;
  return true;
}
bool ps_audio_stream_play_opus_wait(const uint8_t *data, size_t size, unsigned wait_ms) {
  if (!data || !size || size > OPUS_RX_MAX || !playback ||
      !atomic_load(&running) || !atomic_load(&stream_mode)) return false;
  opus_input p = {.size = (uint16_t)size};
  memcpy(p.data, data, size);
  return xQueueSend(playback, &p, pdMS_TO_TICKS(wait_ms>80?80:wait_ms)) == pdTRUE;
}
int ps_audio_level(void) { return atomic_load(&level); }

bool ps_audio_stream_half_duplex(bool enabled) {
  if (worker && half_duplex!=enabled) return false;
  half_duplex=enabled; return true;
}

bool ps_audio_stream_play_opus(const uint8_t *data,size_t size) { return ps_audio_stream_play_opus_wait(data,size,0); }

bool ps_audio_busy(void) { return worker!=NULL || atomic_load(&external_output); }
bool ps_audio_output_claim(void) {
  bool expected=false;
  return !worker && atomic_compare_exchange_strong(&external_output,&expected,true);
}
void ps_audio_output_release(void) {atomic_store(&external_output,false);}
