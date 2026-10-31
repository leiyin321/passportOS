#pragma once
#include "package.h"
bool ps_store_init(void);
bool ps_store_install(const void *data, size_t n, char id[64]);
bool ps_store_load(const char *id, ps_package *p);
bool ps_store_offload(const char *id);
bool ps_store_pin(const char *id, bool pin);
bool ps_store_remove(const char *id, bool delete_data);
int ps_store_list(char ids[][64], int capacity);
bool ps_data_get(const ps_package *p, const char *key, char *out,
                 size_t capacity);
bool ps_data_set(const ps_package *p, const char *key, const char *value);

int ps_store_registered(char ids[][64], int capacity);
bool ps_store_is_pinned(const char *id);
void ps_store_cache_budget(size_t bytes);

bool ps_store_install_file(const char *source, const char *expected_id,
                           char id[64]);

bool ps_store_icon(const char *id, char out[65]);
bool ps_store_data_hash(const char *id, const char *key, char hex[65]);
bool ps_store_fault_fixture(const char *id, const char *mode);

/* Actual cached PAP file length; zero when offloaded or unavailable. */
size_t ps_store_package_size(const char *id);
