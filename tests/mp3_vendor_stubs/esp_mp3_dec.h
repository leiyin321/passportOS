#pragma once
#include <stdint.h>
#define ESP_AUDIO_ERR_OK 0
typedef struct {uint8_t *buffer;uint32_t len,consumed;int frame_recover;} esp_audio_dec_in_raw_t;
typedef struct {uint8_t *buffer;uint32_t len,needed_size,decoded_size;} esp_audio_dec_out_frame_t;
typedef struct {uint32_t sample_rate;uint8_t bits_per_sample,channel;uint32_t bitrate,frame_size;} esp_audio_dec_info_t;
int esp_mp3_dec_open(void *,uint32_t,void **);
int esp_mp3_dec_decode(void *,esp_audio_dec_in_raw_t *,esp_audio_dec_out_frame_t *,esp_audio_dec_info_t *);
int esp_mp3_dec_close(void *);

int esp_mp3_dec_reset(void *);
