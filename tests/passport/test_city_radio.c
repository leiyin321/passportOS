#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../examples/city_radio/main.c"
static uint32_t now,media;
static unsigned plays,stops,writes_seen;
static int online=1,vol=45,light;
static char last_url[128],saved[100],quality_saved[16],ui[512];
static int txt(const char *s,uint32_t c){(void)c;assert(strlen(s)<sizeof(ui));strcpy(ui,s);return 0;}
static int clear(uint32_t c){(void)c;return 0;}
static int rect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t c){(void)c;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=240&&y+h<=264);return 0;}
static uint32_t ms(void){return now;}
static const char *lang(void){return light?"en":"zh-CN";}
static const char *theme(void){return "dark";}
static int net(void){return online;}
static int get(const char*k,char*out,uint32_t cap){const char *v=!strcmp(k,"quality")?quality_saved:saved;if(!v[0])return 0;assert(strlen(v)<cap);strcpy(out,v);return 1;}
static int set(const char*k,const char*v){char *out=!strcmp(k,"quality")?quality_saved:saved;assert(strlen(v)<(!strcmp(k,"quality")?sizeof(quality_saved):sizeof(saved)));strcpy(out,v);writes_seen++;return 1;}
static int play(const char*u){assert(media!=1&&media!=2&&media!=3);strcpy(last_url,u);plays++;media=1;return 1;}
static void end(void){stops++;media=0;}
static uint32_t state(void){return media;}
static int volume(int v){if(v>=0)vol=v;return vol;}
static uint32_t wall(void){return 1793318400;}
static int battery(void){return 75;}
static const passport_native_host_media_v2 host={
 .hid={.core={.base={2,sizeof(host),txt,clear,rect,ms,lang,theme},.network_ready=net,.storage_get=get,.storage_set=set}},
 .media_play=play,.media_stop=end,.media_state=state,.media_volume=volume,.wall_time=wall,.battery_percent=battery
};
static passport_native_app_v2 app;
static void key(unsigned e,int k){app.base.event(e,k);}
static void click(int k){key(PASSPORT_NATIVE_SHORT,k);}
static void tick(unsigned elapsed){now+=elapsed;key(PASSPORT_NATIVE_TICK,(int)elapsed);}
static void reset(void){assert(!passport_app_entry(&host.hid.core.base,&app.base));}
int main(void){
 passport_native_host_v2 old={.base={.abi=2,.size=sizeof(old)}};assert(passport_app_entry(&old.base,&app.base)<0);
 reset();assert(low_rate);online=0;tick(1000);assert(!plays&&strstr(ui,"Wi-Fi"));online=1;tick(1000);assert(plays==1&&strstr(last_url,"/339/")&&strstr(last_url,"24k.mp3"));
 media=2;tick(1000);assert(history_count==1&&history[0]==0&&writes_seen==1);
 click(2);assert(!desired&&media==0);click(2);tick(1000);assert(plays==2);
 key(PASSPORT_NATIVE_LONG,0);assert(vol==50);key(PASSPORT_NATIVE_LONG,1);assert(vol==45);
 unsigned before=station;key(PASSPORT_NATIVE_DOUBLE,0);assert(station==before);
 click(1);assert(station==1);tick(1000);assert(strstr(last_url,"/332/"));
 media=4;tick(4000);media=4;tick(4000);media=4;tick(4000);before=plays;tick(1000);assert(attempts==3&&plays==before);click(2);tick(1000);assert(plays==before+1);
 key(PASSPORT_NATIVE_LONG,2);assert(page==MENU);selected=0;click(2);assert(page==CITIES);selected=4;click(2);assert(page==STATIONS);click(1);click(2);assert(station==9);
 key(PASSPORT_NATIVE_LONG,2);selected=1;click(2);assert(page==SEARCH);strcpy(query,"GZ");query_size=2;letter=26;click(2);assert(page==RESULTS&&match_count==1&&matches[0]==2);click(2);assert(page==STATIONS&&city==2);
 key(PASSPORT_NATIVE_LONG,2);assert(page==CITIES);key(PASSPORT_NATIVE_LONG,2);assert(page==PLAYER);
 key(PASSPORT_NATIVE_LONG,2);selected=3;click(2);assert(sleep_minutes==15);tick(900001);assert(!desired&&!sleep_minutes);assert(page==CLOCK);click(0);assert(page==PLAYER);
 key(PASSPORT_NATIVE_LONG,2);selected=4;click(2);assert(style==1&&page==PLAYER);
 for(unsigned i=0;i<14;i++)radio_remember(history,&history_count,i);assert(history_count==10&&history[0]==13&&history[9]==4);
 radio_remember(history,&history_count,8);assert(history_count==10&&history[0]==8);save();unsigned expected[10];memcpy(expected,history,sizeof(expected));reset();assert(!memcmp(expected,history,sizeof(expected))&&style==1);
 key(PASSPORT_NATIVE_LONG,2);selected=5;click(2);assert(page==CLOCK);click(0);assert(page==PLAYER);
 strcpy(saved,"1,999,0,0");reset();assert(station==0&&history_count==0);
 light=1;reset();assert(strstr(ui,"CITY RADIO"));
 key(PASSPORT_NATIVE_LONG,2);selected=6;click(2);assert(!low_rate && !strcmp(quality_saved,"standard"));
 tick(1000);assert(strstr(last_url,"64k.mp3"));reset();assert(!low_rate);
 key(PASSPORT_NATIVE_LONG,2);selected=6;click(2);assert(low_rate && !strcmp(quality_saved,"smooth"));app.base.stop();assert(!desired&&stops);
 puts("City Radio PASS ABI/offline/restore/retry/pause/volume/navigation/search/history/timer/style/clock/exit/en-zh");
}
