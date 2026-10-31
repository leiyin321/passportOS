#include "native.h"
#include "native_elf.h"
#include "passport_native_v2.h"
#include "passport_native_media.h"
#include "media.h"
#include "volume.h"
#include "bsp_battery.h"
#include <time.h>
#include "hid.h"
#include "native_json.h"
#include "network.h"
#include "wifi.h"
#include "audio.h"
#include "store.h"
#include "native_exec_pool.h"
#include "esp_elf.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static esp_elf_t image;
static passport_native_app_v2 callbacks;
static const ps_package *package;
static uint32_t generation;
static unsigned abi, writes;
static bool audio_allowed;
static ps_view *view;
static const ps_display_layout *display;
static unsigned permissions;
static const char *language, *mode;
static bool started;
static uint32_t load_span;
static int text(const char *s, uint32_t color) {
    if (!(permissions & PS_UI) || !s || color>0xffffff || strnlen(s,sizeof(view->text))>=sizeof(view->text)) return -1;
    snprintf(view->text,sizeof(view->text),"%s",s); view->text_color=color; return 0;
}
static int rect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t color) {
    if (!(permissions & PS_GRAPHICS) || color>0xffffff || x<0 || y<0 || w<=0 || h<=0 ||
        x>=display->viewport.width || y>=display->viewport.height || view->count==PS_DRAW_MAX) return -1;
    if (w>display->viewport.width-x) w=display->viewport.width-x;
    if (h>display->viewport.height-y) h=display->viewport.height-y;
    view->commands[view->count++]=(ps_draw){x,y,w,h,color}; return 0;
}
static int clear(uint32_t background) {
    if (!(permissions & PS_GRAPHICS) || background>0xffffff) return -1;
    view->count=0; view->image_count=0; view->text[0]=0;
    return rect(0,0,display->viewport.width,display->viewport.height,background);
}
static uint32_t millis(void) { return permissions & PS_TIMER ? (uint32_t)(esp_timer_get_time()/1000) : 0; }
static const char *get_language(void) { return language; }
static const char *get_mode(void) { return mode; }
static bool permitted(unsigned permission) { return package && (permissions&permission)==permission; }
static int network_ready(void) { return permitted(PS_FETCH)||permitted(PS_WS) ? ps_wifi_online():0; }
static int http_request(const char *method,const char *url,const char *headers,const void *body,uint32_t bytes) {
    return permitted(PS_FETCH) && method && url && bytes<=2048 && ps_net_request(package,method,url,headers,headers?strnlen(headers,1025):0,body,bytes,generation);
}
static int ws_open(const char *url,const char *headers) {
    return permitted(PS_WS) && url && ps_net_ws_open(package,url,headers,headers?strnlen(headers,1025):0,generation);
}
static int ws_send(const void *data,uint32_t bytes,int binary) { return permitted(PS_WS) && data && bytes<=2048 && ps_net_ws_send(data,bytes,binary!=0); }
static int ws_connected(void) { return permitted(PS_WS) && ps_net_ws_connected(); }
static void ws_close(void) { if (permitted(PS_WS)) ps_net_ws_close(); }
static int identity(char *device,uint32_t dc,char *client,uint32_t cc) { return ps_runtime_identity(package,device,dc,client,cc); }
static int storage_get(const char *key,char *out,uint32_t capacity) { return permitted(PS_STORAGE) && key && out && capacity && capacity<=4097 && ps_data_get(package,key,out,capacity); }
static int storage_set(const char *key,const char *value) { return permitted(PS_STORAGE) && ++writes<=4 && key && value && ps_data_set(package,key,value); }
static int audio_mode(int capture) {
    if (!permitted(PS_AUDIO_STREAM) || !audio_allowed || capture < -1 || capture>1) return 0;
    if (capture>=0 && !ps_audio_stream_half_duplex(true)) return 0;
    view->microphone=capture==1; view->audio_stream=capture>=0;
    if (capture!=1) (void)ps_audio_stream_stop();
    return 1;
}
static int audio_play(const void *data,uint32_t bytes) { return permitted(PS_AUDIO_STREAM) && audio_allowed && !view->microphone && data && bytes<=1024 && ps_audio_stream_play_opus_wait(data,bytes,80); }
static const passport_native_host_v1 host1={1,sizeof(host1),text,clear,rect,millis,get_language,get_mode};
static int hid_start(uint32_t flags) {return permitted(PS_BLE_HID) && ps_hid_start(flags);}
static uint32_t hid_state(void) {return permitted(PS_BLE_HID)?ps_hid_state():PS_HID_UNAVAILABLE;}
static int hid_report(uint32_t id,const void *data,uint32_t bytes) {return permitted(PS_BLE_HID) && ps_hid_report(id,data,bytes);}
static void hid_stop(void) {if(permitted(PS_BLE_HID))ps_hid_stop();}
static int media_play(const char *url) {
    return permitted(PS_AUDIO_PLAYBACK|PS_FETCH) &&
           !(permissions&(PS_AUDIO|PS_AUDIO_STREAM)) && url && ps_media_play(package,url,generation);
}
static void media_stop(void) {if(permitted(PS_AUDIO_PLAYBACK))ps_media_stop();}
static uint32_t media_state(void) {return permitted(PS_AUDIO_PLAYBACK)?ps_media_state():4;}
static int media_volume(int percent) {
    if(!permitted(PS_AUDIO_PLAYBACK) || percent < -1 || percent>100)return -1;
    if(percent>=0)ps_audio_volume_set(false,(unsigned)percent);
    return (int)ps_audio_volume(false);
}
static uint32_t wall_time(void) {time_t t=time(NULL);return t>=1704067200 && t<=UINT32_MAX?(uint32_t)t:0;}
static int battery_percent(void){return bsp_battery_soc();}
static const passport_native_host_media_v2 host2={
 .hid={.core={{2,sizeof(host2),text,clear,rect,millis,get_language,get_mode},
      network_ready,http_request,ws_open,ws_send,ws_connected,ws_close,
      ps_native_json_get,identity,storage_get,storage_set,audio_mode,audio_play},
    .hid_start=hid_start,.hid_state=hid_state,.hid_report=hid_report,.hid_stop=hid_stop},
 .media_play=media_play,.media_stop=media_stop,.media_state=media_state,
 .media_volume=media_volume,.wall_time=wall_time,.battery_percent=battery_percent
};
_Static_assert(sizeof(passport_native_host_media_v2)==120,"Native media ABI layout");

