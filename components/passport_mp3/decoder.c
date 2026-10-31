#include "passport_mp3.h"
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "esp_mp3_dec.h"
struct ps_mp3_decoder {void *decoder;};
ps_mp3_decoder *ps_mp3_decoder_open(void) {
  ps_mp3_decoder *d=calloc(1,sizeof(*d));
  if(d && esp_mp3_dec_open(NULL,0,&d->decoder)!=ESP_AUDIO_ERR_OK){free(d);return NULL;}
  if(d) {
    /* Allocate the vendor's lazy working set before network/queue allocations.
     * Valid MPEG-1 Layer III stereo silence; reset discards warmup reservoir. */
    uint8_t silence[417]={0xff,0xfb,0x90,0};
    uint8_t *pcm=malloc(4608);
    esp_audio_dec_in_raw_t raw={.buffer=silence,.len=sizeof(silence)};
    esp_audio_dec_out_frame_t out={.buffer=pcm,.len=4608};
    esp_audio_dec_info_t info={0};
    if(!pcm || esp_mp3_dec_decode(d->decoder,&raw,&out,&info)!=ESP_AUDIO_ERR_OK ||
       esp_mp3_dec_reset(d->decoder)!=ESP_AUDIO_ERR_OK){free(pcm);esp_mp3_dec_close(d->decoder);free(d);return NULL;}
    free(pcm);
  }
  return d;
}
int ps_mp3_decoder_decode(ps_mp3_decoder *d,const uint8_t *p,size_t n,int16_t pcm[2304],int *hz,int *channels) {
  esp_audio_dec_in_raw_t raw={.buffer=(uint8_t *)p,.len=n};
  esp_audio_dec_out_frame_t out={.buffer=(uint8_t *)pcm,.len=4608};
  esp_audio_dec_info_t info={0};
  int rc=esp_mp3_dec_decode(d->decoder,&raw,&out,&info);
  *hz=(int)info.sample_rate;*channels=info.channel;
  if(rc!=ESP_AUDIO_ERR_OK || out.decoded_size>4608 || info.bits_per_sample!=16 ||
     (info.channel!=1 && info.channel!=2) || out.decoded_size%(2*info.channel))return 0;
  return (int)(out.decoded_size/(2*info.channel));
}
void ps_mp3_decoder_close(ps_mp3_decoder *d){if(d){esp_mp3_dec_close(d->decoder);free(d);}}
/* Opaque vendor codec allocations are measured on device, not sizeof wrapper. */
size_t ps_mp3_decoder_bytes(void){return 0;}
#else
/* Portable reference decoder for host format/queue tests; device uses the
 * pinned Espressif integer decoder above, not scalar floating point on C3. */
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
struct ps_mp3_decoder {mp3dec_t decoder;mp3dec_scratch_t scratch;};
ps_mp3_decoder *ps_mp3_decoder_open(void) {
  ps_mp3_decoder *d=calloc(1,sizeof(*d));if(d)mp3dec_init(&d->decoder);return d;
}
int ps_mp3_decoder_decode(ps_mp3_decoder *d,const uint8_t *p,size_t n,int16_t pcm[2304],int *hz,int *channels) {
  mp3dec_frame_info_t info={0};
  int samples=mp3dec_decode_frame(&d->decoder,p,(int)n,pcm,&info,&d->scratch);
  *hz=info.hz;*channels=info.channels;return samples;
}
void ps_mp3_decoder_close(ps_mp3_decoder *d){free(d);}
size_t ps_mp3_decoder_bytes(void){return sizeof(ps_mp3_decoder);}
#endif
