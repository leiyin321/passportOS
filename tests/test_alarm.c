#include "ps_alarm.h"

#include <assert.h>
#include <limits.h>

int main(void) {
  ps_alarm_config a = {.hour = 7, .minute = 30, .enabled = true};
  struct tm t = {.tm_year = 126, .tm_yday = 297, .tm_hour = 7, .tm_min = 29};
  int32_t last = INT32_MIN;
  assert(!ps_alarm_due(&a, &t, &last));
  t.tm_min = 30;
  t.tm_sec = 17; // delayed main-loop check must still ring during this minute
  assert(ps_alarm_due(&a, &t, &last));
  assert(!ps_alarm_due(&a, &t, &last));
  t.tm_yday++;
  assert(ps_alarm_due(&a, &t, &last));
  a.enabled = false;
  t.tm_yday++;
  assert(!ps_alarm_due(&a, &t, &last));
  a.hour = 23; a.minute = 59;
  ps_alarm_adjust(&a, 0, 1); assert(a.hour == 0);
  ps_alarm_adjust(&a, 1, -1); assert(a.minute == 58);
  ps_alarm_adjust(&a, 2, 1); assert(a.enabled);
  return 0;
}
