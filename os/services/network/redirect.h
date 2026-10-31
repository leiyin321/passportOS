#pragma once
#include <stdbool.h>
#include <stddef.h>
#define PS_REDIRECT_MAX 3
bool ps_redirect_status(int status);
bool ps_redirect_url(const char *base, const char *location, char *out,
                     size_t capacity);
