#include "wifi_profiles.h"
#include <string.h>
static bool credential(const char*s,const char*p) {
 size_t n=strnlen(s,33),m=strnlen(p,64);
 return n>0&&n<=32&&(m==0||(m>=8&&m<=63));
}
int ps_wifi_profiles_find(const ps_wifi_profiles*p,const char*s){
 if(!p||!s)return -1;
 for(unsigned i=0;i<p->count;i++)if(!strcmp(p->items[i].ssid,s))return (int)i;
 return -1;
}
bool ps_wifi_profiles_valid(const ps_wifi_profiles*p){
 if(!p||p->version!=1||p->count>PS_WIFI_PROFILES||p->reserved[0]||p->reserved[1])return false;
 for(unsigned i=0;i<p->count;i++){
  if(!credential(p->items[i].ssid,p->items[i].password))return false;
  for(unsigned j=0;j<i;j++)if(!strcmp(p->items[i].ssid,p->items[j].ssid))return false;
 }
 return true;
}
bool ps_wifi_profiles_save(ps_wifi_profiles*p,const char*s,const char*password){
 if(!p||!s||!password||!credential(s,password))return false;
 int i=ps_wifi_profiles_find(p,s);
 if(i<0){if(p->count==PS_WIFI_PROFILES)return false;i=p->count++;}
 memset(&p->items[i],0,sizeof(p->items[i]));
 memcpy(p->items[i].ssid,s,strlen(s));memcpy(p->items[i].password,password,strlen(password));return true;
}
bool ps_wifi_profiles_forget(ps_wifi_profiles*p,const char*s){
 int i=ps_wifi_profiles_find(p,s);if(i<0)return false;
 memmove(p->items+i,p->items+i+1,(p->count-i-1)*sizeof(p->items[0]));
 memset(&p->items[--p->count],0,sizeof(p->items[0]));return true;
}
bool ps_wifi_profiles_success(ps_wifi_profiles*p,const char*s){
 int i=ps_wifi_profiles_find(p,s);if(i<=0)return false;
 ps_wifi_profile item=p->items[i];memmove(p->items+1,p->items,i*sizeof(item));p->items[0]=item;
 memset(&item,0,sizeof(item));return true;
}
int ps_wifi_profiles_pick(const ps_wifi_profiles*p,const int8_t rssi[PS_WIFI_PROFILES],uint8_t tried){
 if(!p||!rssi)return -1;
 if(p->count&&!(tried&1)&&rssi[0]!=-128)return 0;
 int best=-1;
 for(unsigned i=1;i<p->count;i++)if(!(tried&(1u<<i))&&rssi[i]!=-128&&(best<0||rssi[i]>rssi[best]))best=(int)i;
 return best;
}
