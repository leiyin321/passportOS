#pragma once
#include "runtime.h"
bool ps_declarative_start(ps_package *app, ps_view *view, uint32_t generation,
                          const ps_display_layout *display);
bool ps_declarative_event(const char *callback, int value);
void ps_declarative_result(int status, const char *data, size_t size);
void ps_declarative_stop(void);
