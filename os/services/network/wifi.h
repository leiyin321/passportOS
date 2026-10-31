#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "portal_flow.h"
void ps_wifi_init(void);
/* Queue save/update and connection; true means accepted, not yet connected.
 * Up to eight networks, persisted by the Wi-Fi worker. */
bool ps_wifi_configure(const char *ssid, const char *password);
bool ps_wifi_online(void);
/* Copy the actual associated SSID, never a saved/pending target. */
bool ps_wifi_connected_ssid(char *out, size_t capacity);
const char *ps_wifi_state(void);
bool ps_wifi_setup_start(char *ssid, size_t ssid_capacity, char *password,
                         size_t password_capacity);
bool ps_wifi_setup_active(void);
bool ps_wifi_setup_ready_to_stop(void);
bool ps_wifi_setup_phone_connected(void);
ps_portal_state ps_wifi_setup_state(void);
bool ps_wifi_setup_waiting_hotspot(void);
void ps_wifi_setup_poll(void);
void ps_wifi_setup_stop(void);
