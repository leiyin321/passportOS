#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PS_MAX_RESOURCES 16
typedef void (*ps_release_fn)(void *);
typedef struct {
  void *value;
  ps_release_fn release;
  uint32_t token;
} ps_resource;
typedef struct {
  uint32_t generation;
  bool active;
  unsigned count;
  ps_resource resources[PS_MAX_RESOURCES];
} ps_session;
void ps_session_begin(ps_session *s);
uint32_t ps_session_own(ps_session *s, void *value, ps_release_fn release);
bool ps_session_valid(const ps_session *s, uint32_t token);
void ps_session_end(ps_session *s);
