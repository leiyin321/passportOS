#pragma once
#include "runtime.h"
bool ps_native_start(ps_package *p, ps_view *view, const ps_display_layout *display,
                     const char *language, const char *mode, uint32_t generation, char *error, size_t capacity);
bool ps_native_event(const char *callback, int value);
void ps_native_stop(void);

void ps_native_audio_allowed(bool allowed);
void ps_native_data(unsigned kind, int status, const void *data, size_t bytes);
