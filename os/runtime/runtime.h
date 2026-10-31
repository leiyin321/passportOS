#pragma once
#include "package.h"
#include "display_layout.h"
#include <stdint.h>
#define PS_DRAW_MAX 192
#define PS_IMAGE_MAX 16
typedef struct {
  uint32_t handle, color;
  int16_t x, y;
  uint16_t size;
  uint8_t system;
} ps_image_command;
typedef struct {
  int16_t x, y, w, h;
  uint32_t color;
} ps_draw;
typedef struct {
  ps_draw commands[PS_DRAW_MAX];
  unsigned count;
  ps_image_command images[PS_IMAGE_MAX];
  unsigned image_count;
  char text[512];
  uint32_t text_color;
  bool microphone;
  bool audio_stream;
  unsigned lua_used, lua_peak;
} ps_view;
// Consumes the heap-owned package code on compilation/parsing. Reload the
// package before another start; manifest and assets remain owned by the caller.
bool ps_runtime_start(ps_package *package);
bool ps_runtime_event(const char *callback, int value);
void ps_runtime_stop(void);
const ps_view *ps_runtime_view(void);
const char *ps_runtime_error(void);
const ps_display_layout *ps_runtime_display(void);
void ps_runtime_audio_allowed(bool allowed);
void ps_runtime_audio_error(int code);
#define PS_AUDIO_ERROR_START_FAILED 1
void ps_runtime_set_system_language(const char *language);
void ps_runtime_set_system_display_mode(const char *mode);

bool ps_runtime_result(uint32_t generation, int status, const char *data,
                       size_t size);
void ps_runtime_message(const char *data, size_t size, int opcode);
void ps_runtime_audio_frame(const uint8_t *data, size_t size);

bool ps_runtime_identity(const ps_package *package, char *device, size_t device_capacity, char *client, size_t client_capacity);
