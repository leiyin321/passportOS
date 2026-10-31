#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
#define ESP_AUDIO_ERR_OK 0
#define MALLOC_CAP_8BIT 1
#define MALLOC_CAP_INTERNAL 0x800
#define MALLOC_CAP_DEFAULT 0x1000
#define MALLOC_CAP_RETENTION 2
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY (-1)
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
typedef void (*TaskFunction_t)(void *);
#define ESP_LOGI(tag, ...) ((void)(tag), (void)printf(__VA_ARGS__), (void)puts(""))
#define ESP_LOGE ESP_LOGI
#define ESP_LOGW ESP_LOGI
const char *esp_err_to_name(int code);
size_t heap_caps_get_free_size(int caps);
size_t heap_caps_get_largest_free_block(int caps);
int64_t esp_timer_get_time(void);
int bsp_audio_init(void);
int bsp_audio_init_playback(void);
int bsp_audio_deinit(void);
void bsp_audio_set_volume(uint8_t percent);
int bsp_audio_wake(void);
int bsp_audio_sleep(void);
int bsp_audio_set_format(int rate, int bits, int channels);
int bsp_audio_read(void *data, size_t size);
int bsp_audio_write(const void *data, size_t size);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreTake(SemaphoreHandle_t sem, int ticks);
int xSemaphoreGive(SemaphoreHandle_t sem);
void vSemaphoreDelete(SemaphoreHandle_t sem);
QueueHandle_t xQueueCreate(unsigned count, unsigned item_size);
int xQueueReceive(QueueHandle_t q, void *out, int ticks);
int xQueueSend(QueueHandle_t q, const void *in, int ticks);
void xQueueReset(QueueHandle_t q);
void vQueueDelete(QueueHandle_t q);
int xTaskCreate(TaskFunction_t fn, const char *name, int stack, void *arg, int priority, TaskHandle_t *task);
int xTaskCreateWithCaps(TaskFunction_t fn, const char *name, int stack, void *arg, int priority, TaskHandle_t *task, unsigned caps);
void vTaskDelete(TaskHandle_t task);
void vTaskDeleteWithCaps(TaskHandle_t task);
void vTaskSuspend(TaskHandle_t task);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task);
void taskYIELD(void);
void vTaskDelay(int ticks);
unsigned uxQueueMessagesWaiting(QueueHandle_t q);
typedef struct { int sample_rate, channel, bits_per_sample, bitrate, frame_duration, application_mode, complexity, enable_dtx, enable_vbr; } esp_opus_enc_config_t;
typedef struct { int sample_rate, channel, frame_duration; } esp_opus_dec_cfg_t;
#define ESP_OPUS_ENC_CONFIG_DEFAULT() ((esp_opus_enc_config_t){0})
#define ESP_OPUS_DEC_CONFIG_DEFAULT() ((esp_opus_dec_cfg_t){0})
#define ESP_OPUS_ENC_FRAME_DURATION_20_MS 20
#define ESP_OPUS_ENC_FRAME_DURATION_60_MS 60
#define ESP_OPUS_DEC_FRAME_DURATION_60_MS 60
#define ESP_OPUS_ENC_APPLICATION_VOIP 1
#define ESP_OPUS_ENC_APPLICATION_LOWDELAY 2
typedef struct { uint8_t *buffer; unsigned len; } esp_audio_enc_in_frame_t;
typedef struct { uint8_t *buffer; unsigned len, encoded_bytes; } esp_audio_enc_out_frame_t;
typedef struct { uint8_t *buffer; unsigned len; } esp_audio_dec_in_raw_t;
typedef struct { uint8_t *buffer; unsigned len, decoded_size; } esp_audio_dec_out_frame_t;
typedef struct { int unused; } esp_audio_dec_info_t;
typedef struct {int in_frame_size;uint8_t in_frame_align;int out_frame_size;uint8_t out_frame_align;} esp_audio_enc_frame_info_t;
int esp_opus_enc_get_frame_info_by_cfg(void *cfg,esp_audio_enc_frame_info_t *info);
int esp_opus_enc_open(const void *cfg, unsigned size, void **encoder);
int esp_opus_dec_open(const void *cfg, unsigned size, void **decoder);
int esp_opus_enc_process(void *enc, const esp_audio_enc_in_frame_t *in, esp_audio_enc_out_frame_t *out);
int esp_opus_dec_decode(void *dec, const esp_audio_dec_in_raw_t *in, esp_audio_dec_out_frame_t *out, esp_audio_dec_info_t *info);
void esp_opus_enc_close(void *enc);
void esp_opus_dec_close(void *dec);
