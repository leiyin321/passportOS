#include "session.h"
void ps_session_begin(ps_session *s) {
  if (s->active)
    ps_session_end(s);
  s->generation = (s->generation + 1) & 0x00ffffff;
  if (!s->generation)
    s->generation = 1;
  s->active = true;
  s->count = 0;
}
uint32_t ps_session_own(ps_session *s, void *v, ps_release_fn fn) {
  if (!s->active || !fn || s->count == PS_MAX_RESOURCES)
    return 0;
  unsigned i = s->count++;
  uint32_t t = (s->generation << 8) | (i + 1);
  s->resources[i] = (ps_resource){v, fn, t};
  return t;
}
bool ps_session_valid(const ps_session *s, uint32_t t) {
  unsigned i = (t & 255);
  return s->active && i && i <= s->count && s->resources[i - 1].token == t;
}
void ps_session_end(ps_session *s) {
  s->active = false;
  while (s->count) {
    ps_resource *r = &s->resources[--s->count];
    r->release(r->value);
    *r = (ps_resource){0};
  }
}
