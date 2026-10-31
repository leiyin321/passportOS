#include "wifi_profiles.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 ps_wifi_profiles p={.version=1};assert(ps_wifi_profiles_valid(&p));
 assert(ps_wifi_profiles_save(&p,"A","12345678"));assert(ps_wifi_profiles_save(&p,"B","abcdefgh"));
 assert(ps_wifi_profiles_save(&p,"A","87654321")&&p.count==2&&!strcmp(p.items[0].password,"87654321"));
 assert(ps_wifi_profiles_success(&p,"B")&&!strcmp(p.items[0].ssid,"B"));
 int8_t r[8]={-90,-30,-128,-128,-128,-128,-128,-128};assert(ps_wifi_profiles_pick(&p,r,0)==0);assert(ps_wifi_profiles_pick(&p,r,1)==1);
 r[0]=-128;assert(ps_wifi_profiles_pick(&p,r,0)==1);assert(ps_wifi_profiles_pick(&p,r,2)==-1);
 for(int i=2;i<8;i++){char s[8];snprintf(s,sizeof(s),"N%d",i);assert(ps_wifi_profiles_save(&p,s,""));}
 assert(!ps_wifi_profiles_save(&p,"overflow","12345678")&&p.count==8);
 assert(ps_wifi_profiles_forget(&p,"B")&&p.count==7);assert(!strcmp(p.items[0].ssid,"A"));
 assert(!memcmp(&p.items[7],&(ps_wifi_profile){0},sizeof(ps_wifi_profile)));
 assert(!ps_wifi_profiles_save(&p,"","12345678"));assert(!ps_wifi_profiles_save(&p,"bad","short"));
 ps_wifi_profiles copy=p;assert(ps_wifi_profiles_valid(&copy));copy.version=2;assert(!ps_wifi_profiles_valid(&copy));copy=p;copy.count=9;assert(!ps_wifi_profiles_valid(&copy));
 copy=p;memset(copy.items[0].ssid,'a',33);assert(!ps_wifi_profiles_valid(&copy));
 copy=p;copy.items[1]=copy.items[0];assert(!ps_wifi_profiles_valid(&copy));
 while(p.count){char s[33];strcpy(s,p.items[0].ssid);assert(ps_wifi_profiles_forget(&p,s));}
 assert(ps_wifi_profiles_valid(&p));assert(ps_wifi_profiles_pick(&p,r,0)==-1);
 printf("Wi-Fi profiles PASS migration format, update, full rejection, MRU, signal fallback, tried exclusion, forget/clear, malformed blob; blob=%zu bytes\n",sizeof(p));
}
