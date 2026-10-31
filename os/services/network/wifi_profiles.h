#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PS_WIFI_PROFILES 8
/* Versioned fixed blob; no pointers/padding in persistent representation. */
typedef struct { char ssid[33],password[64]; } ps_wifi_profile;
typedef struct { uint8_t version,count,reserved[2]; ps_wifi_profile items[PS_WIFI_PROFILES]; } ps_wifi_profiles;
bool ps_wifi_profiles_valid(const ps_wifi_profiles *p);
int ps_wifi_profiles_find(const ps_wifi_profiles *p,const char *ssid);
bool ps_wifi_profiles_save(ps_wifi_profiles *p,const char *ssid,const char *password);
bool ps_wifi_profiles_forget(ps_wifi_profiles *p,const char *ssid);
bool ps_wifi_profiles_success(ps_wifi_profiles *p,const char *ssid);
/* Among visible, untried networks prefer most recently successful (index 0).
 * Caller supplies strongest signal for each profile, -128 means absent. */
int ps_wifi_profiles_pick(const ps_wifi_profiles *p,const int8_t rssi[PS_WIFI_PROFILES],uint8_t tried);
