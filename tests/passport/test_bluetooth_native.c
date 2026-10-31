#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../examples/bluetooth_peripheral/main.c"
static char ui[1024];
static unsigned state, starts, stops, last_flags, packets;
static int reject;
static uint32_t clock_ms;
static struct {
  unsigned id, n;
  unsigned char d[4];
} sent[128];
static int txt(const char *s, uint32_t c) {
  (void)c;
  assert(strlen(s) < 512);
  snprintf(ui, sizeof(ui), "%s", s);
  return 0;
}
static int clear(uint32_t c) {
  (void)c;
  return 0;
}
static int rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  (void)c;
  assert(x >= 0 && y >= 0 && w > 0 && h > 0);
  return 0;
}
static uint32_t ms(void) { return clock_ms; }
static const char *lang(void) { return "zh-CN"; }
static const char *theme(void) { return "dark"; }
static int begin(uint32_t flags) {
  starts++;
  last_flags = flags;
  state = 2;
  return 1;
}
static uint32_t status(void) { return state; }
static int report(uint32_t id, const void *p, uint32_t n) {
  if (reject || state != 4)
    return 0;
  assert(packets < 128);
  sent[packets].id = id;
  sent[packets].n = n;
  memcpy(sent[packets++].d, p, n);
  return 1;
}
static void end(void) {
  stops++;
  state = 0;
}
static const passport_native_host_hid_v2 host = {
    .core = {.base = {2, sizeof(host), txt, clear, rect, ms, lang, theme}},
    .hid_start = begin,
    .hid_state = status,
    .hid_report = report,
    .hid_stop = end};
static passport_native_app_v2 app;
static void tick(void) {
  clock_ms += 50;
  app.base.event(PASSPORT_NATIVE_TICK, 50);
}
static void shortkey(int v) { app.base.event(PASSPORT_NATIVE_SHORT, v); }
static void reset(void) {
  state = 0;
  packets = starts = stops = last_flags = 0;
  reject = 0;
  assert(!passport_app_entry(&host.core.base, &app.base));
  menu = 0;
}
int main(void) {
  passport_native_host_v2 old = {
      .base = {2, sizeof(old), txt, clear, rect, ms, lang, theme}};
  assert(passport_app_entry(&old.base, &app.base) < 0);
  reset();
  assert(!starts && strstr(ui, "iPhone") && strstr(ui, "清除配对"));
  shortkey(0);
  assert(menu == 1);
  shortkey(1);
  assert(menu == 0);
  shortkey(2);
  assert(starts == 1 && !last_flags && !packets);
  shortkey(0);
  tick();
  assert(!packets);
  state = 4;
  tick();
  packets = 0;
  shortkey(0);
  tick();
  tick();
  assert(packets == 2 && sent[0].id == 1 && sent[0].d[0] == 2 &&
         sent[1].d[0] == 0);
  shortkey(1);
  tick();
  tick();
  assert(packets == 4 && sent[2].d[0] == 4 && sent[3].d[0] == 0);
  shortkey(2);
  tick();
  tick();
  assert(packets == 6 && sent[4].d[0] == 1 && sent[5].d[0] == 0);
  unsigned prior = packets;
  app.base.event(PASSPORT_NATIVE_DOUBLE, 2);
  assert(packets == prior);
  app.base.event(PASSPORT_NATIVE_LONG, 2);
  assert(mode < 0);
  shortkey(1);
  shortkey(2);
  assert(mode == 1 && starts == 1);
  shortkey(0);
  assert(starts == 1);
  app.base.event(PASSPORT_NATIVE_LONG, 2);
  assert(mode < 0 && !pending_reset);
  shortkey(2);
  assert(mode == 1);
  shortkey(2);
  assert(pending_reset && starts == 1);
  tick();
  assert(starts == 2 && last_flags == 1 && mode == 0);
  reset();
  shortkey(2);
  state = 4;
  shortkey(2);
  reject = 1;
  for (unsigned i = 0; i < 21; i++)
    tick();
  assert(stops && !gesture.active);
  app.base.stop();
  assert(stops);
  puts("Native Bluetooth PASS two-entry navigation/ABI/offline/button "
       "2-3-1/release/forget confirmation/cancel/backpressure");
}
