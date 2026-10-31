#pragma once
#include <stdbool.h>
#include <stddef.h>
bool ps_trust_key_id(const char *pem, char out[17]);
bool ps_trust_lookup(const char *key_id, char *pem, size_t capacity);
bool ps_trust_add(const char *pem);

bool ps_trust_native_allowed(const char *key_id);
