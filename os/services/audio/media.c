#include "media.h"
#include "mp3_frames.h"
#include "audio.h"
#include "volume.h"
#include "network.h"
#include "bsp_audio.h"
#include "passport_mp3.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define MEDIA_BLOCK 512
#define MEDIA_QUEUE 16
#define MEDIA_HTTP_QUEUE 64
#define MEDIA_HTTP_START 32
#define MEDIA_HEAP_RESERVE (14u*1024u)
#define MEDIA_STACK 4096
typedef struct {unsigned size;uint8_t data[MEDIA_BLOCK];} media_block;
typedef struct {
  ps_mp3_frames frames;
  ps_mp3_decoder *decoder;
  int16_t pcm[2304];
  QueueHandle_t queue,free_blocks;
  media_block *blocks[MEDIA_HTTP_QUEUE];
  media_block *pending; /* Network task only: pack short HTTP reads. */
  unsigned depth,limit;
  SemaphoreHandle_t gate,done;
  TaskHandle_t worker;
  atomic_bool input_done,failed;
  unsigned rate,volume,decoded,errors,starves;
  size_t heap_low,codec_bytes;
  int64_t decode_us,max_decode_us,max_gap_us,last_write_us,first_write_us;
  uint64_t output_us;
  bool audio;
} media_session;
static atomic_bool active,stopping;
static atomic_uint state;
/* Decoder task owns PCM, codec, parser and BSP; network task only copies
 * compressed chunks. Neither worker retains application code or buffers. */
