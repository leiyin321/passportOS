#include "volume.h"
#include <stdatomic.h>

static atomic_uint media_volume = 45, alarm_volume = 35;
unsigned ps_audio_volume(bool alarm) {
  return atomic_load(alarm ? &alarm_volume : &media_volume);
}
void ps_audio_volume_set(bool alarm, unsigned percent) {
  atomic_store(alarm ? &alarm_volume : &media_volume, percent > 100 ? 100 : percent);
}
unsigned ps_audio_volume_adjust(unsigned percent, int direction) {
  if (percent > 100) percent = 100;
  if (direction > 0) return percent > 95 ? 100 : percent + 5;
  if (direction < 0) return percent < 5 ? 0 : percent - 5;
  return percent;
}
