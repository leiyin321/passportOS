#include <assert.h>
#include <stdio.h>
#include "esp_mp3_dec.h"
#include "passport_mp3.h"
static int open_error,decode_error;
static unsigned output_bytes=4608,bits=16,channels=2,opened,closed;
int esp_mp3_dec_open(void *cfg,uint32_t size,void **out){assert(!cfg&&!size);if(open_error)return -1;*out=(void*)1;opened++;return 0;}
int esp_mp3_dec_decode(void*d,esp_audio_dec_in_raw_t*in,esp_audio_dec_out_frame_t*out,esp_audio_dec_info_t*info){assert(d==(void*)1&&in->buffer&&(in->len==4||in->len==417)&&out->len==4608);out->decoded_size=output_bytes;info->channel=channels;info->bits_per_sample=bits;info->sample_rate=44100;return decode_error;}
int esp_mp3_dec_reset(void*d){assert(d==(void*)1);return 0;}
int esp_mp3_dec_close(void*d){assert(d==(void*)1);closed++;return 0;}
int main(void){
 open_error=1;assert(!ps_mp3_decoder_open());open_error=0;
 for(unsigned i=0;i<100;i++){
  ps_mp3_decoder*d=ps_mp3_decoder_open();assert(d);int hz,ch;int16_t pcm[2304];uint8_t in[4]={0};
  assert(ps_mp3_decoder_decode(d,in,4,pcm,&hz,&ch)==1152&&hz==44100&&ch==2);
  output_bytes=4609;assert(!ps_mp3_decoder_decode(d,in,4,pcm,&hz,&ch));output_bytes=4608;
  channels=0;assert(!ps_mp3_decoder_decode(d,in,4,pcm,&hz,&ch));channels=2;
  bits=8;assert(!ps_mp3_decoder_decode(d,in,4,pcm,&hz,&ch));bits=16;
  decode_error=-1;assert(!ps_mp3_decoder_decode(d,in,4,pcm,&hz,&ch));decode_error=0;
  ps_mp3_decoder_close(d);
 }
 assert(opened==100&&closed==100&&ps_mp3_decoder_bytes()==0);
 puts("Device MP3 adapter PASS vendor API contract, OOM, 100 open/close cycles, bounded PCM/format/errors; codec itself requires board tests");
}
