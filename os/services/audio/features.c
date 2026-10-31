#include "features.h"
#include <math.h>
float ps_audio_rms_percent(const int16_t *samples, size_t count) {
  if (!samples || !count)
    return 0;
  int64_t sum = 0;
  for (size_t i = 0; i < count; i++)
    sum += samples[i];
  int32_t mean = sum / (int64_t)count;
  uint64_t energy = 0;
  for (size_t i = 0; i < count; i++) {
    int64_t ac = (int32_t)samples[i] - mean;
    energy += (uint64_t)(ac * ac);
  }
  float rms = sqrtf((float)energy / count) * 100.f / 32768.f;
  return rms > 100.f ? 100.f : rms;
}
