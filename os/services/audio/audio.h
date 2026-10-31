#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
bool ps_audio_start(void);
bool ps_audio_stop(void);
int ps_audio_level(void);
bool ps_audio_stream_start(void);
bool ps_audio_stream_prepare(void);
bool ps_audio_stream_prepared(void);
bool ps_audio_stream_active(void);
bool ps_audio_stream_stop(void);
bool ps_audio_stream_poll(uint8_t *data, size_t capacity, size_t *size);
bool ps_audio_stream_play_opus(const uint8_t *data, size_t size);

bool ps_audio_stream_playback_start(void);

/* Select before worker creation; cannot change policy while a worker exists. */
bool ps_audio_stream_half_duplex(bool enabled);

bool ps_audio_stream_play_opus_wait(const uint8_t *data,size_t size,unsigned wait_ms);

bool ps_audio_busy(void);
/* Reserve output for an OS playback service; foreground acquires, worker releases.
 * Blocks capture/Opus worker creation until the owner has actually finished. */
bool ps_audio_output_claim(void);
void ps_audio_output_release(void);
