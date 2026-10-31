#pragma once
#include <stdbool.h>
#include <stddef.h>
enum {PS_HID_OFF,PS_HID_STARTING,PS_HID_ADVERTISING,PS_HID_CONNECTED,
      PS_HID_READY,PS_HID_ERROR,PS_HID_STOPPING,PS_HID_UNAVAILABLE};
/* Foreground-only session API. Reports are copied; callbacks never reach apps.
 * Stop is asynchronous, and a new start is refused until teardown completes. */
/* Warm the pinned NimBLE bond namespace before large radio allocations. */
void ps_hid_prepare_storage(void);
bool ps_hid_start(unsigned flags);
unsigned ps_hid_state(void);
bool ps_hid_report(unsigned id,const void *data,size_t bytes);
void ps_hid_stop(void);

/* USB development diagnostics: aggregate flags/counters, no addresses/keys. */
void ps_hid_debug(void);
void ps_hid_trace(bool enabled);