static bool function_valid(const void *fn, const void *payload, size_t bytes) {
    if (!fn) return true;
    uintptr_t address=(uintptr_t)fn, base=(uintptr_t)image.psegment;
    return address>=base && address-base<load_span && ps_native_elf_executable(payload,bytes,address-base);
}
bool ps_native_start(ps_package *p, ps_view *v, const ps_display_layout *d,
                     const char *lang, const char *theme, uint32_t gen, char *error, size_t capacity) {
    ps_native_stop();
    const cJSON *limits=cJSON_GetObjectItemCaseSensitive(p->manifest,"limits");
    unsigned budget=cJSON_GetObjectItemCaseSensitive(limits,"native_exec_kib")->valueint*1024u;
    if (!ps_native_elf_validate(p->code,p->code_size,budget,&load_span) ||
        load_span>native_exec_capacity() || native_exec_verify()!=0) {
        snprintf(error,capacity,"%s",lang[0]=='z'?"ELF 应用不可用":"Native ELF/target/arena invalid"); return false;
    }
    view=v; display=d; language=lang; mode=theme; permissions=p->permissions;
    package=p; generation=gen; audio_allowed=false; writes=0;
    abi=cJSON_GetObjectItemCaseSensitive(p->manifest,"abi")->valueint;
    int rc=esp_elf_init(&image);
    if (!rc) rc=esp_elf_relocate(&image,(const void *)p->code);
    if (!rc) {
        passport_native_entry_v1 entry=(void *)image.entry;
        callbacks=(passport_native_app_v2){0};
        rc=entry(abi==1?&host1:&host2.hid.core.base,&callbacks.base);
        if (!rc && (callbacks.base.abi!=abi || callbacks.base.size!=(abi==1?sizeof(callbacks.base):sizeof(callbacks)) ||
            !function_valid((const void *)callbacks.base.event,p->code,p->code_size) ||
            !function_valid((const void *)callbacks.base.stop,p->code,p->code_size) ||
            (abi==2 && !function_valid((const void *)callbacks.data,p->code,p->code_size)))) rc=-1;
    }
    free(p->code); p->code=NULL; p->code_size=0;
    if (rc) { snprintf(error,capacity,lang[0]=='z'?"ELF ABI 错误 (%d)":"Native load/ABI failed (%d)",rc); ps_native_stop(); return false; }
    started=true;
    ESP_LOGI("passport_native","started pool_used=%u capacity=%u",(unsigned)native_exec_used(),(unsigned)native_exec_capacity());
    return true;
}
bool ps_native_event(const char *callback,int value) {
    if (!started) return false;
    static const char *names[]={"on_button_down","on_button_up","on_button_short","on_button_double","on_button_long","on_tick","on_permission","on_audio_error"};
    for (unsigned i=0;i<8;i++) if (!strcmp(callback,names[i])) {
        writes=0;
        if (callbacks.base.event) callbacks.base.event(i+1,value);
        return true;
    }
    return true;
}
void ps_native_stop(void) {
    if (started && callbacks.base.stop) callbacks.base.stop();
    if(permissions&PS_BLE_HID)ps_hid_stop();
    if(permissions&PS_AUDIO_PLAYBACK && !ps_media_close())
      ESP_LOGE("passport_native","media teardown pending; audio ownership retained");
    permissions=0;
    started=false; callbacks=(passport_native_app_v2){0};
    if (image.psegment) esp_elf_deinit(&image);
    image=(esp_elf_t){0}; view=NULL; display=NULL; package=NULL; audio_allowed=false;
}

void ps_native_audio_allowed(bool allowed) {
    audio_allowed=allowed;
    if (!allowed && view) { view->microphone=false; view->audio_stream=false; }
}
void ps_native_data(unsigned kind,int status,const void *data,size_t bytes) {
    if (!started || abi!=2 || !callbacks.data || bytes>16384 || (bytes && !data)) return;
    if ((kind==1 && !(permissions&(PS_FETCH|PS_WS))) ||
        ((kind==2 || kind==3) && !(permissions&PS_WS)) ||
        (kind==4 && (!(permissions&PS_AUDIO_STREAM) || !audio_allowed))) return;
    writes=0; callbacks.data(kind,status,data,(uint32_t)bytes);
}
