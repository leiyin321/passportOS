#include "ps_alarm.h"

#include <limits.h>

bool ps_alarm_due(const ps_alarm_config *alarm, const struct tm *local,
                  int32_t *last_day) {
  if (!alarm || !local || !last_day || !alarm->enabled || alarm->hour > 23 ||
      alarm->minute > 59 || local->tm_hour != alarm->hour ||
      local->tm_min != alarm->minute)
    return false;
  int32_t day = (int32_t)(local->tm_year + 1900) * 512 + local->tm_yday;
  if (*last_day == day) return false;
  *last_day = day;
  return true;
}

void ps_alarm_adjust(ps_alarm_config *alarm, int field, int direction) {
  if (!alarm || !direction) return;
  if (field == 0) {
    alarm->hour = (uint8_t)((alarm->hour + (direction > 0 ? 1 : 23)) % 24);
  } else if (field == 1) {
    alarm->minute = (uint8_t)((alarm->minute + (direction > 0 ? 1 : 59)) % 60);
  } else {
    alarm->enabled = !alarm->enabled;
  }
}
