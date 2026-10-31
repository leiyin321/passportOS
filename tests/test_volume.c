#include "volume.h"
#include <assert.h>
#include <limits.h>
int main(void) {
  assert(ps_audio_volume(false)==45 && ps_audio_volume(true)==35);
  ps_audio_volume_set(false,0);
  assert(ps_audio_volume(false)==0 && ps_audio_volume(true)==35);
  ps_audio_volume_set(true,100);
  assert(ps_audio_volume(false)==0 && ps_audio_volume(true)==100);
  ps_audio_volume_set(false,UINT_MAX);
  assert(ps_audio_volume(false)==100);
  assert(ps_audio_volume_adjust(0,-1)==0);
  assert(ps_audio_volume_adjust(100,1)==100);
  assert(ps_audio_volume_adjust(97,1)==100);
  assert(ps_audio_volume_adjust(3,-1)==0);
  unsigned value=0;
  for (unsigned i=0;i<20;i++) value=ps_audio_volume_adjust(value,1);
  assert(value==100);
  for (unsigned i=0;i<20;i++) value=ps_audio_volume_adjust(value,-1);
  assert(value==0);
  return 0;
}
