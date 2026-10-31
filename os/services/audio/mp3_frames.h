#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Bounded MPEG Layer III frame assembler, arbitrary chunk boundaries and ID3.
 * Sink borrows one complete frame. Reject unsupported free bitrate/layers. */
typedef bool (*ps_mp3_frame_fn)(const uint8_t *,size_t,void *);
typedef struct { uint8_t data[1536]; unsigned used, skip, discarded; } ps_mp3_frames;
unsigned ps_mp3_frame_size(const uint8_t h[4]);
bool ps_mp3_feed(ps_mp3_frames *,const void *,size_t,ps_mp3_frame_fn,void *);
