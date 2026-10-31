#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
void *native_exec_alloc(size_t size);
bool native_exec_owns(const void *ptr);
void native_exec_free(void *ptr);
void native_exec_sync(void);
size_t native_exec_capacity(void);
size_t native_exec_used(void);
size_t native_exec_peak(void);
esp_err_t native_exec_verify(void);
void native_exec_dump(void);
bool native_exec_is_clear(void);
