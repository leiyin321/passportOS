#pragma once
typedef void *esp_transport_handle_t;
typedef void *esp_transport_list_handle_t;
typedef int (*connect_func)(esp_transport_handle_t,const char*,int,int);
