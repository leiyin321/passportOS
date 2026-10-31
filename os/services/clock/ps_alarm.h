#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
  uint8_t hour;
  uint8_t minute;
  bool enabled;
} ps_alarm_config;

/* True once per matching local calendar day; last_day starts at INT32_MIN. */
bool ps_alarm_due(const ps_alarm_config *alarm, const struct tm *local,
                  int32_t *last_day);
void ps_alarm_adjust(ps_alarm_config *alarm, int field, int direction);

