#include "wifi.h"
#include "wifi_setup_form.h"
#include "wifi_profiles.h"
#include "portal_flow.h"
#include "portal_dns_server.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>
static EventGroupHandle_t flags;
static esp_netif_t *setup_ap_netif;
static httpd_handle_t setup_httpd;
static atomic_bool setup_active,setup_running,setup_deferred,setup_retry_reset;
static atomic_uint setup_saved_ms,setup_generation,setup_processed_generation;
static atomic_uint setup_peer_mask;
static atomic_bool setup_peer_has_ip;
static ps_portal_flow setup_flow;
static char setup_target[33];


/* The worker owns connect/roam and NVS writes. Event callbacks only set bits. */
static ps_wifi_profiles profiles={.version=1};
static SemaphoreHandle_t profile_lock,radio_lock;
static QueueHandle_t wifi_commands;
static atomic_int profile_status;
typedef struct { bool forget; uint32_t generation; ps_wifi_profile item; } wifi_command;
static bool persist_profiles(const ps_wifi_profiles *next) {
  nvs_handle_t h;
  if(nvs_open_from_partition("appdata","wifi_profiles",NVS_READWRITE,&h)!=ESP_OK)return false;
  bool ok=nvs_set_blob(h,"saved",next,sizeof(*next))==ESP_OK && nvs_commit(h)==ESP_OK;
  nvs_close(h);return ok;
}
static void load_profiles(void) {
  nvs_handle_t h;size_t n=sizeof(profiles);
  esp_err_t result=nvs_open_from_partition("appdata","wifi_profiles",NVS_READONLY,&h);
  if(result==ESP_OK){result=nvs_get_blob(h,"saved",&profiles,&n);nvs_close(h);}
  if(result==ESP_ERR_NVS_NOT_FOUND) {
    wifi_config_t old={0};
    if(esp_wifi_get_config(WIFI_IF_STA,&old)==ESP_OK) {
      char ssid[33]={0},password[65]={0};
      memcpy(ssid,old.sta.ssid,32);memcpy(password,old.sta.password,64);
      (void)ps_wifi_profiles_save(&profiles,ssid,password);
      memset(password,0,sizeof(password));
    }
    memset(&old,0,sizeof(old));
    if(!persist_profiles(&profiles))atomic_store(&profile_status,2);
  } else if(result!=ESP_OK || n!=sizeof(profiles) || !ps_wifi_profiles_valid(&profiles)) {
    memset(&profiles,0,sizeof(profiles));profiles.version=1;
    atomic_store(&profile_status,2); /* Never re-import a deliberately empty list. */
  }
}
static int pick_network(uint8_t tried) {
  wifi_scan_config_t scan={.show_hidden=false,.scan_time.active={.min=40,.max=120}};
  int8_t strength[PS_WIFI_PROFILES];memset(strength,-128,sizeof(strength));
  if(esp_wifi_scan_start(&scan,true)==ESP_OK) {
    uint16_t count=24;wifi_ap_record_t *ap=calloc(count,sizeof(*ap));
    if(ap && esp_wifi_scan_get_ap_records(&count,ap)==ESP_OK)for(unsigned i=0;i<count;i++) {
      ap[i].ssid[32]=0;int at=ps_wifi_profiles_find(&profiles,(char*)ap[i].ssid);
      if(at>=0 && ap[i].rssi>strength[at])strength[at]=ap[i].rssi;
    }
    free(ap);esp_wifi_clear_ap_list();
  }
  int choice=ps_wifi_profiles_pick(&profiles,strength,tried);
  /* Directed connection also supports known hidden SSIDs. */
  if(choice<0)for(unsigned i=0;i<profiles.count;i++)if(!(tried&(1u<<i))){choice=(int)i;break;}
  return choice;
}
static void setup_verify(void) {
  if(!atomic_load(&setup_running) || atomic_load(&setup_generation)!=atomic_load(&setup_processed_generation))return;
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  if(setup_flow.state==PS_PORTAL_CONNECTING || setup_flow.state==PS_PORTAL_CONNECTED) {
    int status=atomic_load(&profile_status);
    if(status==1 || status==2 || status==4)ps_portal_fail(&setup_flow);
    else {
      wifi_ap_record_t actual={0};esp_netif_ip_info_t ip={0};
      esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
      bool exact=status==0 && ps_wifi_online() && sta &&
        esp_wifi_sta_get_ap_info(&actual)==ESP_OK && esp_netif_get_ip_info(sta,&ip)==ESP_OK && ip.ip.addr;
      actual.ssid[32]=0;
      exact=exact && setup_target[0] && !strcmp(setup_target,(char*)actual.ssid);
      ps_portal_observe(&setup_flow,exact,(uint32_t)(esp_timer_get_time()/1000));
    }
  }
  xSemaphoreGive(profile_lock);
}
static void wifi_worker(void *unused) {
  (void)unused;
  char connecting[33]={0},desired[33]={0};uint8_t tried=0;TickType_t deadline=0;
  bool was_online=false,first=true;
  for(;;) {
    wifi_command command={0};bool have=xQueueReceive(wifi_commands,&command,pdMS_TO_TICKS(500))==pdTRUE;
    xSemaphoreTake(radio_lock,portMAX_DELAY);
    if(atomic_exchange(&setup_retry_reset,false) && !ps_wifi_online()){deadline=0;tried=0;first=true;desired[0]=0;}
    int requested=-1;
    if(have) {
      xSemaphoreTake(profile_lock,portMAX_DELAY);
      ps_wifi_profiles next=profiles;
      bool changed=command.forget?ps_wifi_profiles_forget(&next,command.item.ssid):
        ps_wifi_profiles_save(&next,command.item.ssid,command.item.password);
      if(changed && persist_profiles(&next)) {
        if(!command.forget && command.generation==atomic_load(&setup_generation) && !strcmp(command.item.ssid,setup_target))
          atomic_store(&setup_saved_ms,((uint32_t)(esp_timer_get_time()/1000))|1u);
        profiles=next;atomic_store(&profile_status,command.forget?0:3);tried=0;
        if(!command.forget){requested=ps_wifi_profiles_find(&profiles,command.item.ssid);strcpy(desired,command.item.ssid);}
        else if(!strcmp(desired,command.item.ssid))desired[0]=0;
        if(requested>=0 || !strcmp(connecting,command.item.ssid)) {
          esp_wifi_disconnect();xEventGroupClearBits(flags,1);deadline=0;was_online=false;
        }
      } else atomic_store(&profile_status,changed?2:1);
      atomic_store(&setup_processed_generation,command.generation);
      memset(&next,0,sizeof(next));
      xSemaphoreGive(profile_lock);
    }
    memset(&command,0,sizeof(command));
    bool online=ps_wifi_online();TickType_t now=xTaskGetTickCount();
    int hotspot_choice=-1;
    if(atomic_load(&setup_running) && atomic_load(&setup_deferred)) {
      xSemaphoreTake(profile_lock,portMAX_DELAY);
      if(setup_flow.state==PS_PORTAL_CONNECTING)hotspot_choice=ps_wifi_profiles_find(&profiles,setup_target);
      xSemaphoreGive(profile_lock);
    }
    if(online) {
      if(!was_online && connecting[0]) {
        xSemaphoreTake(profile_lock,portMAX_DELAY);
        ps_wifi_profiles next=profiles;
        if(ps_wifi_profiles_success(&next,connecting) && persist_profiles(&next))profiles=next;
        memset(&next,0,sizeof(next));xSemaphoreGive(profile_lock);
      }
      if(atomic_load(&profile_status)==3)atomic_store(&profile_status,desired[0] && strcmp(desired,connecting)?4:0);
      was_online=true;tried=0;deadline=0;
    } else {
      if(was_online){deadline=0;was_online=false;first=true;}
      if(requested>=0 || !deadline || (int32_t)(now-deadline)>=0) {
        esp_wifi_disconnect();
        int choice=requested>=0?requested:hotspot_choice>=0?hotspot_choice:(first && profiles.count?0:profiles.count?pick_network(tried):-1);first=false;
        if(choice>=0) {
          wifi_config_t cfg={0};
          memcpy(cfg.sta.ssid,profiles.items[choice].ssid,strlen(profiles.items[choice].ssid));
          memcpy(cfg.sta.password,profiles.items[choice].password,strlen(profiles.items[choice].password));
          cfg.sta.pmf_cfg.capable=true;
          cfg.sta.scan_method=WIFI_ALL_CHANNEL_SCAN;
          cfg.sta.sort_method=WIFI_CONNECT_AP_BY_SIGNAL;cfg.sta.failure_retry_cnt=2;
          strcpy(connecting,profiles.items[choice].ssid);
          tried|=1u<<choice;
          if(esp_wifi_set_config(WIFI_IF_STA,&cfg)==ESP_OK)esp_wifi_connect();
          memset(&cfg,0,sizeof(cfg));deadline=now+pdMS_TO_TICKS(12000);
        } else {tried=0;deadline=now+pdMS_TO_TICKS(30000);connecting[0]=0;}
      }
    }
    xSemaphoreGive(radio_lock);
    setup_verify();
  }
}

