#pragma once
#include <stdbool.h>

/* OS-owned percentages. Workers read atomically; settings persist before set.
 * Applications cannot overwrite these system controls. Zero means silent. */
unsigned ps_audio_volume(bool alarm);
void ps_audio_volume_set(bool alarm, unsigned percent);
unsigned ps_audio_volume_adjust(unsigned percent, int direction);
