#pragma once
#include "passport_native_hid.h"
/* Optional ABI 2 extension, OS 1.0.51+. Prefix remains compatible (120B C3).
 * audio.playback + network.fetch required for media_play (HTTPS MP3).
 * OS 1.0.56+: audio.http additionally permits public HTTP/80 broadcast
 * audio GETs on exact manifest domains, no authentication or private LAN.
 * Asynchronous copied URL, exact manifest domains, DNS/TLS/redirect policy.
 * One foreground session. No app callback/buffer retained by workers.
 * state: 0 idle,1 buffering,2 playing,3 stopping,4 failed,5 ended.
 * stop is asynchronous; a replacement start waits for idle/failed/ended.
 * volume(-1) queries; 0..100 changes system media volume for this boot.
 * wall_time is Unix seconds, zero if unsynchronized; battery -1 unavailable.
 * Playback requires no microphone permission and stops on app exit/alarm. */
typedef struct {
    passport_native_host_hid_v2 hid;
    int (*media_play)(const char *url);
    void (*media_stop)(void);
    uint32_t (*media_state)(void);
    int (*media_volume)(int percent);
    uint32_t (*wall_time)(void);
    int (*battery_percent)(void);
} passport_native_host_media_v2;