static esp_err_t setup_page(httpd_req_t *request) {
  static const unsigned char html[] = {
#include "wifi_setup_page.inc"
  };
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request,"Content-Encoding","gzip");
  return httpd_resp_send(request,(const char*)html,sizeof(html));
}

/* Runs on the HTTP server task, never a button/LVGL callback. The driver
 * owns scan results until get_ap_records/clear_ap_list releases them. */
static esp_err_t setup_scan(httpd_req_t *request) {
  httpd_resp_set_type(request,"application/json");
  httpd_resp_set_hdr(request,"Cache-Control","no-store");
  wifi_scan_config_t config={.show_hidden=false,
    .scan_time.active={.min=40,.max=120}};
  if(xSemaphoreTake(radio_lock,pdMS_TO_TICKS(1000))!=pdTRUE)
    return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Busy");
  if(!ps_wifi_online())esp_wifi_disconnect();
  esp_err_t scan_result=esp_wifi_scan_start(&config,true);
  if(scan_result!=ESP_OK) {
    xSemaphoreGive(radio_lock);
    httpd_resp_set_status(request,"503 Service Unavailable");
    return httpd_resp_sendstr(request,"{\"error\":\"busy\"}");
  }
  uint16_t count=16;
  wifi_ap_record_t *records=calloc(count,sizeof(*records));
  if(!records){esp_wifi_clear_ap_list();xSemaphoreGive(radio_lock);return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");}
  esp_err_t status=esp_wifi_scan_get_ap_records(&count,records);
  esp_wifi_clear_ap_list();xSemaphoreGive(radio_lock);
  cJSON *list=cJSON_CreateArray();bool ok=status==ESP_OK && list;
  for(unsigned i=0;ok && i<count;i++) {
    records[i].ssid[32]=0;
    if(!records[i].ssid[0])continue;
    bool duplicate=false;
    for(unsigned j=0;j<i;j++)if(!strcmp((char*)records[j].ssid,(char*)records[i].ssid)){duplicate=true;break;}
    if(duplicate)continue;
    cJSON *item=cJSON_CreateObject();
    if(!item){ok=false;break;}
    ok=cJSON_AddStringToObject(item,"ssid",(char*)records[i].ssid) &&
       cJSON_AddNumberToObject(item,"rssi",records[i].rssi) &&
       cJSON_AddBoolToObject(item,"secure",records[i].authmode!=WIFI_AUTH_OPEN);
    if(!cJSON_AddItemToArray(list,item)){cJSON_Delete(item);ok=false;}
  }
  free(records);
  char *json=ok?cJSON_PrintUnformatted(list):NULL;cJSON_Delete(list);
  if(!json)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Scan unavailable");
  esp_err_t result=httpd_resp_sendstr(request,json);cJSON_free(json);return result;
}

/* Custom header forces cross-origin browser requests through an unapproved
 * CORS preflight. Ordinary cross-site HTML forms cannot mutate credentials. */
static bool setup_post(httpd_req_t *request) {
  char value[2];
  return httpd_req_get_hdr_value_str(request,"X-Passport-Setup",value,sizeof(value))==ESP_OK && !strcmp(value,"1");
}
static bool setup_submit(const char *ssid,const char *password,bool deferred) {
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  atomic_fetch_add(&setup_generation,1);
  snprintf(setup_target,sizeof(setup_target),"%s",ssid);ps_portal_submit(&setup_flow,(uint32_t)(esp_timer_get_time()/1000));
  if(deferred)setup_flow.timeout_ms=300000;
  atomic_store(&setup_deferred,deferred);atomic_store(&setup_saved_ms,0);
  bool accepted=ps_wifi_configure(ssid,password);
  if(!accepted){setup_flow=(ps_portal_flow){0};memset(setup_target,0,sizeof(setup_target));}
  xSemaphoreGive(profile_lock);return accepted;
}
static esp_err_t setup_save(httpd_req_t *request) {
  if(!setup_post(request))return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Forbidden");
  char body[513], ssid[33], password[64];
  if (request->content_len <= 0 || request->content_len > 512) {
    httpd_resp_set_status(request, "413 Payload Too Large");
    return httpd_resp_sendstr(request, "Wi-Fi form is too large. / 配网表单过大。");
  }
  int received = 0;
  while (received < request->content_len) {
    int n = httpd_req_recv(request, body + received,
                           request->content_len - received);
    if (n <= 0) break;
    received += n;
  }
  if (received != request->content_len ||
      !ps_wifi_setup_parse_form(body, (size_t)received, ssid, sizeof(ssid),
                                password, sizeof(password))) {
    memset(body, 0, sizeof(body));
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));
    httpd_resp_set_status(request, "400 Bad Request");
    return httpd_resp_sendstr(request, "Invalid Wi-Fi name or password. / Wi-Fi 名称或密码无效。");
  }
  bool accepted = setup_submit(ssid, password,!strcmp(request->uri,"/hotspot"));
  memset(ssid, 0, sizeof(ssid));
  memset(password, 0, sizeof(password));
  memset(body, 0, sizeof(body));
  if (!accepted) {
    httpd_resp_set_status(request, "400 Bad Request");
    return httpd_resp_sendstr(request, "Could not start the Wi-Fi connection. / 无法启动 Wi-Fi 连接。");
  }
  ESP_LOGI("passport", "Wi-Fi credentials received through local setup portal");
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  return httpd_resp_sendstr(request,
      "<!doctype html><meta name=viewport content='width=device-width'>"
      "<meta charset=utf-8><h2>Request received / 已提交</h2><p>Passport is connecting. You can close this page. / 正在连接，可关闭此页面。</p>");
}

