#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Standard Wi-Fi QR text; never log the result (it contains credentials). */
bool ps_wifi_setup_qr_payload(const char *ssid,const char *password,char *out,size_t capacity);
