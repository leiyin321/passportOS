#include "wifi_setup_form.h"
#include <assert.h>
#include <string.h>

int main(void) {
  char ssid[33], password[64];
  const char good[] = "ssid=Studio+WiFi&password=p%40ssword%21";
  assert(ps_wifi_setup_parse_form(good, sizeof(good) - 1, ssid, sizeof(ssid),
                                 password, sizeof(password)));
  assert(!strcmp(ssid, "Studio WiFi"));
  assert(!strcmp(password, "p@ssword!"));
  const char open_network[] = "ssid=Guest&password=";
  assert(ps_wifi_setup_parse_form(open_network, sizeof(open_network) - 1, ssid,
                                 sizeof(ssid), password, sizeof(password)));
  assert(password[0] == 0);
  const char too_short[] = "ssid=Guest&password=short";
  assert(!ps_wifi_setup_parse_form(too_short, sizeof(too_short) - 1, ssid,
                                  sizeof(ssid), password, sizeof(password)));
  const char malformed[] = "ssid=Bad%Q0&password=12345678";
  assert(!ps_wifi_setup_parse_form(malformed, sizeof(malformed) - 1, ssid,
                                  sizeof(ssid), password, sizeof(password)));
  const char duplicate[] = "ssid=one&ssid=two&password=12345678";
  assert(!ps_wifi_setup_parse_form(duplicate, sizeof(duplicate) - 1, ssid,
                                  sizeof(ssid), password, sizeof(password)));
  return 0;
}
