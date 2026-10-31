#pragma once
#include "passport_native_v2.h"
/* Optional ABI 2 extension, OS 1.0.47+. Core prefix remains 80 bytes on C3.
 * Check base.size >= sizeof(passport_native_host_hid_v2) before casting.
 * Native ABI 2 + bluetooth.hid required. Reports are copied, not retained.
 * start(0): bonded HID; start(1): explicitly forget bonds before pairing.
 * state: 0 off,1 starting,2 advertising,3 connected,4 ready,5 error,
 *        6 stopping,7 unavailable. Mouse reports need encrypted subscription.
 * report(1): uint8 buttons[0..7], int8 x/y/wheel[-127..127] (4 bytes).
 * report(2): little-endian Consumer usage (2 bytes): 0 release, B5 next,
 * B6 previous, CD play/pause, E2 mute, E9/Ea volume up/down. No audio/calls.
 * 40 reports/s, copied eight-slot queue; 0 means rejected/busy/disconnected.
 * stop() starts asynchronous teardown; never retains app callbacks or buffers. */
typedef struct {
    passport_native_host_v2 core;
    int (*hid_start)(uint32_t flags);
    uint32_t (*hid_state)(void);
    int (*hid_report)(uint32_t id,const void *data,uint32_t bytes);
    void (*hid_stop)(void);
} passport_native_host_hid_v2;
