#include "hid.h"
#include "hid_protocol.h"
#include "sdkconfig.h"
#include <stdio.h>
#if CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include <stdatomic.h>
#include <string.h>

typedef struct { uint8_t id,n,data[4]; } report;
static atomic_uint state,connection=BLE_HS_CONN_HANDLE_NONE,mouse_value,consumer_value;
static atomic_bool suspended,release_pending,trace_enabled;
static atomic_uint queued_reports,sent_reports,rejected_reports,failed_reports,completed_reports;
static atomic_bool busy,stop_requested,encrypted,mouse_notify,consumer_notify,synced,forget_bonds;
/* Fixed reusable mailbox: no app pointer or ELF callback is ever retained.
 * Kept after first use to eliminate enqueue/delete races during async stop. */
static QueueHandle_t reports;
static StaticQueue_t report_queue_storage;
static uint8_t report_storage[8*sizeof(report)];
static StaticSemaphore_t host_semaphore_storage;
static SemaphoreHandle_t host_stopped;
static uint16_t mouse_handle,consumer_handle;
static uint8_t own_addr_type,battery;
static unsigned report_second,report_count;
static const uint8_t mouse_ref[]={1,1},consumer_ref[]={2,1};
void ble_store_config_init(void);
static int gap_event(struct ble_gap_event *,void *);
static void update_state(void) {
  if(atomic_load(&stop_requested))return;
  atomic_store(&state,atomic_load(&connection)==BLE_HS_CONN_HANDLE_NONE?PS_HID_ADVERTISING:
    atomic_load(&encrypted)&&atomic_load(&mouse_notify)&&!atomic_load(&suspended)?PS_HID_READY:PS_HID_CONNECTED);
}
static int value_access(uint16_t conn,uint16_t attr,struct ble_gatt_access_ctxt *c,void *arg) {
  (void)conn;(void)attr;
  unsigned kind=(uintptr_t)arg;const void *data=NULL;size_t size=0;
  uint32_t snapshot=0;
  /* Internal development identity; no third-party vendor is impersonated. */
  static const uint8_t pnp[]={1,255,255,1,0,0,1};
  static const uint8_t info[]={0x11,0x01,0,2},protocol=1;
  static const char maker[]="FoloToy",model[]="AI Passport";
  if(c->op==BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint8_t v=255;uint16_t n=0;
    if(ble_hs_mbuf_to_flat(c->om,&v,1,&n) || n!=1)return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if((kind==2 && v>1)||(kind==3 && v!=1))return BLE_ATT_ERR_UNLIKELY;
    if(kind==2) {
      atomic_store(&suspended,v==0);xQueueReset(reports);
      ESP_LOGI("passport_ble","control suspended=%d",v==0);
      atomic_store(&mouse_value,0);atomic_store(&consumer_value,0);
      if(v==1)atomic_store(&release_pending,true);
      update_state();
    }
    return 0;
  }
  switch(kind) {
    case 0:data=info;size=sizeof(info);break;
    case 1:data=ps_hid_report_map;size=ps_hid_report_map_size;break;
    case 3:data=&protocol;size=1;break;
    case 4:snapshot=atomic_load(&mouse_value);data=&snapshot;size=4;break;
    case 5:snapshot=atomic_load(&consumer_value);data=&snapshot;size=2;break;
    case 6:data=mouse_ref;size=2;break;
    case 7:data=consumer_ref;size=2;break;
    case 8:data=&battery;size=1;break;
    case 9:data=maker;size=sizeof(maker)-1;break;
    case 10:data=model;size=sizeof(model)-1;break;
    case 11:data=pnp;size=sizeof(pnp);break;
    default:return BLE_ATT_ERR_UNLIKELY;
  }
  return os_mbuf_append(c->om,data,size)?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
#define UUID(x) BLE_UUID16_DECLARE(x)
#define ACCESS(k) .access_cb=value_access,.arg=(void *)(uintptr_t)(k)
static const struct ble_gatt_svc_def services[]={
 {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=UUID(0x1812),.characteristics=(struct ble_gatt_chr_def[]){
  {.uuid=UUID(0x2a4a),ACCESS(0),.flags=BLE_GATT_CHR_F_READ},
  {.uuid=UUID(0x2a4b),ACCESS(1),.flags=BLE_GATT_CHR_F_READ},
  {.uuid=UUID(0x2a4c),ACCESS(2),.flags=BLE_GATT_CHR_F_WRITE_NO_RSP|BLE_GATT_CHR_F_WRITE_ENC},
  {.uuid=UUID(0x2a4e),ACCESS(3),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_WRITE_NO_RSP|BLE_GATT_CHR_F_WRITE_ENC},
  {.uuid=UUID(0x2a4d),ACCESS(4),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_NOTIFY,.val_handle=&mouse_handle,
   .descriptors=(struct ble_gatt_dsc_def[]){{.uuid=UUID(0x2908),.att_flags=BLE_ATT_F_READ,ACCESS(6)},{0}}},
  {.uuid=UUID(0x2a4d),ACCESS(5),.flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_READ_ENC|BLE_GATT_CHR_F_NOTIFY,.val_handle=&consumer_handle,
   .descriptors=(struct ble_gatt_dsc_def[]){{.uuid=UUID(0x2908),.att_flags=BLE_ATT_F_READ,ACCESS(7)},{0}}},
  {0}}},
 {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=UUID(0x180f),.characteristics=(struct ble_gatt_chr_def[]){
  {.uuid=UUID(0x2a19),ACCESS(8),.flags=BLE_GATT_CHR_F_READ},{0}}},
 {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=UUID(0x180a),.characteristics=(struct ble_gatt_chr_def[]){
  {.uuid=UUID(0x2a29),ACCESS(9),.flags=BLE_GATT_CHR_F_READ},
  {.uuid=UUID(0x2a24),ACCESS(10),.flags=BLE_GATT_CHR_F_READ},
  {.uuid=UUID(0x2a50),ACCESS(11),.flags=BLE_GATT_CHR_F_READ},{0}}},
 {0}
};
static void advertise(void) {
  if(atomic_load(&stop_requested))return;
  struct ble_hs_adv_fields f={0};
  const char *name="Passport Remote";
  f.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
  f.appearance=0x03c2;f.appearance_is_present=1;
  ble_uuid16_t uuid=BLE_UUID16_INIT(0x1812);f.uuids16=&uuid;f.num_uuids16=1;f.uuids16_is_complete=1;
  f.name=(uint8_t *)name;f.name_len=strlen(name);f.name_is_complete=1;
  struct ble_gap_adv_params p={0};p.conn_mode=BLE_GAP_CONN_MODE_UND;p.disc_mode=BLE_GAP_DISC_MODE_GEN;
  p.itvl_min=BLE_GAP_ADV_ITVL_MS(50);p.itvl_max=BLE_GAP_ADV_ITVL_MS(100);
  int rc=ble_gap_adv_set_fields(&f);
  if(!rc)rc=ble_gap_adv_start(own_addr_type,NULL,180000,&p,gap_event,NULL);
  atomic_store(&state,rc?PS_HID_ERROR:PS_HID_ADVERTISING);
  ESP_LOGI("passport_ble","advertising rc=%d",rc);
}
static int gap_event(struct ble_gap_event *e,void *arg) {
  (void)arg;
  switch(e->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI("passport_ble","connect status=%d",e->connect.status);
      if(e->connect.status){advertise();break;}
      atomic_store(&connection,e->connect.conn_handle);update_state();
      (void)ble_gap_security_initiate(e->connect.conn_handle);break;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI("passport_ble","disconnect reason=%d",e->disconnect.reason);
      atomic_store(&connection,BLE_HS_CONN_HANDLE_NONE);atomic_store(&encrypted,false);
      atomic_store(&mouse_notify,false);atomic_store(&consumer_notify,false);xQueueReset(reports);advertise();break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
      struct ble_gap_conn_desc d;
      if(!ble_gap_conn_find(e->enc_change.conn_handle,&d))atomic_store(&encrypted,d.sec_state.encrypted!=0);
      ESP_LOGI("passport_ble","encryption status=%d active=%d",e->enc_change.status,(int)atomic_load(&encrypted));
      update_state();break; }
    case BLE_GAP_EVENT_SUBSCRIBE:
      if(e->subscribe.attr_handle==mouse_handle)atomic_store(&mouse_notify,e->subscribe.cur_notify!=0);
      if(e->subscribe.attr_handle==consumer_handle)atomic_store(&consumer_notify,e->subscribe.cur_notify!=0);
      ESP_LOGI("passport_ble","subscriptions mouse=%d consumer=%d",(int)atomic_load(&mouse_notify),(int)atomic_load(&consumer_notify));
      update_state();break;
    case BLE_GAP_EVENT_NOTIFY_TX:
      if(!e->notify_tx.status)atomic_fetch_add(&completed_reports,1);
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      /* A paired peer must be forgotten deliberately on the phone; do not
       * silently delete an existing bond at an unauthenticated request. */
      return BLE_GAP_REPEAT_PAIRING_IGNORE;
    case BLE_GAP_EVENT_ADV_COMPLETE:
      if(!atomic_load(&stop_requested))atomic_store(&state,PS_HID_ERROR);
      break;
    default:break;
  }
  return 0;
}
static void on_sync(void) {
  if(ble_hs_util_ensure_addr(0) || ble_hs_id_infer_auto(0,&own_addr_type)) {
    atomic_store(&state,PS_HID_ERROR);return;
  }
  atomic_store(&synced,true);advertise();
}
static void host_task(void *arg) {
  (void)arg;nimble_port_run();xSemaphoreGive(host_stopped);vTaskSuspend(NULL);
}
static void control_task(void *arg) {
  (void)arg;TaskHandle_t host=NULL;bool initialized=false;int rc=nimble_port_init();
  if(rc)goto finish;
  initialized=true;
  ble_hs_cfg.sync_cb=on_sync;ble_hs_cfg.store_status_cb=ble_store_util_status_rr;
  ble_hs_cfg.sm_io_cap=BLE_HS_IO_NO_INPUT_OUTPUT;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=0;ble_hs_cfg.sm_sc=1;
  ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
  ble_svc_gap_init();ble_svc_gatt_init();ble_store_config_init();
  if(atomic_load(&forget_bonds)){rc=ble_store_clear();if(rc)goto finish;}
  rc=ble_svc_gap_device_name_set("Passport Remote");
  if(!rc)rc=ble_gatts_count_cfg(services);
  if(!rc)rc=ble_gatts_add_svcs(services);
  if(rc)goto finish;
  int soc=bsp_battery_soc();battery=soc<0?0:soc>100?100:(uint8_t)soc;
  if(xTaskCreate(host_task,"passport_ble_host",4096,NULL,configMAX_PRIORITIES-4,&host)!=pdPASS){rc=-1;goto finish;}
  while(!atomic_load(&stop_requested) && atomic_load(&state)!=PS_HID_ERROR) {
    if(ps_hid_state()==PS_HID_READY && atomic_exchange(&release_pending,false)) {
      report neutral={.id=1,.n=4};(void)xQueueSendToFront(reports,&neutral,0);
      if(atomic_load(&consumer_notify)){neutral.id=2;neutral.n=2;(void)xQueueSendToFront(reports,&neutral,0);}
    }
    report r;
    if(xQueueReceive(reports,&r,pdMS_TO_TICKS(30))!=pdTRUE)continue;
    if(ps_hid_state()!=PS_HID_READY || (r.id==2 && !atomic_load(&consumer_notify)))continue;
    struct os_mbuf *om=ble_hs_mbuf_from_flat(r.data,r.n);
    if(!om){ESP_LOGW("passport_ble","report allocation unavailable");atomic_store(&state,PS_HID_ERROR);break;}
    int sent=ble_gatts_notify_custom(atomic_load(&connection),r.id==1?mouse_handle:consumer_handle,om);
    if(!sent){atomic_fetch_add(&sent_reports,1);
      if(atomic_load(&trace_enabled))ESP_LOGI("passport_ble","report id=%u n=%u data=%02x/%02x/%02x/%02x",r.id,r.n,r.data[0],r.data[1],r.data[2],r.data[3]);
      uint32_t snapshot=0;memcpy(&snapshot,r.data,r.n);atomic_store(r.id==1?&mouse_value:&consumer_value,snapshot);}
    if(sent){atomic_fetch_add(&failed_reports,1);ESP_LOGW("passport_ble","report rejected rc=%d",sent);atomic_store(&state,PS_HID_ERROR);break;}
  }
finish:
  if(!rc && atomic_load(&state)==PS_HID_ERROR)rc=-1;
  atomic_store(&state,PS_HID_STOPPING);
  if(host) {
    /* Release any held button before disconnect, without touching app memory. */
    if(atomic_load(&encrypted)) {
      const uint8_t neutral[4]={0};struct os_mbuf *om=ble_hs_mbuf_from_flat(neutral,4);
      if(om)(void)ble_gatts_notify_custom(atomic_load(&connection),mouse_handle,om);
      om=ble_hs_mbuf_from_flat(neutral,2);
      if(om)(void)ble_gatts_notify_custom(atomic_load(&connection),consumer_handle,om);
    }
    int stopped=nimble_port_stop();
    if(stopped || xSemaphoreTake(host_stopped,pdMS_TO_TICKS(3000))!=pdTRUE) {
      atomic_store(&state,PS_HID_ERROR);ESP_LOGE("passport_ble","host stop failed rc=%d; restart required",stopped);
      vTaskDelete(NULL);return; /* Fail closed: do not deinit a live host. */
    }
    vTaskDelete(host);
  }
  if(initialized){int cleanup=nimble_port_deinit();if(cleanup)rc=cleanup;}
  xQueueReset(reports);atomic_store(&connection,BLE_HS_CONN_HANDLE_NONE);
  atomic_store(&mouse_notify,false);atomic_store(&consumer_notify,false);atomic_store(&encrypted,false);
  atomic_store(&state,rc?PS_HID_ERROR:PS_HID_OFF);atomic_store(&busy,false);
  ESP_LOGI("passport_ble","stopped rc=%d free=%u largest=%u stack=%u",rc,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
    (unsigned)uxTaskGetStackHighWaterMark(NULL));
  vTaskDelete(NULL);
}
void ps_hid_prepare_storage(void) {
  /* ESP-IDF 5.5.3 store/config/ble_store_nvs.c pins this namespace. NVS
   * retains namespace bookkeeping after close; creating it during radio init
   * can strand a tiny permanent block between large audio allocations. */
  nvs_handle_t h;
  int rc=nvs_open("nimble_bond",NVS_READWRITE,&h);
  if(!rc)nvs_close(h);
  else ESP_LOGW("passport_ble","bond namespace unavailable rc=%d",rc);
}
bool ps_hid_start(unsigned flags) {
  if(flags>1)return false;
  if(atomic_exchange(&busy,true))return false;
  atomic_store(&forget_bonds,flags==1);
  if(!reports)reports=xQueueCreateStatic(8,sizeof(report),report_storage,&report_queue_storage);
  if(!host_stopped)host_stopped=xSemaphoreCreateBinaryStatic(&host_semaphore_storage);
  if(!reports || !host_stopped){atomic_store(&busy,false);return false;}
  while(xSemaphoreTake(host_stopped,0)==pdTRUE){}
  xQueueReset(reports);atomic_store(&stop_requested,false);atomic_store(&synced,false);
  atomic_store(&suspended,false);atomic_store(&release_pending,false);atomic_store(&mouse_value,0);atomic_store(&consumer_value,0);
  atomic_store(&state,PS_HID_STARTING);report_second=report_count=0;
  atomic_store(&queued_reports,0);atomic_store(&sent_reports,0);atomic_store(&rejected_reports,0);atomic_store(&failed_reports,0);atomic_store(&completed_reports,0);
  if(xTaskCreate(control_task,"passport_ble_ctl",4096,NULL,3,NULL)!=pdPASS) {
    atomic_store(&state,PS_HID_ERROR);atomic_store(&busy,false);return false;
  }
  return true;
}
unsigned ps_hid_state(void){return atomic_load(&state);}
bool ps_hid_report(unsigned id,const void *data,size_t n) {
  if(ps_hid_state()!=PS_HID_READY || !ps_hid_report_valid(id,data,n) || (id==2 && !atomic_load(&consumer_notify))){atomic_fetch_add(&rejected_reports,1);return false;}
  unsigned second=esp_timer_get_time()/1000000;
  if(second!=report_second){report_second=second;report_count=0;}
  if(report_count>=40)return false;
  report r={.id=id,.n=n};memcpy(r.data,data,n);
  if(xQueueSend(reports,&r,0)!=pdTRUE)return false;
  report_count++;atomic_fetch_add(&queued_reports,1);return true;
}
void ps_hid_stop(void) {
  if(!atomic_load(&busy))return;
  atomic_store(&stop_requested,true);atomic_store(&state,PS_HID_STOPPING);
}
void ps_hid_trace(bool enabled){atomic_store(&trace_enabled,enabled);printf("PASSPORT BLE_TRACE %s\n",enabled?"on":"off");}
void ps_hid_debug(void) {
 printf("PASSPORT BLE_DEBUG state=%u connected=%u encrypted=%u mouse_notify=%u consumer_notify=%u suspended=%u queued=%u sent=%u completed=%u rejected=%u failed=%u\n",
  ps_hid_state(),atomic_load(&connection)!=BLE_HS_CONN_HANDLE_NONE,(unsigned)atomic_load(&encrypted),
  (unsigned)atomic_load(&mouse_notify),(unsigned)atomic_load(&consumer_notify),(unsigned)atomic_load(&suspended),
  atomic_load(&queued_reports),atomic_load(&sent_reports),atomic_load(&completed_reports),atomic_load(&rejected_reports),atomic_load(&failed_reports));
}

#else
void ps_hid_debug(void){printf("PASSPORT BLE_DEBUG state=7\n");}
void ps_hid_trace(bool enabled){(void)enabled;}
void ps_hid_prepare_storage(void){}
bool ps_hid_start(unsigned flags){(void)flags;return false;}
unsigned ps_hid_state(void){return PS_HID_UNAVAILABLE;}
bool ps_hid_report(unsigned id,const void *data,size_t n){(void)id;(void)data;(void)n;return false;}
void ps_hid_stop(void){}
#endif
