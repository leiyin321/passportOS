#include "ps_audio_features.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
int main(void){int16_t pcm[320];for(int i=0;i<320;i++)pcm[i]=20000;assert(ps_audio_rms_percent(pcm,320)==0);for(int i=0;i<320;i++)pcm[i]=i%2?16384:-16384;assert(fabsf(ps_audio_rms_percent(pcm,320)-50.f)<.01f);for(int i=0;i<320;i++)pcm[i]=i%2?32767:-32768;assert(ps_audio_rms_percent(pcm,320)>99.9f);assert(ps_audio_rms_percent(NULL,0)==0);puts("A03 PASS DC rejection, RMS amplitude, full-scale bounds, empty input");}
