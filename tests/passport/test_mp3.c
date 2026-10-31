#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp3_frames.h"
#include "passport_mp3.h"
#include "../fixtures/mp3_sine.h"
static unsigned frames, samples_total;
static ps_mp3_decoder *decoder;
static bool decode(const uint8_t *p, size_t bytes, void *ctx) {
  (void)ctx;
  int16_t pcm[2304];
  int hz = 0, ch = 0;
  int n = ps_mp3_decoder_decode(decoder, p, bytes, pcm, &hz, &ch);
  frames++;
  assert(hz == 16000 && ch == 1 && n >= 0 && n <= 1152);
  samples_total += (unsigned)n;
  return true;
}
static bool reject(const uint8_t *p, size_t n, void *ctx) {
  (void)p;
  (void)n;
  (void)ctx;
  return false;
}
int main(void) {
  const uint8_t valid[] = {255, 251, 144, 0}, bad[] = {255, 249, 0, 0};
  assert(ps_mp3_frame_size(valid) == 417);
  assert(!ps_mp3_frame_size(bad));
  for (unsigned cycle = 0; cycle < 100; cycle++) {
    decoder = ps_mp3_decoder_open();
    assert(decoder);
    ps_mp3_frames parser = {0};
    frames = samples_total = 0;
    unsigned step = cycle + 1;
    for (unsigned i = 0; i < sizeof(mp3_sine);) {
      unsigned k = sizeof(mp3_sine) - i;
      if (k > step)
        k = step;
      assert(ps_mp3_feed(&parser, mp3_sine + i, k, decode, NULL));
      i += k;
    }
    assert(frames >= 9 && samples_total >= 4800);
    ps_mp3_decoder_close(decoder);
  }
  ps_mp3_frames parser = {0};
  assert(!ps_mp3_feed(&parser, mp3_sine, sizeof(mp3_sine), reject, NULL));
  parser = (ps_mp3_frames){0};
  const uint8_t oversized[] = {73, 68, 51, 4, 0, 0, 0x7f, 0x7f, 0x7f, 0x7f};
  assert(!ps_mp3_feed(&parser, oversized, sizeof(oversized), reject, NULL));
  parser = (ps_mp3_frames){0};
  uint8_t garbage[513] = {0};
  bool ok = true;
  for (unsigned i = 0; i < 130 && ok; i++)
    ok = ps_mp3_feed(&parser, garbage, sizeof(garbage), reject, NULL);
  assert(!ok);
  printf("MP3 PASS 100 decode cycles / chunk boundaries / ID3 / bounded "
         "garbage / sink failure; decoder=%zu parser=%zu bytes\n",
         ps_mp3_decoder_bytes(), sizeof(ps_mp3_frames));
}
