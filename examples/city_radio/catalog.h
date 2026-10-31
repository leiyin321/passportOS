#pragma once
#include <stdbool.h>
#define RADIO_CITIES 7
#define RADIO_STATIONS 14
#define RADIO_HISTORY 10
typedef struct { const char *zh,*en,*initials; } radio_city;
typedef struct { unsigned city,source;const char *zh,*en; } radio_station;
extern const radio_city radio_cities[RADIO_CITIES];
extern const radio_station radio_stations[RADIO_STATIONS];
unsigned radio_city_matches(const char *prefix,unsigned results[RADIO_CITIES]);
void radio_remember(unsigned history[RADIO_HISTORY],unsigned *count,unsigned station);