static esp_err_t setup_saved(httpd_req_t *request) {
  cJSON *root=cJSON_CreateObject(),*list=cJSON_CreateArray();
  if(!root || !list){cJSON_Delete(root);cJSON_Delete(list);return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Memory");}
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  bool ok=true;
  for(unsigned i=0;i<profiles.count;i++) {
    cJSON *name=cJSON_CreateString(profiles.items[i].ssid);
    if(!name || !cJSON_AddItemToArray(list,name)){cJSON_Delete(name);ok=false;break;}
  }
  xSemaphoreGive(profile_lock);
  if(!cJSON_AddItemToObject(root,"networks",list)){cJSON_Delete(list);ok=false;}
  ok=ok && cJSON_AddNumberToObject(root,"status",atomic_load(&profile_status)) &&
    cJSON_AddNumberToObject(root,"capacity",PS_WIFI_PROFILES);
  char *json=ok?cJSON_PrintUnformatted(root):NULL;cJSON_Delete(root);
  if(!json)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Memory");
  httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
  esp_err_t result=httpd_resp_sendstr(request,json);cJSON_free(json);return result;
}
static esp_err_t setup_forget(httpd_req_t *request) {
  if(!setup_post(request))return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Forbidden");
  char body[513],password[64];wifi_command command={.forget=true};int used=0;
  bool ok=request->content_len>0 && request->content_len<=512;
  while(ok && used<request->content_len) {
    int n=httpd_req_recv(request,body+used,request->content_len-used);
    if(n<=0){ok=false;break;}used+=n;
  }
  ok=ok && ps_wifi_setup_parse_form(body,(size_t)used,command.item.ssid,sizeof(command.item.ssid),password,sizeof(password));
  if(ok) {
    xSemaphoreTake(profile_lock,portMAX_DELAY);
    setup_flow=(ps_portal_flow){0};memset(setup_target,0,sizeof(setup_target));
    atomic_store(&setup_deferred,false);atomic_store(&setup_saved_ms,0);
    xSemaphoreGive(profile_lock);
  }
  ok=ok && xQueueSend(wifi_commands,&command,0)==pdTRUE;
  memset(body,0,sizeof(body));memset(password,0,sizeof(password));memset(&command,0,sizeof(command));
  if(!ok)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid request / 请求无效");
  httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
  return httpd_resp_sendstr(request,"{}");
}

static esp_err_t setup_connect_saved(httpd_req_t *request) {
  if(!setup_post(request))return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Forbidden");
  char body[513],ssid[33],password[64];int used=0;
  bool ok=request->content_len>0 && request->content_len<=512;
  while(ok && used<request->content_len) {
    int n=httpd_req_recv(request,body+used,request->content_len-used);
    if(n<=0){ok=false;break;}used+=n;
  }
  ok=ok && ps_wifi_setup_parse_form(body,(size_t)used,ssid,sizeof(ssid),password,sizeof(password));
  if(ok) {
    xSemaphoreTake(profile_lock,portMAX_DELAY);int i=ps_wifi_profiles_find(&profiles,ssid);
    if(i>=0)snprintf(password,sizeof(password),"%s",profiles.items[i].password);
    else ok=false;
    xSemaphoreGive(profile_lock);
  }
  if(ok)ok=setup_submit(ssid,password,!strcmp(request->uri,"/connect-hotspot"));
  memset(body,0,sizeof(body));memset(ssid,0,sizeof(ssid));memset(password,0,sizeof(password));
  if(!ok)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid saved network / 保存网络无效");
  httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
  return httpd_resp_sendstr(request,"{}");
}
static esp_err_t setup_redirect(httpd_req_t *request) {
  httpd_resp_set_status(request, "302 Found");
  httpd_resp_set_hdr(request, "Location", "http://192.168.4.1/");
  httpd_resp_set_hdr(request,"Cache-Control","no-store");
  return httpd_resp_sendstr(request,"<!doctype html><a href='http://192.168.4.1/'>Open Passport Wi-Fi setup</a>");
}

static esp_err_t setup_404(httpd_req_t *request,httpd_err_code_t error) {
  (void)error;return setup_redirect(request);
}
static esp_err_t setup_status(httpd_req_t *request) {
  static const char *names[]={"waiting","connecting","connected","failed"};
  ps_portal_state state=ps_wifi_setup_state();char json[112];
  snprintf(json,sizeof(json),"{\"state\":\"%s\",\"error\":%d,\"deferred\":%s,\"stored\":%s}",names[state],atomic_load(&profile_status),atomic_load(&setup_deferred)?"true":"false",atomic_load(&setup_saved_ms)?"true":"false");
  httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
  return httpd_resp_sendstr(request,json);
}

static bool setup_register_handlers(httpd_handle_t server) {
  const httpd_uri_t page = {.uri = "/", .method = HTTP_GET,
                            .handler = setup_page};
  const httpd_uri_t save = {.uri = "/wifi", .method = HTTP_POST,
                            .handler = setup_save};
  const httpd_uri_t scan = {.uri = "/scan", .method = HTTP_GET,
                            .handler = setup_scan};
  const httpd_uri_t saved = {.uri="/saved",.method=HTTP_GET,.handler=setup_saved};
  const httpd_uri_t forget = {.uri="/forget",.method=HTTP_POST,.handler=setup_forget};
  const httpd_uri_t connect = {.uri="/connect",.method=HTTP_POST,.handler=setup_connect_saved};
  const httpd_uri_t hotspot = {.uri="/hotspot",.method=HTTP_POST,.handler=setup_save};
  const httpd_uri_t reuse_hotspot = {.uri="/connect-hotspot",.method=HTTP_POST,.handler=setup_connect_saved};
  const httpd_uri_t status = {.uri="/status",.method=HTTP_GET,.handler=setup_status};
  const httpd_uri_t probe = {.uri = "/generate_204", .method = HTTP_GET,
                             .handler = setup_redirect};
  return httpd_register_uri_handler(server, &page) == ESP_OK &&
         httpd_register_uri_handler(server, &save) == ESP_OK &&
         httpd_register_uri_handler(server, &scan) == ESP_OK &&
         httpd_register_uri_handler(server, &saved) == ESP_OK &&
         httpd_register_uri_handler(server, &forget) == ESP_OK &&
         httpd_register_uri_handler(server, &status) == ESP_OK &&
         httpd_register_uri_handler(server, &connect) == ESP_OK &&
         httpd_register_uri_handler(server, &hotspot) == ESP_OK &&
         httpd_register_uri_handler(server, &reuse_hotspot) == ESP_OK &&
         httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,setup_404)==ESP_OK &&
         httpd_register_uri_handler(server, &probe) == ESP_OK;
}
static void event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)arg;
  (void)data;
  if(base==WIFI_EVENT && id==WIFI_EVENT_AP_STACONNECTED && data) {
    unsigned aid=((wifi_event_ap_staconnected_t*)data)->aid;
    if(aid>0 && aid<32)atomic_fetch_or(&setup_peer_mask,1u<<aid);
  }
  if(base==WIFI_EVENT && id==WIFI_EVENT_AP_STADISCONNECTED && data) {
    unsigned aid=((wifi_event_ap_stadisconnected_t*)data)->aid;
    if(aid>0 && aid<32) {
      unsigned previous=atomic_fetch_and(&setup_peer_mask,~(1u<<aid));
      if(!(previous&~(1u<<aid)))atomic_store(&setup_peer_has_ip,false);
    }
  }
  if(base==IP_EVENT && id==IP_EVENT_AP_STAIPASSIGNED)atomic_store(&setup_peer_has_ip,true);
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(flags, 1);
    ESP_LOGI("passport", "WIFI online");
    if (!esp_sntp_enabled()) {
      esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
      esp_sntp_setservername(0, "pool.ntp.org");
      esp_sntp_init();
    }
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(flags, 1);
    const wifi_event_sta_disconnected_t *lost=data;
    if(lost && lost->reason!=WIFI_REASON_ASSOC_LEAVE)
      ESP_LOGW("passport","Wi-Fi disconnect reason=%u",lost->reason);

  }

}
void ps_wifi_init(void) {
  flags = xEventGroupCreate();
  configASSERT(flags);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  configASSERT(esp_netif_create_default_wifi_sta());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL));
  ESP_ERROR_CHECK(
      esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  profile_lock=xSemaphoreCreateMutex();radio_lock=xSemaphoreCreateMutex();
  wifi_commands=xQueueCreate(2,sizeof(wifi_command));
  configASSERT(profile_lock && radio_lock && wifi_commands);
  load_profiles();
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_start());
  configASSERT(xTaskCreate(wifi_worker,"wifi_profiles",4096,NULL,3,NULL)==pdPASS);
}
bool ps_wifi_configure(const char *ssid, const char *pass) {
  if (!ssid || !*ssid || strlen(ssid) > 32 || !pass || strlen(pass) > 63)
    return false;
  wifi_command command={.generation=atomic_load(&setup_generation)};
  if(!wifi_commands || !ps_wifi_profiles_save(&(ps_wifi_profiles){.version=1},ssid,pass))return false;
  strcpy(command.item.ssid,ssid);strcpy(command.item.password,pass);
  int previous=atomic_exchange(&profile_status,3);
  bool queued=xQueueSend(wifi_commands,&command,0)==pdTRUE;
  if(!queued)atomic_store(&profile_status,previous);
  memset(&command,0,sizeof(command));return queued;
}
bool ps_wifi_online(void) { return flags && (xEventGroupGetBits(flags) & 1); }
bool ps_wifi_connected_ssid(char *out, size_t capacity) {
  if (!out || !capacity) return false;
  out[0] = 0;
  wifi_ap_record_t ap = {0};
  if (!ps_wifi_online() || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
  size_t length = strnlen((const char *)ap.ssid, 32);
  if (length >= capacity) return false;
  memcpy(out, ap.ssid, length);
  out[length] = 0;
  return ps_wifi_online();
}
const char *ps_wifi_state(void) {
  return ps_wifi_online() ? "Online" : "Offline / reconnecting";
}

static bool setup_dhcp_dns(void) {
  esp_netif_ip_info_t ip={0};
  if(esp_netif_get_ip_info(setup_ap_netif,&ip)!=ESP_OK)return false;
  esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;dns.ip.u_addr.ip4=ip.ip;
  (void)esp_netif_dhcps_stop(setup_ap_netif);
  uint8_t offer=1;
  bool ok=esp_netif_set_dns_info(setup_ap_netif,ESP_NETIF_DNS_MAIN,&dns)==ESP_OK &&
    esp_netif_dhcps_option(setup_ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer,sizeof(offer))==ESP_OK;
  esp_err_t started=esp_netif_dhcps_start(setup_ap_netif);
  return ok && (started==ESP_OK || started==ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED);
}
bool ps_wifi_setup_start(char *ssid, size_t ssid_capacity, char *password,
                         size_t password_capacity) {
  if (!ssid || !password || ssid_capacity < 20 || password_capacity < 13)
    return false;
  if (setup_active) {
    wifi_config_t active={0};
    if(esp_wifi_get_config(WIFI_IF_AP,&active)!=ESP_OK)return false;
    snprintf(ssid,ssid_capacity,"%.*s",32,(char*)active.ap.ssid);
    snprintf(password,password_capacity,"%.*s",63,(char*)active.ap.password);
    memset(&active,0,sizeof(active));return true;
  }
  if (!setup_ap_netif) setup_ap_netif = esp_netif_create_default_wifi_ap();
  if (!setup_ap_netif) return false;
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(ssid, ssid_capacity, "Passport-%02X%02X", mac[4], mac[5]);
  snprintf(password, password_capacity, "%08lx%04lx",
           (unsigned long)esp_random(),
           (unsigned long)(esp_random() & 0xffff));
  atomic_store(&setup_peer_mask,0);atomic_store(&setup_peer_has_ip,false);
  atomic_store(&setup_deferred,false);atomic_store(&setup_saved_ms,0);
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  setup_flow=(ps_portal_flow){0};memset(setup_target,0,sizeof(setup_target));
  xSemaphoreGive(profile_lock);
  wifi_config_t ap = {0};
  snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", ssid);
  snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", password);
  ap.ap.ssid_len = strlen(ssid);
  ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap.ap.max_connection = 2;
  if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
      esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK) {
    esp_wifi_set_mode(WIFI_MODE_STA);
    memset(password, 0, password_capacity);
    return false;
  }
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.max_uri_handlers = 10;
  cfg.stack_size = 4096;
  if (!setup_dhcp_dns() || !ps_portal_dns_start() || httpd_start(&setup_httpd, &cfg) != ESP_OK ||
      !setup_register_handlers(setup_httpd)) {
    if (setup_httpd) httpd_stop(setup_httpd);
    setup_httpd = NULL;
    ps_portal_dns_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    memset(password, 0, password_capacity);
    return false;
  }
  setup_active = true;setup_running=true;
  return true;
}

