#include "passport_native_media.h"
#include "catalog.h"
#include <stddef.h>
static const passport_native_host_media_v2 *H;
static const passport_native_host_v2 *C;
static const passport_native_host_v1 *B;
enum {PLAYER,MENU,CITIES,STATIONS,HISTORY,SEARCH,RESULTS,CLOCK};
static unsigned page,selected,station,city,style,sleep_minutes,history[RADIO_HISTORY],history_count;
static unsigned matches[RADIO_CITIES],match_count,letter,query_size;
static unsigned last_state,last_second,attempts,phase;
static uint32_t sleep_due,retry_due,last_input;
static int zh,desired,waiting,remembered,low_rate;
static char query[4],screen[512],url[100];
static unsigned used;
static const char *tr(const char *en,const char *cn){return zh?cn:en;}
static void add(const char *p){while(*p && used+1<sizeof(screen))screen[used++]=*p++;screen[used]=0;}
static void number(unsigned v){char a[11];unsigned n=0;do{a[n++]=(char)('0'+v%10);v/=10;}while(v);while(n){char t[2]={a[--n],0};add(t);}}
static void two(unsigned v){if(v<10)add("0");number(v);}
static void clock_text(void) {
 uint32_t t=H->wall_time();if(!t){add("--:--:--");return;}t=(t+28800)%86400;
 two(t/3600);add(":");two(t/60%60);add(":");two(t%60);
}
static void clock_date(void) {
 uint32_t t=H->wall_time();if(!t){add(tr("Waiting for system time","等待系统校时"));return;}
 unsigned days=(t+28800)/86400,week=(days+4)%7,year=1970;
 for(;;){unsigned n=365+(year%4==0 && (year%100!=0 || year%400==0));if(days<n)break;days-=n;year++;}
 unsigned month=0;static const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
 while(month<11){unsigned n=lengths[month]+(month==1 && year%4==0 && (year%100!=0 || year%400==0));if(days<n)break;days-=n;month++;}
 number(year);add("-");two(month+1);add("-");two(days+1);
 static const char *weekdays[]={" Sun"," Mon"," Tue"," Wed"," Thu"," Fri"," Sat"};add(weekdays[week]);
}
static const char *city_name(unsigned c){return zh?radio_cities[c].zh:radio_cities[c].en;}
static const char *station_name(unsigned s){return zh?radio_stations[s].zh:radio_stations[s].en;}
static void save(void) {
 char out[100];unsigned n=0; /* Numeric versioned format; one bounded storage write. */
 unsigned fields[13]={1,station,style};for(unsigned i=0;i<10;i++)fields[i+3]=i<history_count?history[i]+1:0;
 for(unsigned i=0;i<13;i++){
  unsigned v=fields[i],k=0;char a[11];do{a[k++]=(char)('0'+v%10);v/=10;}while(v);
  while(k)out[n++]=a[--k];out[n++]=i==12?0:',';
 }
 C->storage_set("radio",out);
}
static void restore(void) {
 char in[100];if(!C->storage_get("radio",in,sizeof(in)))return;
 unsigned fields[13]={0},i=0,v=0,digits=0;
 for(unsigned k=0;k<sizeof(in);k++){
  char c=in[k];if(c>='0' && c<='9'){if(++digits>3)return;v=v*10+(unsigned)(c-'0');}
  else if(c==',' || !c){if(!digits || i>=13)return;fields[i++]=v;v=digits=0;if(!c)break;}
  else return;
 }
 if(i!=13 || fields[0]!=1 || fields[1]>=RADIO_STATIONS || fields[2]>1)return;
 for(unsigned j=3;j<13;j++)if(fields[j]>RADIO_STATIONS)return;
 station=fields[1];style=fields[2];
 for(unsigned j=13;j>3;){j--;if(fields[j])radio_remember(history,&history_count,fields[j]-1);}
}
static void select_station(unsigned s) {
 if(s>=RADIO_STATIONS)return;
 station=s;city=radio_stations[s].city;desired=1;waiting=1;remembered=0;attempts=0;retry_due=B->millis();
 H->media_stop();page=PLAYER;
}
static void make_url(void){
 const char *start="http://lhttp.qingting.fm/live/";unsigned n=0;while(*start)url[n++]=*start++;
 unsigned v=radio_stations[station].source,k=0;char a[11];do{a[k++]=(char)('0'+v%10);v/=10;}while(v);
 while(k)url[n++]=a[--k];const char *tail=low_rate?"/24k.mp3":"/64k.mp3";while(*tail)url[n++]=*tail++;url[n]=0;
}
static void digit(unsigned d,int x,int y) {
 static const unsigned bits[]={63,6,91,79,102,109,125,7,127,111};
 static const int r[7][4]={{4,0,24,4},{28,4,4,28},{28,36,4,28},{4,64,24,4},{0,36,4,28},{0,4,4,28},{4,32,24,4}};
 for(unsigned i=0;i<7;i++)if(bits[d]&(1u<<i))B->rect(x+r[i][0],y+r[i][1],r[i][2],r[i][3],0x62d8df);
}
static void draw(void) {
 used=0;screen[0]=0;unsigned state=H->media_state();
 B->clear(style?0x082e58:0x111923);
 if(page==PLAYER){
  add(tr("CITY RADIO","城市电台"));add("   ");int battery=H->battery_percent();if(battery>=0){number((unsigned)battery);add("%");}add("\n");
  add(city_name(radio_stations[station].city));add(" / ");number(station%2+1);add(" / 2\n");add(station_name(station));add("\n");
  if(!desired)add(tr("Paused","已暂停"));
  else if(!C->network_ready())add(tr("Connect Wi-Fi in Settings","请在系统设置连接 Wi-Fi"));
  else if(state==2)add(tr("Playing","正在播放"));
  else if(attempts>=3 && state!=1 && state!=3)add(tr("Stream failed; OK retry","台源不可用，OK 重试"));
  else add(tr("Buffering...","正在缓冲..."));
  add(tr("  Vol ","  音量 "));number((unsigned)H->media_volume(-1));add("%\n");clock_text();
  if(sleep_minutes){add("  ");unsigned left=(int32_t)(sleep_due-B->millis())>0?(sleep_due-B->millis())/60000+1:0;number(left);add(tr(" min"," 分钟"));}
  add(tr("\nUP/DOWN tune | hold: volume\nOK pause | hold OK: menu","\n上下换台，长按调音量\nOK 暂停，长按 OK 菜单"));
  if(style){
   B->rect(12,168,216,81,0x226e9c);B->rect(20,176,200,45,0x071d30);
   for(unsigned side=0;side<2;side++){
    int x=side?163:43;B->rect(x,183,31,31,0x8bb5ba);B->rect(x+8,191,15,15,0x123c5a);
    if(state==2 && phase%2)B->rect(x+2,196,27,4,0xe9d7a3);else B->rect(x+14,184,4,29,0xe9d7a3);
   }B->rect(74,196,89,4,0xe9d7a3);
   for(unsigned i=0;i<14;i++)B->rect(27+i*14,230,8,5,0x082e58);
  }else{
   for(unsigned i=0;i<18;i++){int h=state==2?8+(int)((i*13+phase*7)%40):4;B->rect(18+i*11,230-h,7,h,i%3==0?0xe9bd62:i%3==1?0x54cdbb:0x6397f1);}
   B->rect(14,243,212,2,0x406070);B->rect(18+(int)(station*14),238,4,13,0xf3c86a);
  }
 }else if(page==CLOCK){
  add(tr("CITY RADIO / CLOCK\n","城市电台 / 时钟\n"));clock_date();add(tr("\nAny key: return","\n任意键返回"));
  uint32_t t=H->wall_time();if(t){unsigned m=(t+28800)/60%1440;digit(m/600,27,105);digit(m/60%10,67,105);digit(m%60/10,137,105);digit(m%10,177,105);B->rect(116,125,4,4,0x62d8df);B->rect(116,150,4,4,0x62d8df);}
 }else if(page==SEARCH){
  add(tr("CITY INITIALS\n","城市拼音首字母\n"));add(query);add("_\n\n> ");
  if(letter<26){char a[2]={(char)('A'+letter),0};add(a);}else add(letter==26?tr("Search","搜索"):tr("Delete","删除"));
  add(tr("\n\nUP/DOWN choose\nOK add / search / delete\nHold OK: back","\n\n上下选择字母或操作\nOK 输入/搜索/删除\n长按 OK 返回"));
 }else{
  add(tr("CITY RADIO\n\n","城市电台\n\n"));
  unsigned count=page==MENU?7:page==CITIES?RADIO_CITIES:page==STATIONS?2:page==HISTORY?history_count:match_count;
  if(!count)add(tr("No results\n","暂无记录或结果\n"));
  unsigned first=selected/5*5,end=first+5;if(end>count)end=count;
  for(unsigned i=first;i<end;i++){
   add(i==selected?"> ":"  ");
   if(page==MENU){static const char *en[]={"Cities","Search city","Recent stations","Sleep timer","Switch style","Clock","Quality"};static const char *cn[]={"城市选台","搜索城市","历史电台","定时停止","切换风格","时钟","音质"};add(zh?cn[i]:en[i]);if(i==6)add(low_rate?tr(" smooth"," 流畅"):tr(" standard"," 标准"));if(i==3){add(" ");number(sleep_minutes);add(tr("m","分"));}}
   else if(page==CITIES)add(city_name(i));
   else if(page==RESULTS)add(city_name(matches[i]));
   else if(page==STATIONS)add(station_name(city*2+i));
   else add(station_name(history[i]));add("\n");
  }
  add(tr("\nUP/DOWN select | OK open\nHold OK: back","\n上下选择，OK 打开\n长按 OK 返回"));
 }
 B->text(screen,0xeaf4fb);
}
static void event(uint32_t e,int32_t key) {
 uint32_t now=B->millis();
 if(e==PASSPORT_NATIVE_TICK){
  unsigned state=H->media_state();
  if(sleep_minutes && (int32_t)(now-sleep_due)>=0){sleep_minutes=0;desired=waiting=0;H->media_stop();}
  if(desired && C->network_ready()){
   if((waiting || state==4 || state==5) && state!=1 && state!=2 && state!=3 && attempts<3 && (int32_t)(now-retry_due)>=0){
    make_url();attempts++;retry_due=now+3000;
    if(H->media_play(url))waiting=0;
   }
   if(state==2 && !remembered){radio_remember(history,&history_count,station);save();remembered=1;}
  }
  if(page==PLAYER && (uint32_t)(now-last_input)>=30000)page=CLOCK;
  unsigned second=now/1000;
  if(last_second!=second || last_state!=state){phase++;last_second=second;last_state=state;draw();}return;
 }
 if(key<0 || key>2 || (e!=PASSPORT_NATIVE_SHORT && e!=PASSPORT_NATIVE_LONG))return;
 last_input=now;
 if(page==CLOCK){page=PLAYER;draw();return;}
 if(e==PASSPORT_NATIVE_LONG){
  if(key==2){if(page==PLAYER){page=MENU;selected=0;}else if(page==STATIONS){page=CITIES;selected=city;}else if(page==RESULTS){page=SEARCH;selected=0;}else{page=PLAYER;}draw();}
  else if(page==PLAYER){int v=H->media_volume(-1)+(key==0?5:-5);H->media_volume(v<0?0:v>100?100:v);draw();}return;
 }
 if(page==PLAYER){
  if(key<2){unsigned base=radio_stations[station].city*2;select_station(base+((station%2)^1u));}
  else if(desired && !(attempts>=3 && H->media_state()!=1 && H->media_state()!=2)){desired=waiting=0;H->media_stop();}
  else {desired=waiting=1;attempts=0;retry_due=now;remembered=0;}draw();return;
 }
 if(page==SEARCH){
  if(key<2)letter=(letter+(key==0?27:1))%28;
  else if(letter<26 && query_size<3){query[query_size++]=(char)('A'+letter);query[query_size]=0;letter=26;}
  else if(letter==27){if(query_size)query[--query_size]=0;}
  else if(letter==26){match_count=radio_city_matches(query,matches);page=RESULTS;selected=0;}
  draw();return;
 }
 unsigned count=page==MENU?7:page==CITIES?RADIO_CITIES:page==STATIONS?2:page==HISTORY?history_count:match_count;
 if(key<2){if(count)selected=(selected+(key==0?count-1:1))%count;}
 else if(count){
  if(page==MENU){unsigned item=selected;selected=0;
   if(item==0)page=CITIES;
   else if(item==1){page=SEARCH;letter=query_size=0;query[0]=0;}
   else if(item==2)page=HISTORY;
   else if(item==3){sleep_minutes=sleep_minutes==0?15:sleep_minutes==15?30:sleep_minutes==30?60:0;sleep_due=now+sleep_minutes*60000;page=PLAYER;}
   else if(item==4){style^=1;save();page=PLAYER;}
   else if(item==5)page=CLOCK;
   else {low_rate^=1;C->storage_set("quality",low_rate?"smooth":"standard");if(desired)select_station(station);else page=PLAYER;}
  }else if(page==CITIES || page==RESULTS){city=page==CITIES?selected:matches[selected];page=STATIONS;selected=0;}
  else if(page==STATIONS)select_station(city*2+selected);
  else if(page==HISTORY)select_station(history[selected]);
 }
 draw();
}
static void stop(void){desired=waiting=0;H->media_stop();}
int passport_app_entry(const passport_native_host_v1 *host,passport_native_app_v1 *app) {
 if(!host || !app || host->abi!=2 || host->size<sizeof(passport_native_host_media_v2))return -1;
 H=(const void *)host;C=&H->hid.core;B=host;
 if(!H->media_play || !H->media_stop || !H->media_state || !H->media_volume || !H->wall_time || !H->battery_percent || !C->storage_get || !C->storage_set || !C->network_ready)return -1;
 zh=B->language()[0]=='z';low_rate=1;page=PLAYER;selected=station=city=style=sleep_minutes=history_count=query_size=letter=phase=attempts=0;
 query[0]=0;sleep_due=0;last_second=last_state=~0u;desired=waiting=1;remembered=0;retry_due=last_input=B->millis();restore();city=radio_stations[station].city;
 char quality[16]={0};if(C->storage_get("quality",quality,sizeof(quality)) && quality[0]=='s' && quality[1]=='t' && quality[2]=='a' && quality[3]=='n' && quality[4]=='d' && quality[5]=='a' && quality[6]=='r' && quality[7]=='d' && !quality[8])low_rate=0;
 passport_native_app_v2 *out=(void *)app;*out=(passport_native_app_v2){{2,sizeof(*out),event,stop},0};draw();return 0;
}
