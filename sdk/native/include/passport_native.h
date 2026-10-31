#pragma once
#include <stdint.h>
#define PASSPORT_NATIVE_ABI 1u
/* Same ESP32-C3 ILP32 toolchain. Fields may only be appended in future ABIs.
 * Calls are synchronous on the foreground app context. No retained OS resources. */
enum { PASSPORT_NATIVE_PRESS=1, PASSPORT_NATIVE_RELEASE, PASSPORT_NATIVE_SHORT,
       PASSPORT_NATIVE_DOUBLE, PASSPORT_NATIVE_LONG, PASSPORT_NATIVE_TICK };
typedef struct {
    uint32_t abi, size;
    int (*text)(const char *utf8, uint32_t color);
    int (*clear)(uint32_t background);
    int (*rect)(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    uint32_t (*millis)(void);
    const char *(*language)(void);
    const char *(*display_mode)(void);
} passport_native_host_v1;
typedef struct {
    uint32_t abi, size;
    void (*event)(uint32_t event, int32_t value);
    void (*stop)(void);
} passport_native_app_v1;
typedef int (*passport_native_entry_v1)(const passport_native_host_v1 *, passport_native_app_v1 *);
/* Exported ELF entry. Return 0 after filling callbacks, or a negative error.
 * All entry/callback calls must return promptly; there is no preemption sandbox. */
int passport_app_entry(const passport_native_host_v1 *, passport_native_app_v1 *);