bool ps_wifi_setup_active(void) { return setup_running; }
bool ps_wifi_setup_phone_connected(void) {
  return atomic_load(&setup_active) && atomic_load(&setup_peer_mask)!=0 && atomic_load(&setup_peer_has_ip);
}
ps_portal_state ps_wifi_setup_state(void) {
  if(!atomic_load(&setup_running))return PS_PORTAL_WAITING;
  xSemaphoreTake(profile_lock,portMAX_DELAY);ps_portal_state state=setup_flow.state;xSemaphoreGive(profile_lock);
  if(state==PS_PORTAL_CONNECTED && (!ps_wifi_online() || atomic_load(&profile_status)!=0))return PS_PORTAL_CONNECTING;
  return state;
}
bool ps_wifi_setup_ready_to_stop(void) {
  if(!atomic_load(&setup_running) || !ps_wifi_online() || atomic_load(&profile_status)!=0)return false;
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  bool ready=ps_portal_done(&setup_flow,(uint32_t)(esp_timer_get_time()/1000));
  xSemaphoreGive(profile_lock);return ready;
}
static void setup_stop_ap(void) {
  if (!atomic_exchange(&setup_active,false)) return;
  ps_portal_dns_stop();
  if (setup_httpd) httpd_stop(setup_httpd);
  setup_httpd = NULL;
  atomic_store(&setup_peer_mask,0);atomic_store(&setup_peer_has_ip,false);
  esp_wifi_set_mode(WIFI_MODE_STA);
}
bool ps_wifi_setup_waiting_hotspot(void) {
  return atomic_load(&setup_running) && atomic_load(&setup_deferred) && !atomic_load(&setup_active);
}
void ps_wifi_setup_poll(void) {
  unsigned saved=atomic_load(&setup_saved_ms);
  if(saved && atomic_load(&setup_deferred) && (uint32_t)((uint32_t)(esp_timer_get_time()/1000)-saved)>=1800)
    setup_stop_ap();
}
void ps_wifi_setup_stop(void) {
  if(!atomic_exchange(&setup_running,false))return;
  setup_stop_ap();
  xSemaphoreTake(profile_lock,portMAX_DELAY);
  setup_flow=(ps_portal_flow){0};memset(setup_target,0,sizeof(setup_target));
  xSemaphoreGive(profile_lock);
  atomic_store(&setup_deferred,false);atomic_store(&setup_saved_ms,0);atomic_store(&setup_retry_reset,true);
}
