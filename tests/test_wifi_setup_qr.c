#include "wifi_setup_qr.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 char out[256];
 assert(ps_wifi_setup_qr_payload("Passport-1234","0123456789ab",out,sizeof(out)));
 assert(!strcmp(out,"WIFI:T:WPA;S:Passport-1234;P:0123456789ab;;"));
 assert(ps_wifi_setup_qr_payload("a;b:c,\"\\","password",out,sizeof(out)));
 assert(!strcmp(out,"WIFI:T:WPA;S:a\\;b\\:c\\,\\\"\\\\;P:password;;"));
 assert(ps_wifi_setup_qr_payload("中文网络","",out,sizeof(out)));
 assert(!strcmp(out,"WIFI:T:nopass;S:中文网络;;"));
 assert(!ps_wifi_setup_qr_payload("x","12345678",out,8)&&!out[0]);
 assert(!ps_wifi_setup_qr_payload("","12345678",out,sizeof(out)));
 assert(!ps_wifi_setup_qr_payload("bad\nname","12345678",out,sizeof(out)));
 assert(!ps_wifi_setup_qr_payload("x","short",out,sizeof(out)));
 char ssid[34],password[65];memset(ssid,';',32);ssid[32]=0;memset(password,'\\',63);password[63]=0;
 assert(ps_wifi_setup_qr_payload(ssid,password,out,sizeof(out)));
 ssid[32]='a';ssid[33]=0;assert(!ps_wifi_setup_qr_payload(ssid,password,out,sizeof(out)));
 ssid[32]=0;password[63]='a';password[64]=0;assert(!ps_wifi_setup_qr_payload(ssid,password,out,sizeof(out)));
 puts("Wi-Fi QR payload PASS WPA/open, UTF-8, escaping, max bounds, small output and control rejection");
}
