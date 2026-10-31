#ifndef PS_WIFI_SETUP_FORM_H
#define PS_WIFI_SETUP_FORM_H

#include <stdbool.h>
#include <stddef.h>

/* Decode a bounded application/x-www-form-urlencoded body. */
bool ps_wifi_setup_parse_form(const char *body, size_t size, char *ssid,
                              size_t ssid_capacity, char *password,
                              size_t password_capacity);

#endif
