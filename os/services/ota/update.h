#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS_UPDATE_MANIFEST_GENERATION 0xfffffffdu
#define PS_UPDATE_IMAGE_GENERATION 0xfffffffcu

typedef enum {
  PS_UPDATE_IDLE,
  PS_UPDATE_CHECKING,
  PS_UPDATE_CURRENT,
  PS_UPDATE_AVAILABLE,
  PS_UPDATE_DOWNLOADING,
  PS_UPDATE_READY,
  PS_UPDATE_INSTALLING,
  PS_UPDATE_ERROR
} ps_update_state_t;

typedef struct {
  ps_update_state_t state;
  char version[32];
  char error[64];
  size_t downloaded;
  size_t image_size;
  uint32_t build;
} ps_update_info;

void ps_update_init(void);
bool ps_update_check(const char *manifest_url);
bool ps_update_accept_manifest(int status, const char *data, size_t size);
bool ps_update_start_download(void);
bool ps_update_finish_download(int status, size_t received);
bool ps_update_install(void);
const ps_update_info *ps_update_get(void);
void ps_update_read(ps_update_info *out);
uint32_t ps_update_pending_build(void);
bool ps_update_confirm_pending(void);
void ps_update_clear_pending(void);
