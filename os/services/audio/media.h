#pragma once
#include "package.h"
#include <stdbool.h>
#include <stdint.h>
bool ps_media_play(const ps_package *,const char *url,uint32_t generation);
void ps_media_stop(void);
unsigned ps_media_state(void);
bool ps_media_active(void);
/* App teardown joins network worker before allowing another audio owner. */
bool ps_media_close(void);