static bool frame(const uint8_t *p,size_t n,void *context) {
  media_session *s=context;
  if(atomic_load(&stopping))return false;
  int hz=0,channels=0;
  int64_t begin=esp_timer_get_time();
  int samples=ps_mp3_decoder_decode(s->decoder,p,n,s->pcm,&hz,&channels);
  int64_t elapsed=esp_timer_get_time()-begin;
  s->decode_us+=elapsed;if(elapsed>s->max_decode_us)s->max_decode_us=elapsed;
  if(!samples)return ++s->errors<=8;
  s->errors=0;
  if(samples<0 || samples>1152 || (channels!=1 && channels!=2) || hz<8000 || hz>48000)return false;
  if(!s->audio) {
    if(bsp_audio_init_playback()!=ESP_OK || bsp_audio_wake()!=ESP_OK)return false;
    s->audio=true;
  }
  if(s->rate!=(unsigned)hz){
    if(bsp_audio_set_format((unsigned)hz,16,1)!=ESP_OK)return false;
    s->rate=(unsigned)hz;
  }
  if(!s->first_write_us && s->limit>MEDIA_QUEUE && !atomic_load(&s->input_done)) {
    while(s->depth<s->limit && heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DEFAULT)>MEDIA_HEAP_RESERVE+sizeof(media_block)) {
      media_block *extra=malloc(sizeof(*extra));if(!extra)break;
      s->blocks[s->depth++]=extra;(void)xQueueSend(s->free_blocks,&extra,0);
    }
    /* One compressed block is borrowed by this parser invocation. */
    while(uxQueueMessagesWaiting(s->queue)+1<s->depth && !atomic_load(&s->input_done) && !atomic_load(&stopping))
      vTaskDelay(pdMS_TO_TICKS(10));
  }
  unsigned count=(unsigned)samples;
  if(channels==2)for(unsigned i=0;i<count;i++)s->pcm[i]=((int32_t)s->pcm[2*i]+s->pcm[2*i+1])/2;
  unsigned volume=ps_audio_volume(false);
  if(s->volume!=volume){bsp_audio_set_volume(volume);s->volume=volume;}
  int64_t now=esp_timer_get_time();
  if(s->last_write_us && now-s->last_write_us>s->max_gap_us)s->max_gap_us=now-s->last_write_us;
  if(!s->first_write_us)s->first_write_us=now;
  /* Bound the codec/I2S channel-conversion scratch, not one large allocation
   * per complete MP3 frame. 240 samples matches one board DMA descriptor. */
  for(unsigned offset=0;offset<count;) {
    unsigned chunk=count-offset;if(chunk>240)chunk=240;
    if(atomic_load(&stopping) || bsp_audio_write(s->pcm+offset,chunk*2)!=ESP_OK)return false;
    offset+=chunk;
  }
  s->last_write_us=esp_timer_get_time();
  s->output_us+=(uint64_t)count*1000000u/(unsigned)hz;s->decoded++;
  size_t heap=heap_caps_get_free_size(MALLOC_CAP_8BIT);if(heap<s->heap_low)s->heap_low=heap;
  unsigned expected=atomic_load(&state);
  if(expected==1 || expected==2)(void)atomic_compare_exchange_strong(&state,&expected,2);
  return !atomic_load(&stopping);
}
static void playback(void *context) {
  media_session *s=context;
  xSemaphoreTake(s->gate,portMAX_DELAY);
  bool primed=false;
  while(!atomic_load(&stopping) && !atomic_load(&s->failed)) {
    bool ended=atomic_load(&s->input_done);
    if(!primed && !ended && uxQueueMessagesWaiting(s->queue)<s->depth){vTaskDelay(pdMS_TO_TICKS(10));continue;}
    primed=true;
    media_block *block=NULL;
    if(xQueueReceive(s->queue,&block,pdMS_TO_TICKS(30))!=pdTRUE) {
      if(atomic_load(&s->input_done) || atomic_load(&stopping))break;
      s->starves++;primed=false;
      unsigned expected=2;(void)atomic_compare_exchange_strong(&state,&expected,1);
      continue;
    }
    bool ok=ps_mp3_feed(&s->frames,block->data,block->size,frame,s);
    (void)xQueueSend(s->free_blocks,&block,0);
    if(!ok) {
      if(!atomic_load(&stopping))atomic_store(&s->failed,true);
      break;
    }
  }
  bsp_audio_deinit();
  ESP_LOGI("passport_media","playback frames=%u codec_open_bytes=%u blocks=%u decode_avg_us=%u decode_max_us=%u output_ms=%u elapsed_ms=%u starves=%u max_gap_us=%u heap_min_sampled=%u stack_free=%u",
           s->decoded,(unsigned)s->codec_bytes,s->depth,s->decoded?(unsigned)(s->decode_us/s->decoded):0,
           (unsigned)s->max_decode_us,(unsigned)(s->output_us/1000),s->first_write_us?(unsigned)((esp_timer_get_time()-s->first_write_us)/1000):0,
           s->starves,(unsigned)s->max_gap_us,(unsigned)s->heap_low,(unsigned)uxTaskGetStackHighWaterMark(NULL));
  xSemaphoreGive(s->done);vTaskSuspend(NULL);
}
static bool write_stream(const void *data,size_t bytes,void *context) {
  media_session *s=context;
  if(!data || !bytes || bytes>MEDIA_BLOCK)return false;
  const uint8_t *input=data;
  while(bytes) {
    if(atomic_load(&stopping) || atomic_load(&s->failed))return false;
    if(!s->pending) {
      for(unsigned waits=0;waits<75 && !s->pending;waits++) {
        if(atomic_load(&stopping) || atomic_load(&s->failed))return false;
        (void)xQueueReceive(s->free_blocks,&s->pending,pdMS_TO_TICKS(40));
      }
      if(!s->pending)return false;
      s->pending->size=0;
    }
    unsigned n=MEDIA_BLOCK-s->pending->size;if(n>bytes)n=(unsigned)bytes;
    memcpy(s->pending->data+s->pending->size,input,n);
    s->pending->size+=n;input+=n;bytes-=n;
    if(s->pending->size==MEDIA_BLOCK) {
      if(xQueueSend(s->queue,&s->pending,0)!=pdTRUE)return false;
      s->pending=NULL;
    }
  }
  return true;
}
static void release(media_session *s) {
  if(s->worker)vTaskDelete(s->worker);
  if(s->queue)vQueueDelete(s->queue);
  if(s->free_blocks)vQueueDelete(s->free_blocks);
  for(unsigned i=0;i<MEDIA_HTTP_QUEUE;i++)free(s->blocks[i]);
  if(s->gate)vSemaphoreDelete(s->gate);
  if(s->done)vSemaphoreDelete(s->done);
  if(s->decoder)ps_mp3_decoder_close(s->decoder);
  free(s);
}
static void finish(int status,void *context) {
  media_session *s=context;
  /* A short final block still belongs to the stream; publish it before EOF.
   * A pending block was taken from the pool, so its queue slot is available. */
  if(s->pending && !atomic_load(&stopping) && !atomic_load(&s->failed)) {
    (void)xQueueSend(s->queue,&s->pending,0);s->pending=NULL;
  }
  atomic_store(&s->input_done,true);
  if(xSemaphoreTake(s->done,pdMS_TO_TICKS(2500))!=pdTRUE) {
    ESP_LOGW("passport_media","waiting for playback cleanup; output remains owned");
    xSemaphoreTake(s->done,portMAX_DELAY);
  }
  unsigned end=atomic_load(&stopping)?0:status==200 && !atomic_load(&s->failed) && s->decoded?5:4;
  release(s);
  ps_audio_output_release();
  atomic_store(&state,end);atomic_store(&active,false);
  ESP_LOGI("passport_media","closed state=%u heap_after=%u network_stack_free=%u",end,
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),(unsigned)uxTaskGetStackHighWaterMark(NULL));
}
bool ps_media_play(const ps_package *app,const char *url,uint32_t generation) {
  if(!app || !url || (!strncmp(url,"http://",7) && !(app->permissions&PS_AUDIO_HTTP)))return false;
  if(atomic_load(&active) || !ps_audio_output_claim())return false;
  media_session *s=calloc(1,sizeof(*s));
  if(!s){ps_audio_output_release();return false;}
  s->volume=101;
  size_t before=heap_caps_get_free_size(MALLOC_CAP_8BIT);
  s->decoder=ps_mp3_decoder_open();s->codec_bytes=before-heap_caps_get_free_size(MALLOC_CAP_8BIT);
  s->limit=!strncmp(url,"http://",7)?MEDIA_HTTP_QUEUE:MEDIA_QUEUE;
  s->depth=s->limit>MEDIA_QUEUE?MEDIA_HTTP_START:MEDIA_QUEUE;
  s->queue=xQueueCreate(s->limit,sizeof(media_block *));
  s->free_blocks=xQueueCreate(s->limit,sizeof(media_block *));
  unsigned allocated=0;
  if(s->queue && s->free_blocks)for(;allocated<s->depth;allocated++) {
    s->blocks[allocated]=malloc(sizeof(media_block));if(!s->blocks[allocated])break;
    (void)xQueueSend(s->free_blocks,&s->blocks[allocated],0);
  }
  s->gate=xSemaphoreCreateBinary();s->done=xSemaphoreCreateBinary();
  if(!s->decoder || !s->queue || !s->free_blocks || allocated!=s->depth || !s->gate || !s->done ||
     xTaskCreate(playback,"passport_media",MEDIA_STACK,s,5,&s->worker)!=pdPASS) {
    release(s);ps_audio_output_release();atomic_store(&state,4);return false;
  }
  s->heap_low=heap_caps_get_free_size(MALLOC_CAP_8BIT);
  atomic_store(&stopping,false);atomic_store(&state,1);atomic_store(&active,true);
  if(!ps_net_media_stream(app,url,write_stream,finish,s,generation)) {
    release(s);ps_audio_output_release();atomic_store(&active,false);atomic_store(&state,4);return false;
  }
  xSemaphoreGive(s->gate);
  return true;
}
void ps_media_stop(void) {
  if(atomic_load(&active)){atomic_store(&stopping,true);atomic_store(&state,3);ps_net_cancel_pending();}
  else atomic_store(&state,0);
}
unsigned ps_media_state(void){return atomic_load(&state);}
bool ps_media_active(void){return atomic_load(&active);}
bool ps_media_close(void) {
  if(!atomic_load(&active))return true;
  ps_media_stop();ps_net_cancel();return !atomic_load(&active);
}
