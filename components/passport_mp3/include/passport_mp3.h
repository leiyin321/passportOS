#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct ps_mp3_decoder ps_mp3_decoder;
ps_mp3_decoder *ps_mp3_decoder_open(void);
/* Heap-owned scratch, no per-frame allocations. Samples per channel or zero. */
int ps_mp3_decoder_decode(ps_mp3_decoder *, const uint8_t *,size_t,int16_t pcm[2304],int *hz,int *channels);
void ps_mp3_decoder_close(ps_mp3_decoder *);
size_t ps_mp3_decoder_bytes(void);
