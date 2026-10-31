#include "wifi_setup_qr.h"
#include <string.h>
static bool append(char *out,size_t cap,size_t *n,const char *value,bool escape) {
 for(const unsigned char*p=(const unsigned char*)value;*p;p++) {
  if(*p<32||*p==127)return false;
  if(escape && strchr("\\;,:\"",*p)) {if(*n+1>=cap)return false;out[(*n)++]='\\';}
  if(*n+1>=cap)return false;
  out[(*n)++]=(char)*p;
 }
 out[*n]=0;return true;
}
bool ps_wifi_setup_qr_payload(const char *ssid,const char *password,char *out,size_t cap) {
 if(!out||!cap)return false;
 out[0]=0;
 if(!ssid||!password||!ssid[0]||strlen(ssid)>32||strlen(password)>63||(password[0]&&strlen(password)<8))return false;
 size_t n=0;
 bool ok=append(out,cap,&n,password[0]?"WIFI:T:WPA;S:":"WIFI:T:nopass;S:",false) &&
  append(out,cap,&n,ssid,true) && append(out,cap,&n,password[0]?";P:":";",false) &&
  (!password[0]||append(out,cap,&n,password,true)) && append(out,cap,&n,password[0]?";;":";",false);
 if(!ok)memset(out,0,cap);
 return ok;
}
