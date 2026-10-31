#pragma once
#include "passport_native.h"
#include <stddef.h>
/* ABI 2 preserves the ABI 1 prefix. Async callbacks run on the app context.
 * Input data is borrowed only during data(); copy anything retained. */
#define PASSPORT_NATIVE_ABI_V2 2u
enum { PASSPORT_NATIVE_PERMISSION=7, PASSPORT_NATIVE_AUDIO_ERROR=8 };
enum { PASSPORT_NATIVE_HTTP=1, PASSPORT_NATIVE_WS_TEXT=2,
       PASSPORT_NATIVE_WS_BINARY=3, PASSPORT_NATIVE_AUDIO_OPUS=4 };
typedef struct {
    passport_native_host_v1 base;
    int (*network_ready)(void);
    int (*http_request)(const char *method, const char *url, const char *headers,
                        const void *body, uint32_t bytes);
    int (*ws_open)(const char *url, const char *headers);
    int (*ws_send)(const void *data, uint32_t bytes, int binary);
    int (*ws_connected)(void);
    void (*ws_close)(void);
    /* JSON dotted object path, decoded string or scalar JSON; no truncation.
     * >=0 is length excluding NUL; -1 invalid/missing, -2 capacity exceeded. */
    int (*json_get)(const void *json, uint32_t bytes, const char *path,
                    char *out, uint32_t capacity);
    int (*identity)(char *device, uint32_t device_capacity,
                    char *client, uint32_t client_capacity);
    int (*storage_get)(const char *key, char *out, uint32_t capacity);
    int (*storage_set)(const char *key, const char *value);
    /* 1=capture, 0=stop capture/retain playback, -1=close session.
     * Requests are asynchronous; AUDIO_ERROR reports worker startup failure.
     * Mic permission is required. Native uses memory-bounded half-duplex Opus;
     * switch to mode 0 before playback. Mono16kHz capture, 60ms packets. */
    int (*audio_mode)(int capture);
    int (*audio_play_opus)(const void *data, uint32_t bytes);
} passport_native_host_v2;
typedef struct {
    passport_native_app_v1 base;
    void (*data)(uint32_t kind, int32_t status, const void *data, uint32_t bytes);
} passport_native_app_v2;
