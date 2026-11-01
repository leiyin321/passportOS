#include "esp_rom_sys.h"
#include "asset_loader.h"
#include "asset_probe.h"
#include "app_version.h"
#include "audio.h"
#include "volume.h"
#include "hid.h"
#include "ps_alarm.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "cJSON.h"
#include "driver/usb_serial_jtag.h"
#include "native_exec_pool.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "storage_usage.h"
#include "distribution_defaults.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "icon_registry.h"
#include "language.h"
#include "ps_settings_list.h"
#include "lvgl.h"
#include "mbedtls/sha256.h"
#include "network.h"
#include "notification.h"
#include "media.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "package.h"
#include "redirect.h"
#include "runtime.h"
#include "session.h"
#include "store.h"
#include "trust.h"
#include "update.h"
#include "wifi.h"
#include "wifi_setup_qr.h"
#include "src/draw/lv_draw_buf_private.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
typedef struct {
  int kind, value;
  uint32_t sent_us;
  void *data;
  size_t size;
} message;
LV_FONT_DECLARE(passport_cjk_extra_16);
LV_FONT_DECLARE(passport_cjk_chat_16);
enum { KEY, COMMAND, INSTALL, UPLOAD_BEGIN };
static SemaphoreHandle_t upload_gate;
static atomic_bool upload_pending, cancel_cycle;
static QueueHandle_t events;
static lv_obj_t *title, *body, *status, *clock_label, *footer, *canvas, *battery_label;
static lv_obj_t *home_tiles;
static lv_obj_t *cards_view, *app_host, *app_canvas, *app_body;
typedef enum { CARDS_NONE, CARDS_HOME, CARDS_LIBRARY, CARDS_STORE } card_kind_t;
static card_kind_t cards_kind;
static int cards_focus, cards_base, cards_count;
static char cards_labels[4][40];
static char cards_versions[4][40];
static unsigned running_image_bytes;
static ps_storage_usage storage_usage;
static unsigned storage_usage_page;
static bool storage_usage_valid;
static lv_obj_t *storage_bar, *storage_legend;
static const uint32_t storage_colors[PS_STORAGE_PARTS]={0x79baff,0xe1b3ff,0xffca80,0x6ce3c2,0xa0aabc,0x405064};
static unsigned cards_icons[4];
static int home_focus;
static int page, focus, selection, action_index;
static bool storage_ready, app_active, mic_prompt;
static char ids[12][64];
static char library_versions[12][40];
static int app_count;
static ps_package package;
static ps_session session;
static ps_view display_view;
static char notice[160];
static char preview_id[64];
static ps_image_command preview_image;
static void reset_preview(void) {
  preview_id[0] = 0;
  if (bsp_lvgl_lock(1000)) {
    if (canvas)
      lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    display_view = (ps_view){0};
    ps_assets_unpin();
    ps_assets_close();
    bsp_lvgl_unlock();
  }
}
static cJSON *catalog;
static char pending_publisher[512], pending_key_id[17];
static bool publisher_pending;
static char service_ip[16] = "0.0.0.0";
static unsigned service_http = 8040, service_ws = 8041;
static char store_url[256];
static char update_url[512];
static bool service_pending = false, service_approved = false;
static bool wifi_setup_page;
static unsigned wifi_setup_step;
static bool wifi_setup_auto_advanced,wifi_setup_success_shown;
static lv_obj_t *wifi_qr;
static int wifi_qr_step=-1;
static lv_draw_buf_handlers_t wifi_qr_handlers;
static char wifi_setup_ssid[33], wifi_setup_password[16];
static bool settings_detail;
static bool sound_alarm;
static int settings_selection;
static bool system_light_mode;
static unsigned settings_scroll_base;
#define SETTINGS_VISIBLE_ROWS 8
static uint8_t settings_brightness = 65;
static uint16_t screen_timeout_seconds = 60;
static int64_t last_activity_us;
static bool screen_is_off;
static ps_alarm_config alarm_config = {.hour = 7, .minute = 0, .enabled = false};
static int alarm_field;
static bool alarm_dirty, alarm_ringing, alarm_audio_pending;
static int64_t alarm_dirty_us;
static int32_t alarm_last_day = INT32_MIN;
static bool app_button_down[BSP_BTN_COUNT];
static ps_language_t system_language = PS_LANGUAGE_EN;
static int settings_installed_count;
static lv_obj_t *settings_view;
static char default_app[64];
static int catalog_selection, store_request;
static char store_installed_id[64];
static unsigned frame_count;
static unsigned frame_period_us = 50000;
static int64_t app_tick_time;
static atomic_uint display_frames, input_pending_us, input_last_us, input_max_us;
static atomic_uint render_last_us, render_max_us, lvgl_lock_max_us;
static uint32_t render_started_us;
static void metric_max(atomic_uint *value,uint32_t next) {
  unsigned prior=atomic_load(value);
  while(prior<next && !atomic_compare_exchange_weak(value,&prior,next)) {}
}
static void display_event(lv_event_t *e) {
  uint32_t now=(uint32_t)esp_timer_get_time();
  if(lv_event_get_code(e)==LV_EVENT_REFR_START){render_started_us=now;return;}
  display_frames++;
  uint32_t render=now-render_started_us;
  atomic_store(&render_last_us,render);metric_max(&render_max_us,render);
  uint32_t pending=atomic_exchange(&input_pending_us,0);
  if(pending){uint32_t latency=now-pending;atomic_store(&input_last_us,latency);metric_max(&input_max_us,latency);}
}
static void set_label_text(lv_obj_t *label,const char *text) {
  if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
}
static bool paint_visible(lv_layer_t *layer,const lv_area_t *a) {
  /* Task creation callback, not a draw unit. LVGL 9.5's current clip is valid here. */
  const lv_area_t *c=&layer->_clip_area;
  return a->x1<=c->x2 && a->x2>=c->x1 && a->y1<=c->y2 && a->y2>=c->y1;
}

static int64_t frame_start, frame_max;
static void release_package(void *p) { ps_package_free(p); }
static void release_runtime(void *p) {
  (void)p;
  ps_runtime_stop();
  if (!ps_audio_stop())
    ESP_LOGE("passport", "AUDIO stop timed out");
  ps_net_cancel();
  if (bsp_lvgl_lock(1000)) {
    display_view = (ps_view){0};
    if (canvas) lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    if (app_canvas) lv_obj_add_flag(app_canvas, LV_OBJ_FLAG_HIDDEN);
    if (app_host) lv_obj_add_flag(app_host, LV_OBJ_FLAG_HIDDEN);
    ps_assets_unpin();
    ps_assets_close();
    bsp_lvgl_unlock();
  }
}

static const char *items[][2] = {{"App Library", "应用库"},
                                 {"App Store", "应用商店"},
                                 {"Settings", "设置"}};
enum { SET_WIFI, SET_BLUETOOTH, SET_DISPLAY, SET_APPEARANCE, SET_TIMEOUT, SET_SOUND,
       SET_PRIVACY, SET_DATETIME, SET_ALARM, SET_UPDATE, SET_LANGUAGE, SET_STORAGE,
       SET_ABOUT, SET_COUNT };
static const char *setting_names[SET_COUNT][2] = {
    {"Wi-Fi", "Wi-Fi"}, {"Bluetooth", "蓝牙"}, {"Display", "显示"},
    {"Appearance", "外观"},
    {"Screen Timeout", "熄屏时间"}, {"Sounds", "声音"},
    {"Privacy", "隐私"}, {"Date & Time", "日期与时间"},
    {"Alarm", "闹钟"},
    {"System Update", "系统更新"}, {"Language", "语言"},
    {"Storage", "存储空间"}, {"About", "关于"}};
static const char *setting_icons[SET_COUNT] = {
    "system.wifi", "system.app-default", "system.settings",
    "system.settings",
    "system.settings", "system.microphone", "system.check",
    "system.settings", "system.settings", "system.download", "system.download",
    "system.app-default", "system.check"};
static const uint32_t setting_colors[SET_COUNT] = {
    0x35c76f, 0x568cff, 0xffa940, 0x6d8cff, 0x7d8cff, 0xe970a5,
    0x8d78ef, 0x7894b5, 0xf4a261, 0x43b9c6, 0x67c7aa, 0x43b9c6,
    0x8998a9};
#define TXT(en, zh) ps_language_select(system_language, (en), (zh))
static uint32_t theme_rgb(uint32_t dark, uint32_t light) {
  return system_light_mode ? light : dark;
}
static const lv_font_t *multilingual_font(void) {
  static lv_font_t chat_font,localized_font;
  static bool initialized;
  if(!initialized) {
    chat_font=passport_cjk_chat_16;chat_font.fallback=&lv_font_montserrat_14;
    localized_font=passport_cjk_extra_16;localized_font.fallback=&chat_font;
    initialized=true;
  }
  return &localized_font;
}
static const lv_font_t *system_font(void) {
  return system_language==PS_LANGUAGE_ZH_CN ? multilingual_font():&lv_font_montserrat_14;
}

static const char *language_name(void) {
  return system_language == PS_LANGUAGE_ZH_CN ? "简体中文" : "English";
}
static bool settings_save_language(void) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t rc = nvs_set_u8(h, "language", (uint8_t)system_language);
  if (rc == ESP_OK) rc = nvs_commit(h);
  nvs_close(h);
  if (rc != ESP_OK) return false;
  ps_runtime_set_system_language(ps_language_code(system_language));
  return true;
}
static void settings_toggle_language(void) {
  ps_language_t previous = system_language;
  system_language = system_language == PS_LANGUAGE_EN ? PS_LANGUAGE_ZH_CN
                                                       : PS_LANGUAGE_EN;
  if (!settings_save_language()) {
    system_language = previous;
    ps_runtime_set_system_language(ps_language_code(system_language));
    snprintf(notice, sizeof(notice), "%s", "Language change could not be saved");
  } else {
    notice[0] = 0;
  }
}
static const char *display_mode_name(void) {
  return system_light_mode ? TXT("Light", "浅色") : TXT("Dark", "深色");
}
static bool settings_save_display_mode(void) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t rc = nvs_set_u8(h, "display_mode", system_light_mode ? 1 : 0);
  if (rc == ESP_OK) rc = nvs_commit(h);
  nvs_close(h);
  if (rc == ESP_OK)
    ps_runtime_set_system_display_mode(system_light_mode ? "light" : "dark");
  return rc == ESP_OK;
}
static void settings_toggle_display_mode(void) {
  bool previous = system_light_mode;
  system_light_mode = !system_light_mode;
  if (!settings_save_display_mode()) {
    system_light_mode = previous;
    ps_runtime_set_system_display_mode(system_light_mode ? "light" : "dark");
    snprintf(notice, sizeof(notice), "%s", "Appearance change could not be saved");
  } else {
    notice[0] = 0;
  }
}
static const char *localized_notice(const char *message) {
  if (system_language != PS_LANGUAGE_ZH_CN || !message || !*message)
    return message;
  static char translated[192];
  static const struct { const char *en, *zh; } entries[] = {
      {"Package unavailable / invalid", "应用包不可用或无效"},
      {"HTTPS catalog configured. OK browse", "应用商店地址已设置。按 OK 浏览"},
      {"Wi-Fi connected successfully", "Wi-Fi 连接成功"},
      {"Phone Wi-Fi setup could not start", "无法启动手机配网"},
      {"App and data deleted", "应用及数据已删除"},
      {"Delete failed", "删除失败"},
      {"Service approved. OK to browse", "服务已批准。按 OK 浏览"},
      {"Service denied", "服务已拒绝"},
      {"Default app saved", "默认应用已保存"},
      {"Default app cleared", "默认应用已清除"},
      {"Could not set default app", "无法设置默认应用"},
      {"Code removed; data kept.\nReinstall from Store.", "应用代码已移除，数据保留。\n可从商店重新安装。"},
      {"Code already offloaded", "应用代码已卸载"},
      {"Offline pin enabled", "已设为离线保留"},
      {"Pin removed", "已取消离线保留"},
      {"Pin failed", "离线保留设置失败"},
      {"Installed. Open App Library.", "安装完成。请打开应用库。"},
      {"Installed. OK: open app.", "安装完成。按 OK 打开应用。"},
      {"Already up to date. OK: open app.", "已是最新版本。按 OK 打开应用。"},
      {"Install failed / signature or hash", "安装失败：签名或哈希校验错误"},
      {"Language change could not be saved", "语言设置保存失败"},
      {"Alarm is ringing. Press any key to dismiss.", "闹钟响铃中。按任意键停止。"},
      {"Invalid display mode/orientation", "应用请求了无效的屏幕模式或方向"},
      {"Invalid declarative document", "应用界面描述无效"},
      {"VM allocation failed", "应用运行环境内存不足"},
      {"permission denied", "权限被拒绝"},
      {"execution budget exceeded", "应用运行超时或指令超限"},
      {"text too long", "应用文本长度超限"},
      {"draw bounds", "应用绘图超出屏幕范围"},
      {"draw quota", "应用绘图指令数量超限"},
      {"storage write event budget", "应用存储写入次数超限"},
      {"icon bounds/quota", "应用图标超出范围或数量限制"},
      {"sprite bounds/quota", "应用图片超出范围或数量限制"},
      {"find bounds", "应用文本查找长度超限"},
      {"patterns disabled; use plain find", "不支持模式匹配，请使用普通文本查找"},
      {"Loading Store...", "正在加载应用商店…"},
      {"Offline / busy / endpoint denied", "网络离线、繁忙或服务地址未授权"},
      {"Catalog unavailable / invalid", "商店目录不可用或无效"},
      {"App stopped safely", "应用已安全停止"},
      {"Network busy or offline", "网络繁忙或未连接"},
      {"Manifest HTTPS request failed", "更新清单 HTTPS 请求失败"},
      {"Manifest or signature invalid", "更新清单或签名无效"},
      {"Could not start OTA write", "无法开始 OTA 写入"},
      {"Image hash failed", "镜像哈希计算失败"},
      {"Image hash mismatch", "镜像哈希不匹配"},
      {"Image hash or ESP image invalid", "镜像哈希或 ESP 镜像无效"},
      {"Could not select update slot", "无法选择更新系统槽"},
      {"Update image incomplete", "系统更新镜像不完整"},
      {"Update download could not start", "无法开始下载系统更新"},
      {"App data deleted", "应用数据已删除"},
  };
  for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    if (!strcmp(message, entries[i].en)) return entries[i].zh;
  if (!strncmp(message, "Update check failed: ", 21)) {
    snprintf(translated, sizeof(translated), "更新检查失败：%.150s", message + 21);
    return translated;
  }
  if (!strncmp(message, "Update download failed: ", 24)) {
    snprintf(translated, sizeof(translated), "更新下载失败：%.150s", message + 24);
    return translated;
  }
  if (!strncmp(message, "App stopped safely:\n", 20)) {
    snprintf(translated, sizeof(translated), "应用已安全停止：\n%.150s", message + 20);
    return translated;
  }
  if (strstr(message, "permission denied")) return "应用权限被拒绝";
  if (strstr(message, "execution budget exceeded")) return "应用运行超时或指令超限";
  if (strstr(message, "VM allocation failed")) return "应用运行环境内存不足";
  if (strstr(message, "draw bounds")) return "应用绘图超出屏幕范围";
  if (strstr(message, "draw quota")) return "应用绘图指令数量超限";
  if (!strncmp(message, "Service ", 8) && strstr(message, ". OK to browse"))
    return "服务已批准。按 OK 浏览";
  return message;
}
static bool system_time_valid(struct tm *local) {
  time_t now = time(NULL);
  if (now < 1704067200) return false; /* 2024-01-01 UTC */
  if (local && !localtime_r(&now, local)) return false;
  return true;
}
static void format_clock(char *out, size_t capacity, const char *format,
                         const char *fallback) {
  struct tm local;
  if (system_time_valid(&local) && strftime(out, capacity, format, &local))
    return;
  snprintf(out, capacity, "%s", fallback);
}
static void settings_save_brightness(void) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_u8(h, "brightness", settings_brightness);
    nvs_commit(h);
    nvs_close(h);
  }
}
static void settings_adjust_volume(int direction) {
  unsigned value=ps_audio_volume_adjust(ps_audio_volume(sound_alarm),direction);
  if (value==ps_audio_volume(sound_alarm)) return;
  nvs_handle_t h;
  esp_err_t rc=nvs_open("osconfig",NVS_READWRITE,&h);
  if (rc==ESP_OK) {
    rc=nvs_set_u8(h,sound_alarm ? "alarm_volume" : "media_volume",value);
    if (rc==ESP_OK) rc=nvs_commit(h);
    nvs_close(h);
  }
  if (rc==ESP_OK) ps_audio_volume_set(sound_alarm,value);
  else snprintf(notice,sizeof(notice),"%s",TXT("Could not save volume","音量保存失败"));
}
static void settings_adjust_brightness(int delta) {
  int value = settings_brightness + delta;
  if (value < 20) value = 20;
  if (value > 100) value = 100;
  settings_brightness = (uint8_t)value;
  bsp_display_backlight(settings_brightness);
  settings_save_brightness();
}
static void settings_save_timeout(void) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READWRITE, &h) == ESP_OK) {
    nvs_set_u16(h, "screen_timeout", screen_timeout_seconds);
    nvs_commit(h);
    nvs_close(h);
  }
}
static bool alarm_save(void) {
  nvs_handle_t h;
  esp_err_t rc = nvs_open("osconfig", NVS_READWRITE, &h);
  if (rc == ESP_OK) {
    rc = nvs_set_u8(h, "alarm_hour", alarm_config.hour);
    if (rc == ESP_OK) rc = nvs_set_u8(h, "alarm_min", alarm_config.minute);
    if (rc == ESP_OK) rc = nvs_set_u8(h, "alarm_on", alarm_config.enabled ? 1 : 0);
    if (rc == ESP_OK) rc = nvs_commit(h);
    nvs_close(h);
  }
  if (rc != ESP_OK) {
    ESP_LOGE("passport", "ALARM save failed rc=%s", esp_err_to_name(rc));
    return false;
  }
  alarm_dirty = false;
  return true;
}
static void alarm_load(void) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READONLY, &h) != ESP_OK) return;
  uint8_t hour = alarm_config.hour, minute = alarm_config.minute, enabled = 0;
  nvs_get_u8(h, "alarm_hour", &hour);
  nvs_get_u8(h, "alarm_min", &minute);
  nvs_get_u8(h, "alarm_on", &enabled);
  nvs_close(h);
  if (hour < 24 && minute < 60) {
    alarm_config.hour = hour;
    alarm_config.minute = minute;
    alarm_config.enabled = enabled == 1;
  }
}
static void alarm_adjust(int field, int direction) {
  ps_alarm_adjust(&alarm_config, field, direction);
  alarm_dirty = true;
  alarm_dirty_us = esp_timer_get_time();
}
static void settings_adjust_timeout(int direction) {
  static const uint16_t choices[] = {15, 30, 60, 120, 300, 0};
  int current = 0;
  for (int i = 0; i < 6; i++)
    if (choices[i] == screen_timeout_seconds) current = i;
  current = (current + (direction > 0 ? 1 : 5)) % 6;
  screen_timeout_seconds = choices[current];
  settings_save_timeout();
  last_activity_us = esp_timer_get_time();
}
static void default_app_load(void) {
  nvs_handle_t h;
  default_app[0] = 0;
  if (nvs_open("osconfig", NVS_READONLY, &h) == ESP_OK) {
    size_t size = sizeof(default_app);
    if (nvs_get_str(h, "default_app", default_app, &size) != ESP_OK)
      default_app[0] = 0;
    nvs_close(h);
  }
}
static bool default_app_store(const char *id) {
  nvs_handle_t h;
  if (nvs_open("osconfig", NVS_READWRITE, &h) != ESP_OK)
    return false;
  esp_err_t rc = id && *id ? nvs_set_str(h, "default_app", id)
                           : nvs_erase_key(h, "default_app");
  if (rc == ESP_ERR_NVS_NOT_FOUND && (!id || !*id)) rc = ESP_OK;
  if (rc == ESP_OK) rc = nvs_commit(h);
  nvs_close(h);
  if (rc != ESP_OK) return false;
  snprintf(default_app, sizeof(default_app), "%s", id ? id : "");
  return true;
}
static bool default_app_select(const char *id) {
  if (!id || !*id) return false;
  if (!strcmp(default_app, id)) return default_app_store("");
  ps_package candidate;
  if (!ps_store_load(id, &candidate)) return false;
  ps_package_free(&candidate);
  return default_app_store(id);
}
static const char *timeout_label(void) {
  switch (screen_timeout_seconds) {
    case 15: return TXT("15 sec", "15 秒");
    case 30: return TXT("30 sec", "30 秒");
    case 120: return TXT("2 min", "2 分钟");
    case 300: return TXT("5 min", "5 分钟");
    case 0: return TXT("Never", "永不");
    default: return TXT("1 min", "1 分钟");
  }
}
static void settings_value(int index, char *out, size_t capacity) {
  switch (index) {
    case SET_WIFI: snprintf(out, capacity, "%s", ps_wifi_online() ? TXT("On", "开启") : TXT("Off", "关闭")); break;
    case SET_BLUETOOTH: snprintf(out, capacity, "%s", ps_hid_state()==PS_HID_UNAVAILABLE ? TXT("Unavailable", "不可用") : ps_hid_state()==PS_HID_READY ? TXT("Connected", "已连接") : TXT("On demand", "按需启用")); break;
    case SET_DISPLAY: snprintf(out, capacity, "%u%%", settings_brightness); break;
    case SET_APPEARANCE: snprintf(out, capacity, "%s", display_mode_name()); break;
    case SET_TIMEOUT: snprintf(out, capacity, "%s", timeout_label()); break;
    case SET_SOUND: snprintf(out, capacity, "%u%% / %u%%", ps_audio_volume(false), ps_audio_volume(true)); break;
    case SET_PRIVACY: snprintf(out, capacity, "%s", TXT("Per-app", "按应用")); break;
    case SET_DATETIME: {
      struct tm local;
      if (system_time_valid(&local))
        strftime(out, capacity, "%H:%M", &local);
      else
        snprintf(out, capacity, "%s", TXT("Syncing", "同步中"));
      break;
    }
    case SET_ALARM:
      snprintf(out, capacity, "%02u:%02u %s", alarm_config.hour,
               alarm_config.minute,
               alarm_config.enabled ? TXT("On", "开启") : TXT("Off", "关闭"));
      break;
    case SET_UPDATE: {
      ps_update_info u;
      ps_update_read(&u);
      if (!update_url[0]) snprintf(out, capacity, "%s", TXT("Setup", "设置"));
      else switch (u.state) {
        case PS_UPDATE_CHECKING: snprintf(out, capacity, "%s", TXT("Checking", "检查中")); break;
        case PS_UPDATE_CURRENT: snprintf(out, capacity, "%s", TXT("Current", "最新")); break;
        case PS_UPDATE_AVAILABLE: snprintf(out, capacity, "%s", TXT("Available", "可更新")); break;
        case PS_UPDATE_DOWNLOADING:
          snprintf(out, capacity, "%u%%", u.image_size
                       ? (unsigned)(u.downloaded * 100 / u.image_size) : 0);
          break;
        case PS_UPDATE_READY: snprintf(out, capacity, "%s", TXT("Ready", "就绪")); break;
        case PS_UPDATE_INSTALLING: snprintf(out, capacity, "%s", TXT("Restarting", "重启中")); break;
        case PS_UPDATE_ERROR: snprintf(out, capacity, "%s", TXT("Error", "错误")); break;
        default: snprintf(out, capacity, "%s", TXT("Check", "检查")); break;
      }
      break;
    }
    case SET_LANGUAGE: snprintf(out, capacity, "%s", language_name()); break;
    case SET_STORAGE: {
      snprintf(out, capacity, TXT("%d apps", "%d 个应用"), settings_installed_count);
      break;
    }
    default:
      if (capacity) out[0] = 0;
      break;
  }
}
static void measure(const char *phase) {
  printf("PASSPORT BLE state=%u\n",ps_hid_state());
  ESP_LOGI("passport_native", "METRIC %s pool_capacity=%u pool_used=%u pool_peak=%u protection=%d", phase,
           (unsigned)native_exec_capacity(), (unsigned)native_exec_used(), (unsigned)native_exec_peak(), native_exec_verify());
  ESP_LOGI(
      "passport",
      "METRIC %s free=%u minimum=%u largest=%u internal_free=%u internal_largest=%u retention_free=%u retention_largest=%u stack=%u lua=%u lua_peak=%u",
      phase, (unsigned)esp_get_free_heap_size(),
      (unsigned)esp_get_minimum_free_heap_size(),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_RETENTION | MALLOC_CAP_8BIT),
      (unsigned)uxTaskGetStackHighWaterMark(NULL), ps_runtime_view()->lua_used,
      ps_runtime_view()->lua_peak);
}
static void paint(lv_event_t *e) {
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_obj_get_coords(lv_event_get_target(e), &area);
  for (unsigned i = 0; i < display_view.count; i++) {
    ps_draw *c = &display_view.commands[i];
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(c->color);
    d.bg_opa = LV_OPA_COVER;
    d.radius = 0;
    lv_area_t r = {area.x1 + c->x, area.y1 + c->y, area.x1 + c->x + c->w - 1,
                   area.y1 + c->y + c->h - 1};
    lv_draw_rect(layer, &d, &r);
  }
  for (unsigned i = 0; i < display_view.image_count; i++) {
    const ps_image_command *c = &display_view.images[i];
    int x = area.x1 + c->x, y = area.y1 + c->y;
    if (c->system)
      ps_icon_draw(layer, c->handle, x, y, c->size, c->color);
    else {
      const lv_image_dsc_t *image = ps_asset_lvgl(c->handle);
      if (image) {
        lv_draw_image_dsc_t d;
        lv_draw_image_dsc_init(&d);
        d.src = image;
        if (image->header.cf == LV_COLOR_FORMAT_A1 ||
            image->header.cf == LV_COLOR_FORMAT_A4) {
          d.recolor = lv_color_hex(c->color);
          d.recolor_opa = LV_OPA_COVER;
        }
        lv_area_t r = {x, y, x + image->header.w - 1, y + image->header.h - 1};
        lv_draw_image(layer, &d, &r);
      } else
        ps_icon_draw(layer, 0, x, y, 24, 0xe9f0fa);
    }
  }
}
/* One native draw object: four cards share existing fonts/icons, no image cache. */
static void paint_home(lv_event_t *e) {
  static const char *names[][2] = {{"Apps", "应用"}, {"Store", "商店"},
                                   {"Settings", "设置"}};
  static const char *hints[][2] = {{"Play & tools", "应用与工具"},
                                   {"Get apps", "发现应用"},
                                   {"Your device", "设备设置"}};
  static const char *icons[] = {"system.gamepad", "system.download",
                                "system.settings"};
  static const uint32_t accents[] = {0x6ce3c2, 0x79baff, 0xe1b3ff};
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t origin;
  lv_obj_get_coords(home_tiles, &origin);
  for (int i = 0; i < 3; i++) {
    int x = origin.x1 + (i == 2 ? 57 : (i % 2) * 114);
    int y = origin.y1 + (i / 2) * 106;
    lv_area_t visible={x,y,x+101,y+93};
    if(!paint_visible(layer,&visible))continue;
    bool active = home_focus == i;
    lv_draw_rect_dsc_t card;
    lv_draw_rect_dsc_init(&card);
    card.radius = 14;
    card.bg_opa = LV_OPA_COVER;
    card.bg_color = lv_color_hex(active
        ? theme_rgb(0x203b4c, 0xd9eaf6) : theme_rgb(0x192638, 0xffffff));
    card.border_width = active ? 2 : 1;
    card.border_opa = LV_OPA_COVER;
    card.border_color = lv_color_hex(active ? accents[i]
                                             : theme_rgb(0x2a3a50, 0xd5dee8));
    lv_area_t bounds = {x, y, x + 101, y + 93};
    lv_draw_rect(layer, &card, &bounds);
    lv_draw_rect_dsc_t badge;
    lv_draw_rect_dsc_init(&badge);
    badge.radius = 11;
    badge.bg_opa = LV_OPA_COVER;
    badge.bg_color = lv_color_hex(active
        ? theme_rgb(0x315264, 0xc5dff0) : theme_rgb(0x24364c, 0xecf1f6));
    lv_area_t icon_bounds = {x + 32, y + 9, x + 69, y + 44};
    lv_draw_rect(layer, &badge, &icon_bounds);
    ps_icon_draw(layer, ps_icon_index(icons[i]), x + 40, y + 15, 24, accents[i]);
    if (active)
      ps_icon_draw(layer, ps_icon_index("system.check"), x + 81, y + 9, 14,
                   accents[i]);
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font = system_font();
    label.align = LV_TEXT_ALIGN_CENTER;
    label.color = lv_color_hex(theme_rgb(0xe9f0fa, 0x172334));
    label.text = TXT(names[i][0], names[i][1]);
    lv_area_t name_bounds = {x + 3, y + 51, x + 98, y + 69};
    lv_draw_label(layer, &label, &name_bounds);
    label.text = TXT(hints[i][0], hints[i][1]);
    label.color = lv_color_hex(theme_rgb(0x8eabc1, 0x52647a));
    lv_area_t hint_bounds = {x + 3, y + 72, x + 98, y + 89};
    lv_draw_label(layer, &label, &hint_bounds);
  }
}
static void paint_cards(lv_event_t *e) {
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t origin;
  lv_obj_get_coords(cards_view, &origin);
  for (int i = 0; i < 4; i++) {
    int index = cards_base + i;
    if (index >= cards_count) continue;
    int x = origin.x1 + (i % 2) * 114;
    int y = origin.y1 + (i / 2) * 96;
    lv_area_t visible={x,y,x+101,y+87};
    if(!paint_visible(layer,&visible))continue;
    bool active = index == cards_focus;
    uint32_t accent = (uint32_t[]){0x6ce3c2, 0x79baff, 0xe1b3ff, 0xffca80}[index % 4];
    lv_draw_rect_dsc_t card;
    lv_draw_rect_dsc_init(&card);
    card.radius = 12; card.bg_opa = LV_OPA_COVER;
    card.bg_color = lv_color_hex(active
        ? theme_rgb(0x203b4c, 0xd9eaf6) : theme_rgb(0x192638, 0xffffff));
    card.border_width = active ? 2 : 1; card.border_opa = LV_OPA_COVER;
    card.border_color = lv_color_hex(active ? accent
                                             : theme_rgb(0x2a3a50, 0xd5dee8));
    lv_area_t bounds = {x, y, x + 101, y + 87};
    lv_draw_rect(layer, &card, &bounds);
    lv_draw_rect_dsc_t badge;
    lv_draw_rect_dsc_init(&badge); badge.radius = 10;
    badge.bg_opa = LV_OPA_COVER;
    badge.bg_color = lv_color_hex(active
        ? theme_rgb(0x315264, 0xc5dff0) : theme_rgb(0x24364c, 0xecf1f6));
    lv_area_t icon_bounds = {x + 5, y + 7, x + 39, y + 41};
    lv_draw_rect(layer, &badge, &icon_bounds);
    ps_icon_draw(layer, cards_icons[i], x + 10, y + 12, 24, accent);
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label); label.font = system_font();
    label.align = LV_TEXT_ALIGN_LEFT;
    label.color = lv_color_hex(theme_rgb(0xe9f0fa, 0x172334));
    label.text = cards_labels[i];
    lv_area_t name = {x + 44, y + 7, x + 98, y + 43};
    lv_draw_label(layer, &label, &name);
    label.align = LV_TEXT_ALIGN_CENTER;
    label.color = lv_color_hex(theme_rgb(0x8eabc1, 0x52647a));
    label.text = cards_versions[i];
    lv_area_t hint = {x + 3, y + 49, x + 98, y + 84};
    lv_draw_label(layer, &label, &hint);
    if (active) ps_icon_draw(layer, ps_icon_index("system.check"), x + 82, y + 7, 14, accent);
  }
}
static void paint_storage(lv_event_t *e) {
  lv_area_t origin;lv_obj_get_coords(storage_bar,&origin);
  lv_layer_t *layer=lv_event_get_layer(e);
  lv_draw_rect_dsc_t bar;lv_draw_rect_dsc_init(&bar);
  bar.bg_opa=LV_OPA_COVER;bar.radius=4;bar.bg_color=lv_color_hex(0x405064);
  lv_draw_rect(layer,&bar,&origin);
  if(storage_usage.layout_valid) {
    unsigned start=0;
    for(unsigned i=0;i<PS_STORAGE_PARTS;i++) {
      unsigned end=ps_storage_layout_edge(&storage_usage.layout,i+1,216);
      if(end>start){lv_area_t segment=origin;segment.x1+=start;segment.x2=origin.x1+end-1;bar.bg_color=lv_color_hex(storage_colors[i]);lv_draw_rect(layer,&bar,&segment);}
      start=end;
    }
  }
}
static void paint_storage_legend(lv_event_t *e) {
  lv_area_t origin;lv_obj_get_coords(storage_legend,&origin);
  unsigned line=system_font()->line_height+lv_obj_get_style_text_line_space(body,0);
  lv_draw_rect_dsc_t square;lv_draw_rect_dsc_init(&square);square.radius=2;square.bg_opa=LV_OPA_COVER;
  for(unsigned i=0;i<PS_STORAGE_PARTS;i++) {
    lv_area_t bounds={origin.x1,origin.y1+(int)(i*line)+(int)line/2-4,origin.x1+7,origin.y1+(int)(i*line)+(int)line/2+3};
    square.bg_color=lv_color_hex(storage_colors[i]);lv_draw_rect(lv_event_get_layer(e),&square,&bounds);
  }
}
static void paint_settings(lv_event_t *e) {
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t origin;
  lv_obj_get_coords(settings_view, &origin);
  settings_scroll_base = ps_settings_scroll_base(
      SET_COUNT, (unsigned)settings_selection, SETTINGS_VISIBLE_ROWS,
      settings_scroll_base);
  int y = 2;
  unsigned end = settings_scroll_base + SETTINGS_VISIBLE_ROWS;
  if (end > SET_COUNT) end = SET_COUNT;
  for (unsigned i = settings_scroll_base; i < end; i++) {
    if (i == SET_DISPLAY || i == SET_APPEARANCE || i == SET_DATETIME ||
        i == SET_LANGUAGE) y += 2;
    bool active = i == settings_selection;
    lv_draw_rect_dsc_t row;
    lv_draw_rect_dsc_init(&row);
    row.radius = (i == SET_WIFI || i == SET_BLUETOOTH || i == SET_TIMEOUT ||
                  i == SET_PRIVACY || i == SET_APPEARANCE || i == SET_DATETIME ||
                  i == SET_LANGUAGE || i == SET_STORAGE ||
                  i == SET_ABOUT) ? 6 : 0;
    row.bg_opa = LV_OPA_COVER;
    row.bg_color = lv_color_hex(active
        ? theme_rgb(0x263e52, 0xd9eaf6) : theme_rgb(0x1a2838, 0xffffff));
    lv_area_t bounds = {origin.x1, origin.y1 + y, origin.x1 + 207,
                        origin.y1 + y + 20};
    if(!paint_visible(layer,&bounds)){y+=22;continue;}
    lv_draw_rect(layer, &row, &bounds);
    lv_draw_rect_dsc_t badge;
    lv_draw_rect_dsc_init(&badge);
    badge.radius = 5;
    badge.bg_opa = LV_OPA_COVER;
    badge.bg_color = lv_color_hex(setting_colors[i]);
    lv_area_t icon = {origin.x1 + 6, origin.y1 + y + 2,
                      origin.x1 + 20, origin.y1 + y + 16};
    lv_draw_rect(layer, &badge, &icon);
    ps_icon_draw(layer, ps_icon_index(setting_icons[i]), origin.x1 + 6,
                 origin.y1 + y + 2, 14, 0xffffff);
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font = system_font();
    label.color = lv_color_hex(theme_rgb(0xe9f0fa, 0x172334));
    label.text = TXT(setting_names[i][0], setting_names[i][1]);
    lv_area_t name = {origin.x1 + 28, origin.y1 + y,
                      origin.x1 + 143, origin.y1 + y + 20};
    lv_draw_label(layer, &label, &name);
    char value[24];
    settings_value(i, value, sizeof(value));
    label.color = lv_color_hex(i == SET_BLUETOOTH && ps_hid_state()==PS_HID_UNAVAILABLE
                                   ? theme_rgb(0x8290a0, 0x8290a0)
                                   : theme_rgb(0x9aadc0, 0x52647a));
    label.text = value;
    label.align = LV_TEXT_ALIGN_RIGHT;
    lv_area_t detail = {origin.x1 + 141, origin.y1 + y,
                        origin.x1 + 193, origin.y1 + y + 20};
    lv_draw_label(layer, &label, &detail);
    label.text = "\xef\x81\x94";
    label.color = lv_color_hex(active ? 0x3979c6 : 0x71849a);
    lv_area_t chevron = {origin.x1 + 191, origin.y1 + y,
                         origin.x1 + 204, origin.y1 + y + 20};
    lv_draw_label(layer, &label, &chevron);
    y += 22;
  }
  if (SET_COUNT > SETTINGS_VISIBLE_ROWS) {
    const int track_y = origin.y1 + 2;
    const int track_height = 178;
    const int thumb_height = track_height * SETTINGS_VISIBLE_ROWS / SET_COUNT;
    const int max_base = SET_COUNT - SETTINGS_VISIBLE_ROWS;
    int thumb_y = track_y + (track_height - thumb_height) *
                                 (int)settings_scroll_base / max_base;
    lv_draw_rect_dsc_t track;
    lv_draw_rect_dsc_init(&track);
    track.radius = 2;
    track.bg_opa = LV_OPA_COVER;
    track.bg_color = lv_color_hex(theme_rgb(0x263446, 0xdbe2e9));
    lv_area_t track_area = {origin.x1 + 211, track_y,
                            origin.x1 + 214, track_y + track_height};
    lv_draw_rect(layer, &track, &track_area);
    track.bg_color = lv_color_hex(0x79baff);
    lv_area_t thumb_area = {origin.x1 + 211, thumb_y,
                            origin.x1 + 214, thumb_y + thumb_height};
    lv_draw_rect(layer, &track, &thumb_area);
  }
}
static void close_app(void) {
  if (app_active) {
    ps_session_end(&session);
    if (frame_count)
      ESP_LOGI("passport",
               "GAME frames=%u elapsed_us=%lld max_callback_us=%lld",
               frame_count, (long long)(esp_timer_get_time() - frame_start),
               (long long)frame_max);
    app_active = false;
    measure("exit");
  }
  mic_prompt = false;
  memset(app_button_down, 0, sizeof(app_button_down));
}
static bool open_app(const char *id) {
  close_app();
  reset_preview();
  int64_t t = esp_timer_get_time();
  if (!storage_ready || !ps_store_load(id, &package)) {
    snprintf(notice, sizeof(notice), "Package unavailable / invalid");
    return false;
  }
  ps_session_begin(&session);
  ps_session_own(&session, &package, release_package);
  ps_session_own(&session, NULL, release_runtime);
  app_active = true;
  frame_count = 0;
  frame_start = esp_timer_get_time();
  frame_max = 0;
  cJSON *limits = cJSON_GetObjectItemCaseSensitive(package.manifest, "limits");
  frame_period_us =
      1000000 / cJSON_GetObjectItemCaseSensitive(limits, "fps")->valueint;
  ps_runtime_set_system_display_mode(system_light_mode ? "light" : "dark");
  bool ok = ps_runtime_start(&package);
  app_tick_time = esp_timer_get_time();
  mic_prompt = (package.permissions & (PS_AUDIO | PS_AUDIO_STREAM)) != 0;
  if (!ok)
    snprintf(notice, sizeof(notice), "%s", ps_runtime_error());
  ESP_LOGI("passport", "APP start id=%s ok=%d us=%lld", id, ok,
           (long long)(esp_timer_get_time() - t));
  measure("runtime");
  return ok;
}
// Re-entering Store must never offer installs from a stale catalog.
static void refresh_store_catalog(void) {
  cJSON_Delete(catalog);
  catalog = NULL;
  catalog_selection = 0;
  store_installed_id[0] = 0;
  if (store_request) {
    snprintf(notice, sizeof(notice), "Loading Store...");
    return;
  }
  if (!service_approved && !*store_url) {
    service_pending = true;
    notice[0] = 0;
    return;
  }
  char url[512];
  if (*store_url)
    snprintf(url, sizeof(url), "%s", store_url);
  else
    snprintf(url, sizeof(url), "http://%s:%u/index.json", service_ip,
             service_http);
  store_request = 1;
  bool ok = ps_net_fetch(NULL, url, 0xffffffff, true);
  if (!ok) store_request = 0;
  snprintf(notice, sizeof(notice), "%s",
           ok ? "Loading Store..." : "Offline / busy / endpoint denied");
}

static bool installed_app_is_current(const char *id, const char *catalog_version) {
  ps_package installed = {0};
  if (!id || !catalog_version || !ps_store_load(id, &installed))
    return false;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(installed.manifest, "version");
  bool current = cJSON_IsString(version) &&
                 ps_app_version_compare(version->valuestring,
                                        catalog_version) >= 0;
  ps_package_free(&installed);
  return current;
}
static void app_version_size(const char *version, size_t bytes, char *out, size_t capacity) {
  if (bytes)
    snprintf(out, capacity, "v%.14s\n%u.%u KiB", version ? version : "—",
             (unsigned)(bytes / 1024), (unsigned)((bytes % 1024) * 10 / 1024));
  else
    snprintf(out, capacity, "v%.14s\n-- KiB", version ? version : "—");
}
static void installed_app_version(const char *id, char *out, size_t capacity) {
  app_version_size(NULL, 0, out, capacity);
  ps_package installed = {0};
  if (!id || !ps_store_load(id, &installed)) return;
  cJSON *version = cJSON_GetObjectItemCaseSensitive(installed.manifest, "version");
  app_version_size(cJSON_IsString(version) ? version->valuestring : NULL,
                   ps_store_package_size(id), out, capacity);
  ps_package_free(&installed);
}
static void refresh_app_list(void) {
  app_count = storage_ready ? ps_store_registered(ids, 12) : 0;
  memset(library_versions, 0, sizeof(library_versions));
  for (int i = 0; i < app_count; i++)
    installed_app_version(ids[i], library_versions[i],
                         sizeof(library_versions[i]));
}
static void library_app_version(int index, char *out, size_t capacity) {
  if (index >= 0 && index < app_count) {
    snprintf(out, capacity, "%s",
             library_versions[index][0] ? library_versions[index] : "v—");
  } else {
    snprintf(out, capacity, "v—");
  }
}
/* LVGL QR widget owns its draw buffer. Put only pixel storage on the system
 * heap to preserve the bounded 16 KiB LVGL pool; delete reclaims both owners. */
static void *wifi_qr_alloc(size_t bytes,lv_color_format_t format) {
  (void)format;return malloc(bytes);
}
static bool wifi_qr_update(const char *payload) {
  if(!wifi_qr) {
    wifi_qr_handlers=*lv_draw_buf_get_handlers();
    wifi_qr_handlers.buf_malloc_cb=wifi_qr_alloc;wifi_qr_handlers.buf_free_cb=free;
    lv_draw_buf_t *buffer=lv_draw_buf_create_ex(&wifi_qr_handlers,176,176,LV_COLOR_FORMAT_I1,LV_STRIDE_AUTO);
    if(!buffer)return false;
    wifi_qr=lv_qrcode_create(lv_screen_active());
    if(!wifi_qr){lv_draw_buf_destroy(buffer);return false;}
    lv_draw_buf_t *old=lv_canvas_get_draw_buf(wifi_qr);
    lv_canvas_set_draw_buf(wifi_qr,buffer);if(old)lv_draw_buf_destroy(old);
    lv_obj_set_pos(wifi_qr,32,82);
    lv_obj_set_style_outline_width(wifi_qr,12,0);
    lv_obj_set_style_outline_pad(wifi_qr,0,0);
    lv_obj_set_style_outline_color(wifi_qr,lv_color_white(),0);
    lv_obj_set_style_outline_opa(wifi_qr,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(wifi_qr,0,0);lv_obj_set_style_pad_all(wifi_qr,0,0);
    lv_qrcode_set_dark_color(wifi_qr,lv_color_black());
    lv_qrcode_set_light_color(wifi_qr,lv_color_white());
    lv_qrcode_set_quiet_zone(wifi_qr,true);
  }
  return lv_qrcode_update(wifi_qr,payload,strlen(payload))==LV_RESULT_OK;
}
static void draw(void) {
  static int battery = -1;
  static int64_t battery_time;
  static ps_language_t applied_language = PS_LANGUAGE_COUNT;
  int64_t clock_now = esp_timer_get_time();
  if (clock_now - battery_time > 1000000) {
    battery = bsp_battery_soc();
    battery_time = clock_now;
  }
  bool preview = false, show_list_cards = false;
  if (!app_active && page == 1 && focus == 2) {
    char installed[12][64];
    settings_installed_count =
        storage_ready ? ps_store_registered(installed, 12) : 0;
  }
  const char *selected = NULL;
  card_kind_t staged_kind = CARDS_NONE;
  int staged_focus = 0, staged_base = 0, staged_count = 0;
  char staged_labels[4][40] = {{0}}; unsigned staged_icons[4] = {0};
  char staged_versions[4][40] = {{0}};
  if (!app_active && page == 1 && !*notice && focus == 0 && app_count > 0) {
    staged_kind = CARDS_LIBRARY; staged_count = app_count; staged_focus = selection;
    show_list_cards = true;
  } else if (!app_active && page == 1 && !*notice && focus == 1 && catalog) {
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(catalog, "apps");
    if (cJSON_IsArray(entries) && cJSON_GetArraySize(entries) > 0) {
      staged_kind = CARDS_STORE; staged_count = cJSON_GetArraySize(entries);
      staged_focus = catalog_selection; show_list_cards = true;
    }
  }
  if (show_list_cards) {
    staged_base = (staged_focus / 4) * 4;
    for (int i = 0; i < 4; i++) {
      int idx = staged_base + i; staged_icons[i] = ps_icon_index("system.app-default");
      staged_labels[i][0] = 0;
      staged_versions[i][0] = 0;
      if (idx >= staged_count) continue;
      if (staged_kind == CARDS_LIBRARY) {
        const char *id = ids[idx], *tail = strrchr(id, '.');
        snprintf(staged_labels[i], sizeof(staged_labels[i]), "%.16s", tail ? tail + 1 : id);
        library_app_version(idx, staged_versions[i], sizeof(staged_versions[i]));
        char icon[65];
        if (ps_store_icon(id, icon) && !strncmp(icon, "system.", 7)) staged_icons[i] = ps_icon_index(icon);
      } else {
        cJSON *entry = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(catalog, "apps"), idx);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
        cJSON *version = cJSON_GetObjectItemCaseSensitive(entry, "version");
        cJSON *icon = cJSON_GetObjectItemCaseSensitive(entry, "icon");
        snprintf(staged_labels[i], sizeof(staged_labels[i]), "%.20s", cJSON_IsString(name) ? name->valuestring : cJSON_IsString(id) ? id->valuestring : "App");
        cJSON *size = cJSON_GetObjectItemCaseSensitive(entry, "size");
        size_t bytes = cJSON_IsNumber(size) && size->valuedouble > 0 && size->valuedouble <= PS_PAP_MAX
                           ? (size_t)size->valuedouble : 0;
        app_version_size(cJSON_IsString(version) ? version->valuestring : NULL, bytes,
                         staged_versions[i], sizeof(staged_versions[i]));
        if (cJSON_IsString(icon) && !strncmp(icon->valuestring, "system.", 7)) staged_icons[i] = ps_icon_index(icon->valuestring);
      }
    }
  }
  if (!app_active && page == 1 && !*notice && !show_list_cards) {
    if (focus == 0 && selection < app_count)
      selected = ids[selection];
    else if (focus == 1) {
      cJSON *apps = cJSON_GetObjectItemCaseSensitive(catalog, "apps"),
            *entry = cJSON_GetArrayItem(apps, catalog_selection),
            *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
      if (cJSON_IsString(id))
        selected = id->valuestring;
    }
  }
  if (selected && !show_list_cards) {
    preview = true;
    if (strcmp(preview_id, selected) ||
        (!preview_image.system && !ps_asset_get(preview_image.handle, NULL))) {
      snprintf(preview_id, sizeof(preview_id), "%s", selected);
      preview_image = (ps_image_command){.system = 1,
                                         .handle = 0,
                                         .x = 168,
                                         .y = 0,
                                         .size = 24,
                                         .color = 0x68d5e8};
      char icon[65];
      ps_store_icon(selected, icon);
      if (!strncmp(icon, "system.", 7))
        preview_image.handle = ps_icon_index(icon);
      else if (!strncmp(icon, "app://", 6)) {
        ps_package metadata;
        if (ps_store_load(selected, &metadata)) {
          uint32_t token = ps_asset_open(&metadata, icon);
          ps_asset_info image;
          if (token && ps_asset_get(token, &image) && image.width <= 48 &&
              image.height <= 48) {
            preview_image.system = 0;
            preview_image.handle = token;
          }
          ps_package_free(&metadata);
        }
      }
    }
  }
  uint32_t lock_start=(uint32_t)esp_timer_get_time();
  if (!bsp_lvgl_lock(1000)) return;
  metric_max(&lvgl_lock_max_us,(uint32_t)esp_timer_get_time()-lock_start);
  bool show_wifi_qr=!app_active && page==1 && focus==2 && wifi_setup_page && !ps_wifi_setup_waiting_hotspot() && wifi_setup_step<2 && !*notice;
  if(!show_wifi_qr && wifi_qr){lv_obj_delete(wifi_qr);wifi_qr=NULL;wifi_qr_step=-1;}
  if(show_wifi_qr && wifi_qr_step!=(int)wifi_setup_step) {
    char payload[256];bool ok=wifi_setup_step==1;
    if(ok)snprintf(payload,sizeof(payload),"http://192.168.4.1/");
    else ok=ps_wifi_setup_qr_payload(wifi_setup_ssid,wifi_setup_password,payload,sizeof(payload));
    if(ok)ok=wifi_qr_update(payload);
    memset(payload,0,sizeof(payload));
    if(ok)wifi_qr_step=(int)wifi_setup_step;
    else {wifi_setup_step=2;show_wifi_qr=false;if(wifi_qr){lv_obj_delete(wifi_qr);wifi_qr=NULL;}wifi_qr_step=-1;}
  }
  static int applied_theme=-1;
  bool theme_changed=applied_theme!=(int)system_light_mode;
  bool language_changed=applied_language!=system_language;
  bool home_was_visible=!lv_obj_has_flag(home_tiles,LV_OBJ_FLAG_HIDDEN);
  int previous_home_focus=home_focus;
  if(theme_changed) {
  lv_obj_set_style_bg_color(lv_screen_active(),
                            lv_color_hex(theme_rgb(0x101a29, 0xf5f7fa)), 0);
  lv_obj_set_style_text_color(lv_screen_active(),
                              lv_color_hex(theme_rgb(0xe9f0fa, 0x172334)), 0);
  lv_obj_set_style_bg_color(canvas,
                            lv_color_hex(theme_rgb(0x101a29, 0xf5f7fa)), 0);
    applied_theme=system_light_mode;
  }
  if (applied_language != system_language) {
    const lv_font_t *font = system_font();
    lv_obj_set_style_text_font(status, font, 0);
    lv_obj_set_style_text_font(title, system_language == PS_LANGUAGE_ZH_CN
                                         ? font : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(body, font, 0);
    lv_obj_set_style_text_font(footer, font, 0);
    lv_obj_set_style_text_font(app_body, multilingual_font(), 0);
    applied_language = system_language;
  }
  cards_kind = staged_kind; cards_count = staged_count; cards_base = staged_base;
  cards_focus = staged_focus;
  memcpy(cards_labels, staged_labels, sizeof(cards_labels));
  memcpy(cards_versions, staged_versions, sizeof(cards_versions));
  memcpy(cards_icons, staged_icons, sizeof(cards_icons));
  home_focus = focus;
  bool show_home = !page && !app_active && !publisher_pending && !service_pending;
  bool show_settings_list = !app_active && page == 1 && focus == 2 &&
                            !settings_detail && !wifi_setup_page && !*notice;
  bool fullscreen = app_active && !mic_prompt && ps_runtime_display()->mode == PS_DISPLAY_FULLSCREEN;
  if (app_active) {
    lv_obj_set_pos(app_host, 0, fullscreen ? 0 : 24);
    lv_obj_set_size(app_host, 240, fullscreen ? 320 : 272);
    lv_obj_add_flag(home_tiles, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(cards_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    if(mic_prompt || *ps_runtime_error())lv_obj_add_flag(app_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(app_canvas, 0, 0); lv_obj_set_size(app_canvas, 240, fullscreen ? 320 : 272);
    lv_obj_set_pos(app_body, 8, 8); lv_obj_set_width(app_body, 224);
    lv_obj_set_height(app_body, fullscreen ? 304 : 256);
    lv_obj_remove_flag(app_host, LV_OBJ_FLAG_HIDDEN);
    cJSON *name = cJSON_GetObjectItemCaseSensitive(package.manifest, "name");
    char app_status[64];
    snprintf(app_status,sizeof(app_status),"%.12s / %s",cJSON_IsString(name)?name->valuestring:package.id,ps_wifi_online()?LV_SYMBOL_WIFI:TXT("Offline","离线"));
    set_label_text(status,app_status);
  } else {
    lv_obj_add_flag(app_host, LV_OBJ_FLAG_HIDDEN);
    if (show_settings_list) {
      lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(home_tiles, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(cards_view, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(settings_view, LV_OBJ_FLAG_HIDDEN); lv_obj_invalidate(settings_view);
    } else if (show_list_cards) {
      lv_obj_add_flag(settings_view, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(title, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(home_tiles, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(cards_view, LV_OBJ_FLAG_HIDDEN); lv_obj_invalidate(cards_view);
    } else {
      lv_obj_add_flag(settings_view, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(cards_view, LV_OBJ_FLAG_HIDDEN);
      if (!show_home) { lv_obj_remove_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(body, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(home_tiles, LV_OBJ_FLAG_HIDDEN); }
    }
  }
  lv_obj_set_style_text_font(body, focus == 2 && settings_detail && settings_selection == SET_WIFI ? multilingual_font() : system_font(), 0);
  bool show_storage=page==1 && focus==2 && settings_detail && settings_selection==SET_STORAGE && storage_usage_page==0 && storage_usage_valid && !app_active;
  if(show_storage){lv_obj_remove_flag(storage_bar,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(storage_bar);}
  else lv_obj_add_flag(storage_bar,LV_OBJ_FLAG_HIDDEN);
  if(show_storage && storage_usage.layout_valid) {
    unsigned line=system_font()->line_height+lv_obj_get_style_text_line_space(body,0);
    lv_obj_set_pos(storage_legend,10,78+2*line);lv_obj_set_size(storage_legend,8,PS_STORAGE_PARTS*line);
    lv_obj_remove_flag(storage_legend,LV_OBJ_FLAG_HIDDEN);lv_obj_invalidate(storage_legend);
  } else lv_obj_add_flag(storage_legend,LV_OBJ_FLAG_HIDDEN);
  if (fullscreen) { lv_obj_add_flag(status, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(footer, LV_OBJ_FLAG_HIDDEN); }
  else { lv_obj_remove_flag(status, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(battery_label, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(footer, LV_OBJ_FLAG_HIDDEN); }
  if (fullscreen) lv_obj_add_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(clock_label, LV_OBJ_FLAG_HIDDEN);
  if (!app_active) set_label_text(status, ps_wifi_online() ? LV_SYMBOL_WIFI : TXT("Offline", "离线"));
  char clock_text[8];
  format_clock(clock_text, sizeof(clock_text), "%H:%M", "--:--");
  set_label_text(clock_label, clock_text);
  if (battery < 0)
    set_label_text(battery_label, "--%");
  else
    {char level[12];snprintf(level,sizeof(level),"%d%%",battery);set_label_text(battery_label,level);}
  if (show_home) {
    lv_obj_remove_flag(home_tiles, LV_OBJ_FLAG_HIDDEN);
    if(!home_was_visible || theme_changed || language_changed)lv_obj_invalidate(home_tiles);
    else if(previous_home_focus!=home_focus) {
      lv_area_t origin;lv_obj_get_coords(home_tiles,&origin);
      int selections[2]={previous_home_focus,home_focus};
      for(unsigned j=0;j<2;j++){
        int i=selections[j],x=origin.x1+(i==2?57:(i%2)*114),y=origin.y1+(i/2)*106;
        lv_area_t changed={x,y,x+101,y+93};lv_obj_invalidate_area(home_tiles,&changed);
      }
    }
  } else
    lv_obj_add_flag(home_tiles, LV_OBJ_FLAG_HIDDEN);
  char text[600] = {0};
  lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
  if (page == 2 && focus == 0 && !app_active) {
    set_label_text(title, TXT("App actions", "应用操作"));
    const char *default_name = strrchr(default_app, '.');
    snprintf(text, sizeof(text), TXT("Current default: %s\n\n", "当前默认应用：%s\n\n"),
             default_app[0] ? (default_name ? default_name + 1 : default_app)
                            : TXT("None", "无"));
    const char *actions[][2] = {
        {"Open", "打开"},
        {!strcmp(default_app, ids[selection]) ? "Clear default app" : "Set as default app",
         !strcmp(default_app, ids[selection]) ? "清除默认应用" : "设为默认应用"},
        {"Offload code", "卸载应用代码"}, {"Toggle offline pin", "切换离线保留"},
        {"Delete app and data", "删除应用和数据"}};
    for (int i = 0; i < 5; i++) {
      char row[80];
      snprintf(row, sizeof(row), "%s %s\n\n", i == action_index ? ">" : " ",
               TXT(actions[i][0], actions[i][1]));
      strlcat(text, row, sizeof(text));
    }
  } else if (page == 3 && focus == 0 && !app_active) {
    set_label_text(title, TXT("Delete all app data?", "删除此应用的全部数据？"));
    snprintf(text, sizeof(text),
             TXT("%.50s\n\nThis deletes its saved data.\n\nOK: permanently delete\nUP/DOWN: cancel",
                 "%.50s\n\n这会删除应用保存的数据。\n\nOK：确认删除\nUP/DOWN：取消"),
             ids[selection]);
  } else if (publisher_pending) {
    set_label_text(title, TXT("Trust publisher?", "信任此发布者？"));
    snprintf(text, sizeof(text),
             TXT("Key fingerprint ID:\n%s\n\nOnly approve a key from\na developer you trust.\n\nOK: trust / UP,DOWN: deny",
                 "密钥指纹 ID：\n%s\n\n仅信任你认可的开发者密钥。\n\nOK：信任 / UP、DOWN：拒绝"),
             pending_key_id);
  } else if (service_pending) {
    set_label_text(title, TXT("Approve local service", "批准本地服务"));
    snprintf(text, sizeof(text),
             TXT("%s\nHTTP %u / WS %u\n\nOK: approve this endpoint\nUP/DOWN: deny",
                 "%s\nHTTP %u / WS %u\n\nOK：批准此服务\nUP/DOWN：拒绝"),
             service_ip, service_http, service_ws);
  } else if (app_active) {
    if(mic_prompt || *ps_runtime_error())lv_obj_set_style_text_color(app_body,
                                lv_color_hex(theme_rgb(0xe9f0fa, 0x172334)), 0);
    set_label_text(title, mic_prompt ? TXT("Microphone access", "麦克风权限")
                             : strrchr(package.id, '.')
                                 ? strrchr(package.id, '.') + 1
                                 : package.id);
    if (mic_prompt) {
      bool streams_audio = (package.permissions & PS_AUDIO_STREAM) != 0;
      snprintf(text, sizeof(text), "%s", streams_audio
               ? TXT("Allow microphone streaming?\nThis app may send audio to its approved network service while capture is active.\n\nOK: allow\nUP / DOWN: deny",
                     "允许麦克风音频流？\n采集期间，此应用可将音频发送到其获准的网络服务。\n\nOK：允许\nUP / DOWN：拒绝")
               : TXT("Allow microphone features?\nAudio stays on this device.\n\nOK: allow\nUP / DOWN: deny",
                     "允许使用麦克风功能？\n音频仅在本设备处理。\n\nOK：允许\nUP / DOWN：拒绝"));
    }
    else if (*ps_runtime_error())
      snprintf(text, sizeof(text), TXT("Stopped safely:\n%s", "应用已安全停止：\n%s"), localized_notice(ps_runtime_error()));
    else {
      const ps_view *v = ps_runtime_view();
      bool graphics_changed=v->count!=display_view.count || v->image_count!=display_view.image_count ||
        memcmp(v->commands,display_view.commands,v->count*sizeof(ps_draw)) ||
        memcmp(v->images,display_view.images,v->image_count*sizeof(ps_image_command));
      ps_assets_unpin();
      display_view = *v;
      for (unsigned i = 0; i < v->image_count; i++)
        if (!v->images[i].system)
          ps_asset_pin(v->images[i].handle);
      snprintf(text, sizeof(text), "%s", v->text);
      lv_obj_set_style_text_color(app_body, lv_color_hex(v->text_color), 0);
      lv_obj_remove_flag(app_canvas, LV_OBJ_FLAG_HIDDEN);
      if(graphics_changed)lv_obj_invalidate(app_canvas);
    }
  } else if (!page) {
    set_label_text(title, "Passport OS");
  } else {
    set_label_text(title, TXT(items[focus][0], items[focus][1]));
    if (*notice)
      snprintf(text, sizeof(text), "%s", localized_notice(notice));
    else if (focus == 0) {
      if (!app_count)
        snprintf(text, sizeof(text), "%s",
                 TXT("No packages installed.\nOpen App Store or install\nusing the passport USB CLI.",
                     "尚未安装应用。\n请打开应用商店，或使用\nPassport USB 命令行安装。"));
      for (int i = 0; i < app_count; i++) {
        char row[90];
        snprintf(row, sizeof(row), "%s %.63s\n", i == selection ? ">" : " ",
                 strrchr(ids[i], '.') ? strrchr(ids[i], '.') + 1 : ids[i]);
        strlcat(text, row, sizeof(text));
      }
    } else if (focus == 1) {
      cJSON *apps = cJSON_GetObjectItemCaseSensitive(catalog, "apps");
      if (!apps) {
        if (*store_url)
          snprintf(text,sizeof(text),"%s",TXT("App Store\n\nOK: browse apps\nOffline apps remain available", "应用商店\n\nOK：浏览应用\n离线应用仍可使用"));
        else
          snprintf(text,sizeof(text),TXT("Local App Store\n%s:%u\n\nOK: browse / approve service\nOffline apps remain available", "本地应用商店\n%s:%u\n\nOK：浏览 / 批准服务\n离线应用仍可使用"),service_ip,service_http);
      }
      else {
        int count = cJSON_GetArraySize(apps);
        for (int i = 0; i < count && i < 8; i++) {
          cJSON *a = cJSON_GetArrayItem(apps, i),
                *name = cJSON_GetObjectItemCaseSensitive(a, "name");
          char row[80];
          snprintf(row, sizeof(row), "%s %.40s\n",
                   i == catalog_selection ? ">" : " ",
                   cJSON_IsString(name) ? name->valuestring : "App");
          strlcat(text, row, sizeof(text));
        }
      }
    } else if (focus == 2 && wifi_setup_page) {
      if(ps_wifi_setup_waiting_hotspot()) {
        set_label_text(title,TXT("Phone hotspot","请打开手机热点"));
        snprintf(text,sizeof(text),"%s",ps_wifi_setup_state()==PS_PORTAL_FAILED?
          TXT("Hotspot not connected.\n\nOK: configure again\nDouble OK: cancel", "尚未连接手机热点。\n\nOK：重新配网\n双击 OK：取消"):
          TXT("Settings saved.\n\nTurn on Personal Hotspot and Allow Others to Join.\nUse 2.4 GHz / Maximize Compatibility.\n\nConnecting automatically...", "配置已保存。\n\n请打开个人热点，允许他人加入。\n开启最大兼容性（2.4 GHz）。\n\n正在自动连接…"));
      } else if(wifi_setup_step==0) {
        set_label_text(title,TXT("1. Scan to join Wi-Fi","1. 扫码连接热点"));
        snprintf(text,sizeof(text),"%s",wifi_setup_ssid);
      } else if(wifi_setup_step==1) {
        set_label_text(title,TXT("2. Set up on phone","2. 在手机完成配网"));
        snprintf(text,sizeof(text),"%s",TXT("No page? Scan this code","未弹出页面可扫码"));
      } else {
        set_label_text(title,TXT("3. Manual connection","3. 手动连接"));
        snprintf(text,sizeof(text),TXT("Join: %s\n\nPassword: %s\n\nOpen: http://192.168.4.1/\n\nOK: show QR code", "网络：%s\n\n密码：%s\n\n打开：http://192.168.4.1/\n\nOK：显示二维码"),wifi_setup_ssid,wifi_setup_password);
      }
    }
    else if (focus == 2 && settings_detail) {
      switch (settings_selection) {
        case SET_WIFI: {
          char ssid[33];
          bool connected = ps_wifi_connected_ssid(ssid, sizeof(ssid));
          snprintf(text, sizeof(text),
                   TXT("Status: %s\n%s\n\nConnect this Passport to a 2.4 GHz Wi-Fi network using your phone.\n\nOK: start phone setup",
                       "状态：%s\n%s\n\n使用手机将 Passport 连接到 2.4 GHz Wi-Fi 网络。\n\n按 OK 开始手机配网"),
                   connected ? TXT("Connected", "已连接") : TXT("Offline / reconnecting", "离线 / 正在重连"),
                   connected ? ssid : "");
          break;
        }
        case SET_BLUETOOTH: {
          static const char *en[]={"Off","Starting","Pairing","Connected","Ready","Error","Stopping","Unavailable"};
          static const char *zh[]={"已关闭","正在启动","正在配对","已连接","已就绪","错误","正在停止","不可用"};
          unsigned state=ps_hid_state();if(state>7)state=7;
          snprintf(text,sizeof(text),TXT("Bluetooth peripherals\n\nStatus: %s\n\nOpen the Native Bluetooth app\nto pair an iPhone controller.\n\nBLE only on ESP32-C3.\nHeadphone audio/calls unsupported.",
                                        "蓝牙外设\n\n状态：%s\n\n打开原生蓝牙外设应用，\n连接 iPhone 控制器。\n\nESP32-C3 仅支持 BLE，\n不支持耳机音频及通话。"),TXT(en[state],zh[state]));
          break;
        }
        case SET_DISPLAY:
          snprintf(text, sizeof(text),
                   TXT("Display & Brightness\n\nBrightness: %u%%\n\nUP: brighter   DOWN: dimmer\nChanges are saved automatically.\nScreen timeout is a separate setting.",
                       "显示与亮度\n\n亮度：%u%%\n\nUP：调亮   DOWN：调暗\n更改会自动保存。\n熄屏时间可单独设置。"), settings_brightness);
          break;
        case SET_APPEARANCE:
          snprintf(text, sizeof(text),
                   TXT("Appearance\n\nCurrent mode: %s\n\nUP/DOWN: switch between Dark and Light. MiniApps can read this mode and choose their own colors.",
                       "外观\n\n当前模式：%s\n\nUP/DOWN：切换深色或浅色。MiniApp 可读取此模式并设置自己的配色。"),
                   display_mode_name());
          break;
        case SET_TIMEOUT:
          snprintf(text, sizeof(text),
                   TXT("Turn off the display after inactivity.\n\nCurrent: %s\n\nUP/DOWN: change timeout\nThe first key press wakes the screen.",
                       "无操作一段时间后关闭屏幕。\n\n当前设置：%s\n\nUP/DOWN：更改熄屏时间\n首次按键只会唤醒屏幕。"), timeout_label());
          break;
        case SET_SOUND:
          snprintf(text, sizeof(text),
                   TXT("Sounds\n\n%s Media volume: %u%%\n%s Alarm volume: %u%%\n\nUP/DOWN: adjust by 5%%\nOK: switch control\n0%%: silent\nSaved automatically.",
                       "声音\n\n%s 媒体音量：%u%%\n%s 闹钟音量：%u%%\n\n上下键：每次调节 5%%\nOK：切换项目\n0%%：静音\n设置自动保存。"),
                   sound_alarm ? "  " : ">", ps_audio_volume(false),
                   sound_alarm ? ">" : "  ", ps_audio_volume(true));
          break;
        case SET_PRIVACY:
          snprintf(text, sizeof(text), "%s",
                   TXT("App permissions\n\nMicrophone access is requested per app.\nYou can allow or deny it at the prompt.\nNetwork access follows each app manifest.",
                       "应用权限\n\n每个应用单独请求麦克风权限。\n你可以在提示时允许或拒绝。\n网络访问受应用清单限制。"));
          break;
        case SET_DATETIME: {
          char date[32], clock[16];
          format_clock(date, sizeof(date), "%Y-%m-%d", TXT("Waiting for sync", "等待同步"));
          format_clock(clock, sizeof(clock), "%H:%M:%S", "--:--:--");
          snprintf(text, sizeof(text),
                   TXT("Date & Time\n\n%s\n%s\n\nAutomatic time: %s\nServer: pool.ntp.org\nTime zone: Asia/Shanghai (UTC+08:00)",
                       "日期与时间\n\n%s\n%s\n\n自动校时：%s\n服务器：pool.ntp.org\n时区：Asia/Shanghai（UTC+08:00）"),
                   date, clock, system_time_valid(NULL) ? TXT("On", "已开启") : TXT("Waiting for Wi-Fi", "等待 Wi-Fi"));
          break;
        }
        case SET_ALARM: {
          char time[16], state[16];
          if (alarm_field == 0)
            snprintf(time, sizeof(time), "[%02u]:%02u", alarm_config.hour,
                     alarm_config.minute);
          else if (alarm_field == 1)
            snprintf(time, sizeof(time), "%02u:[%02u]", alarm_config.hour,
                     alarm_config.minute);
          else
            snprintf(time, sizeof(time), "%02u:%02u", alarm_config.hour,
                     alarm_config.minute);
          const char *state_value = alarm_config.enabled
                                        ? TXT("On", "开启")
                                        : TXT("Off", "关闭");
          snprintf(state, sizeof(state), alarm_field == 2 ? "[%s]" : "%s",
                   state_value);
          const char *field = alarm_field == 0 ? TXT("Hour", "小时")
                               : alarm_field == 1 ? TXT("Minute", "分钟")
                                                  : TXT("Enabled", "启用");
          snprintf(text, sizeof(text),
                   TXT("System Alarm\n\nTime: %s\nStatus: %s\nEditing: %s\n\nUP/DOWN: adjust selected item\nOK: next item\nWait 1.5 sec to save.",
                       "系统闹钟\n\n时间：%s\n状态：%s\n正在设置：%s\n\n上下键：调整当前项\n短按 OK：下一项\n停止操作 1.5 秒后自动保存。"),
                   time, state, field);
          break;
        }
        case SET_LANGUAGE:
          snprintf(text, sizeof(text),
                   TXT("System language\n\nCurrent: %s\n\nUP/DOWN: switch language\nChanges are saved automatically.",
                       "系统语言\n\n当前语言：%s\n\nUP/DOWN：切换语言\n更改会自动保存。"),
                   language_name());
          break;
        case SET_UPDATE: {
          ps_update_info update;
          ps_update_read(&update);
          if (!update_url[0]) {
            snprintf(text, sizeof(text), "%s",
                     TXT("System Update\n\nNo update source configured.\nConnect by USB and run:\npassport device update-source\n<HTTPS latest.json URL>",
                         "系统更新\n\n尚未设置更新源。\n通过 USB 连接后运行：\npassport device update-source\n<HTTPS latest.json 地址>"));
          } else if (update.state == PS_UPDATE_AVAILABLE) {
            snprintf(text, sizeof(text),
                     TXT("Update available\n\nVersion: %s\nBuild: %lu\nImage: %u bytes\n\nOK: download and verify",
                         "发现可用更新\n\n版本：%s\n构建号：%lu\n镜像：%u 字节\n\n按 OK 下载并验证"),
                     update.version, (unsigned long)update.build,
                     (unsigned)update.image_size);
          } else if (update.state == PS_UPDATE_DOWNLOADING) {
            unsigned percent = update.image_size
                                   ? (unsigned)(update.downloaded * 100 /
                                                update.image_size)
                                   : 0;
            if (percent > 100) percent = 100;
            unsigned filled = percent / 10;
            char progress[11];
            for (unsigned i = 0; i < 10; i++)
              progress[i] = i < filled ? '#' : '-';
            progress[10] = 0;
            snprintf(text, sizeof(text),
                     TXT("Downloading %s\n\n[%s] %u%%\n%u / %u bytes\nKeep Passport connected to Wi-Fi.",
                         "正在下载 %s\n\n[%s] %u%%\n%u / %u 字节\n请保持 Passport 连接 Wi-Fi。"), update.version,
                     progress, percent,
                     (unsigned)update.downloaded,
                     (unsigned)update.image_size);
          } else if (update.state == PS_UPDATE_READY) {
            snprintf(text, sizeof(text),
                     TXT("Update verified\n\nVersion: %s\nPress OK to install and restart.\nThe previous slot remains available for rollback.",
                         "更新验证通过\n\n版本：%s\n按 OK 安装并重启。\n之前的系统槽可用于回滚。"),
                     update.version);
          } else if (update.state == PS_UPDATE_CURRENT) {
            snprintf(text, sizeof(text), "%s",
                     TXT("Passport OS is up to date.\n\nVerified with the built-in system release key.",
                         "Passport OS 已是最新版本。\n\n已通过系统内置发布密钥验证。"));
          } else if (update.state == PS_UPDATE_CHECKING) {
            snprintf(text, sizeof(text), "%s", TXT("Checking signed release manifest...", "正在检查签名更新清单…"));
          } else if (update.state == PS_UPDATE_INSTALLING) {
            snprintf(text, sizeof(text), "%s", TXT("Installing update and restarting...", "正在安装更新并重启…"));
          } else if (update.state == PS_UPDATE_ERROR) {
            snprintf(text, sizeof(text),
                     TXT("Update failed safely.\n\n%s\n\nOK: retry check. Running firmware is unchanged.",
                         "更新失败，当前系统未受影响。\n\n%s\n\n按 OK 重试检查。"),
                     localized_notice(update.error));
          } else {
            snprintf(text, sizeof(text), "%s",
                     TXT("Automatic system updates\n\nChecks a signed HTTPS release manifest, streams the image into the inactive slot, then verifies its hash and ESP image header.\n\nOK: check now",
                         "系统更新\n\n检查经过签名的 HTTPS 发布清单，将镜像下载到非活动系统槽，并验证哈希和 ESP 镜像头。\n\n按 OK 立即检查"));
          }
          break;
        }
        case SET_STORAGE: {
          ps_storage_usage *u = &storage_usage;
          if (!storage_usage_valid) {
            snprintf(text,sizeof(text),"%s",TXT("Reading storage…", "正在读取存储…"));
          } else if (!u->packages_valid || !u->data_valid || !u->layout_valid) {
            snprintf(text,sizeof(text),"%s",TXT("Storage unavailable. OK: retry", "存储不可用，按 OK 重试"));
          } else if (storage_usage_page == 0) {
            const uint32_t *v=u->layout.bytes;
            snprintf(text,sizeof(text),TXT("\nFlash: %u KiB\nOS: %u KiB\nOTA reserve: %u KiB\nApps / cache: %u KiB\nData reserve: %u KiB\nSystem / overhead: %u KiB\nApp available: %u KiB\nUP/DOWN: details", "\nFlash：%u KiB\n系统：%u KiB\nOTA 预留：%u KiB\n应用及缓存：%u KiB\n数据预留：%u KiB\n系统及开销：%u KiB\n应用可用：%u KiB\n上下：查看详情"),
              (unsigned)(u->layout.total/1024),v[0]/1024,v[1]/1024,v[2]/1024,v[3]/1024,v[4]/1024,v[5]/1024);
          } else if (storage_usage_page == 1) {
            size_t packages=0,previous=0;
            for(unsigned i=0;i<u->count;i++){packages+=u->apps[i].package;previous+=u->apps[i].previous;}
            snprintf(text,sizeof(text),TXT("App storage\n\nCurrent packages: %u KiB\nPrevious versions: %u KiB\nCache / overhead: %u KiB\nAvailable: %u KiB\n\nUP/DOWN: app details", "应用存储\n\n当前应用包：%u KiB\n历史版本：%u KiB\n缓存及开销：%u KiB\n可用：%u KiB\n\n上下：应用详情"),
              (unsigned)(packages/1024),(unsigned)(previous/1024),(unsigned)((u->used>packages+previous?u->used-packages-previous:0)/1024),
              (unsigned)((u->total>u->used?u->total-u->used:0)/1024));
          } else if (storage_usage_page == 2) {
            snprintf(text,sizeof(text),TXT("Persistent data\n\nUsed: %u entries\nAvailable: %u entries\n32 bytes per entry.\nIncludes system / app data.\nPage overhead is separate.\n\nOK: refresh", "持久数据\n\n已用：%u 项\n可用：%u 项\n每项 32 字节。\n包含系统及应用数据。\n页开销另计。\n\nOK：刷新"),(unsigned)u->data_used,(unsigned)u->data_available);
          } else {
            unsigned i=storage_usage_page-3;
            if(i>=u->count)i=0;
            ps_storage_app_usage *a=&u->apps[i];
            const char *name=strrchr(a->id,'.');
            if(!a->data_valid){snprintf(text,sizeof(text),TXT("%s\n\nApp package: %u B\nPrevious: %u B\nData: unavailable\n\nOK: refresh", "%s\n\n应用包：%u B\n历史版本：%u B\n数据：不可用\n\nOK：刷新"),name?name+1:a->id,(unsigned)a->package,(unsigned)a->previous);break;}
            snprintf(text,sizeof(text),TXT("%s\n\nApp package: %u B\nPrevious: %u B\nData: %u entries (~%u B)\n\n%d / %u apps\nUP/DOWN: browse\nOK: refresh", "%s\n\n应用包：%u B\n历史版本：%u B\n数据：%u 项（约 %u B）\n\n%d / %u 应用\n上下：浏览\nOK：刷新"),name?name+1:a->id,(unsigned)a->package,(unsigned)a->previous,(unsigned)a->data_entries,(unsigned)(a->data_entries*32),(int)i+1,u->count);
          }
          break;
        }
        default:
          {
            const esp_app_desc_t *app = esp_app_get_description();
            const char *version = app && app->version[0]
                                      ? app->version : "unknown";
          snprintf(text, sizeof(text),
                   TXT("Passport OS\nVersion %s\nOS size: %u bytes\n\nESP32-C3\nESP-IDF %s\nApp API 1",
                       "Passport OS\n系统版本 %s\n系统大小：%u 字节\n\nESP32-C3\nESP-IDF %s\nMiniApp API 1"),
                   version, running_image_bytes, esp_get_idf_version());
          }
          break;
      }
    }
    else
      snprintf(text, sizeof(text),
               TXT("Free heap: %u\nLargest: %u\nMinimum: %u\n\nAPI 1 / ESP32-C3\nSingle foreground session",
                   "空闲堆：%u\n最大连续：%u\n历史最低：%u\n\nAPI 1 / ESP32-C3\n单前台应用会话"),
               (unsigned)esp_get_free_heap_size(),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
               (unsigned)esp_get_minimum_free_heap_size());
  }
  if (preview) {
    ps_assets_unpin();
    display_view.count = 0;
    display_view.image_count = 1;
    display_view.images[0] = preview_image;
    if (!preview_image.system)
      ps_asset_pin(preview_image.handle);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(canvas);
  }
  if (app_active) set_label_text(app_body, text);
  else {
    lv_obj_set_pos(body,show_storage?26:8,show_wifi_qr?272:78);lv_obj_set_height(body,show_wifi_qr?22:210);
    lv_obj_set_style_text_align(body,show_wifi_qr?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_width(body, preview ? 148 : show_storage ? 206 : 224); set_label_text(body, text);
  }
  if (app_active && fullscreen) set_label_text(footer, "");
  else if (app_active) {
    const char *nav = ps_runtime_display()->navigation;
    if (!strcmp(nav, "game")) set_label_text(footer, TXT("UP/DN Move  OK Act  DBL Home", "UP/DOWN 移动  OK 操作  双击返回"));
    else if (!strcmp(nav, "scroll")) set_label_text(footer, TXT("UP/DN Scroll  OK  DBL Home", "UP/DOWN 滚动  双击返回"));
    else if (!strcmp(nav, "select")) set_label_text(footer, TXT("UP/DN Focus  OK Open", "UP/DOWN 选择  OK 打开"));
    else set_label_text(footer, TXT("UP/DN  OK Open", "UP/DOWN 选择  OK 打开"));
  } else if (focus == 2 && settings_detail)
    set_label_text(footer, settings_selection == SET_DISPLAY
                                  ? TXT("UP/DN Adjust  DBL OK Back", "UP/DOWN 调节  双击 OK 返回")
                                  : settings_selection == SET_SOUND
                                        ? TXT("UP/DN Adjust  OK Switch", "上下调节  OK 切换")
                                  : settings_selection == SET_ALARM
                                        ? TXT("UP/DN Adjust  OK Next", "上下调整  OK 下一项")
                                  : settings_selection == SET_TIMEOUT || settings_selection == SET_LANGUAGE || settings_selection == SET_APPEARANCE
                                        ? TXT("UP/DN Change  DBL OK Back", "UP/DOWN 更改  双击 OK 返回")
                                        : settings_selection == SET_UPDATE
                                              ? TXT("OK Next Step  DBL OK Back", "OK 下一步  双击 OK 返回")
                                              : TXT("OK Action  DBL OK Back", "OK 操作  双击 OK 返回"));
  else if (focus == 2)
    set_label_text(footer, TXT("UP/DN Scroll  OK Open", "UP/DOWN 滚动  OK 打开"));
  else set_label_text(footer, TXT("UP/DN Select  OK Open", "UP/DOWN 选择  OK 打开"));
  bsp_lvgl_unlock();
}
static void button(bsp_btn_t b, bsp_btn_ev_t ev, void *u) {
  (void)u;
  message m = {.kind = KEY, .value = b + ev * 8, .sent_us=(uint32_t)esp_timer_get_time()};
  xQueueSend(events, &m, 0);
}
static bool usb_read_all(void *ptr, size_t n) {
  uint8_t *p = ptr;
  int64_t end = esp_timer_get_time() + 10000000;
  while (n && esp_timer_get_time() < end) {
    int r =
        usb_serial_jtag_read_bytes(p, n > 512 ? 512 : n, pdMS_TO_TICKS(200));
    if (r > 0) {
      p += r;
      n -= r;
    }
  }
  return !n;
}
static void serial_task(void *arg) {
  (void)arg;
  char line[768];
  size_t n = 0;
  bool overflow = false;
  for (;;) {
    uint8_t ch;
    if (usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100)) != 1)
      continue;
    if (ch == '\n') {
      line[n] = 0;
      if (!overflow) {
        message m = {.kind = COMMAND};
        if (!strcmp(line, "cancel_cycle")) {
          atomic_store(&cancel_cycle, true);
          printf("PASSPORT cycle_cancel OK\n");
        } else if (!strncmp(line, "install ", 8)) {
          unsigned count = strtoul(line + 8, NULL, 10);
          if (count > 16 && count <= PS_PAP_MAX) {
            if (!atomic_exchange(&upload_pending, true)) {
              message begin = {.kind = UPLOAD_BEGIN};
              if (xQueueSend(events, &begin, pdMS_TO_TICKS(100)) == pdTRUE &&
                  xSemaphoreTake(upload_gate, pdMS_TO_TICKS(20000)) == pdTRUE &&
                  storage_ready) {
                FILE *f = fopen("/cache/incoming.pap", "wb");
                bool ok = f != NULL;
                unsigned offset = 0;
                uint8_t block[128];
                if (ok)
                  printf("PASSPORT READY %u\n", count);
                while (ok && offset < count) {
                  unsigned amount = count - offset > 128 ? 128 : count - offset;
                  if (!usb_read_all(block, amount) ||
                      fwrite(block, 1, amount, f) != amount) {
                    ok = false;
                    break;
                  }
                  offset += amount;
                  printf("PASSPORT ACK %u\n", offset);
                }
                if (f && fclose(f))
                  ok = false;
                if (ok) {
                  m.kind = INSTALL;
                  m.value = 1;
                  m.size = count;
                  m.data = strdup("/cache/incoming.pap");
                } else {
                  unlink("/cache/incoming.pap");
                  printf("PASSPORT install FAIL transfer\n");
                  atomic_store(&upload_pending, false);
                }
              } else {
                printf("PASSPORT install FAIL storage or busy\n");
                atomic_store(&upload_pending, false);
              }
            } else
              printf("PASSPORT install FAIL busy\n");
          }
        } else
          m.data = strdup(line);
        if (m.data && xQueueSend(events, &m, pdMS_TO_TICKS(100)) != pdTRUE)
          free(m.data);
      }
      memset(line, 0, sizeof(line));
      n = 0;
      overflow = false;
    } else if (n < sizeof(line) - 1)
      line[n++] = ch;
    else
      overflow = true;
  }
}
static void command(const char *line) {
  cJSON *j = ps_json_parse(line, strlen(line));
  const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(j, "ssid"),
              *pw = cJSON_GetObjectItemCaseSensitive(j, "password");
  const cJSON *publisher = cJSON_GetObjectItemCaseSensitive(j, "publisher_pem");
  const cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(j, "time");
  if (cJSON_IsString(publisher) &&
      strlen(publisher->valuestring) < sizeof(pending_publisher) &&
      ps_trust_key_id(publisher->valuestring, pending_key_id)) {
    close_app();
    strcpy(pending_publisher, publisher->valuestring);
    publisher_pending = true;
    printf("PASSPORT publisher pending %s\n", pending_key_id);
  } else if (cJSON_IsNumber(timestamp) && timestamp->valuedouble > 1700000000 &&
             timestamp->valuedouble < 2200000000) {
    struct timeval tv = {.tv_sec = (time_t)timestamp->valuedouble};
    settimeofday(&tv, NULL);
    printf("PASSPORT time OK\n");
  } else if (cJSON_IsString(ssid) && cJSON_IsString(pw))
    printf("PASSPORT wifi_config %s\n",
           ps_wifi_configure(ssid->valuestring, pw->valuestring) ? "OK"
                                                                 : "FAIL");
  else if (!strcmp(line,"ble_debug")) {
    ps_hid_debug();
  } else if (!strcmp(line,"ble_trace on") || !strcmp(line,"ble_trace off")) {
    ps_hid_trace(!strcmp(line,"ble_trace on"));
  } else if (!strcmp(line,"display_metrics")) {
    printf("PASSPORT DISPLAY_METRICS frames=%u input_last_us=%u input_max_us=%u render_last_us=%u render_max_us=%u lock_max_us=%u\n",(unsigned)display_frames,(unsigned)input_last_us,(unsigned)input_max_us,(unsigned)render_last_us,(unsigned)render_max_us,(unsigned)lvgl_lock_max_us);
  } else if (!strcmp(line,"font_probe")) {
    unsigned checked=0,missing=0;
    if(bsp_lvgl_lock(1000)) {
      const char *text=lv_label_get_text(app_body);
      const lv_font_t *font=lv_obj_get_style_text_font(app_body,LV_PART_MAIN);
      for(size_t i=0;text[i];) {
        uint32_t cp=(unsigned char)text[i++];unsigned extra=0;
        if(cp>=0xf0){cp&=7;extra=3;}else if(cp>=0xe0){cp&=15;extra=2;}else if(cp>=0xc0){cp&=31;extra=1;}
        while(extra && text[i]){cp=(cp<<6)|((unsigned char)text[i++]&63);extra--;}
        if(cp=='\n'||cp=='\r')continue;
        lv_font_glyph_dsc_t glyph={0};checked++;
        if(!lv_font_get_glyph_dsc(font,&glyph,cp,0)||glyph.is_placeholder)missing++;
      }
      bsp_lvgl_unlock();
    }
    printf("PASSPORT FONT checked=%u missing=%u system_language=%s\n",checked,missing,ps_language_code(system_language));
  } else if (!strcmp(line, "metrics")) {
    measure("serial");
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    TaskStatus_t tasks[24];
    unsigned long total = 0;
    UBaseType_t n = uxTaskGetSystemState(tasks, 24, &total);
    for (UBaseType_t i = 0; i < n; i++)
      printf("PASSPORT TASK %s stack=%u runtime=%lu total=%lu\n",
             tasks[i].pcTaskName, (unsigned)tasks[i].usStackHighWaterMark,
             (unsigned long)tasks[i].ulRunTimeCounter, total);
#endif
    printf("PASSPORT DISPLAY frames=%u\n", display_frames);
    const ps_asset_metrics *asset = ps_assets_metrics();
    lv_mem_monitor_t pool;
    if (bsp_lvgl_lock(1000)) {
      lv_mem_monitor(&pool);
      bsp_lvgl_unlock();
      printf("PASSPORT ASSETS bytes=%u peak=%u loads=%u failures=%u "
             "load_us=%llu max_load_us=%llu lvgl_free=%u lvgl_max_used=%u\n",
             (unsigned)asset->current, (unsigned)asset->peak, asset->loads,
             asset->failures, (unsigned long long)asset->load_us,
             (unsigned long long)asset->max_load_us, (unsigned)pool.free_size,
             (unsigned)pool.max_used);
    }

  } else if (!strcmp(line, "icons")) {
    close_app();
    page = 1;
    focus = 3;
    notice[0] = 0;
    draw();
    bool fonts = false;
    if (bsp_lvgl_lock(1000)) {
      fonts = ps_icon_fonts_ready();
      bsp_lvgl_unlock();
    }
    printf("PASSPORT ICONS count=%u fonts=%d\n", ps_icon_count(), fonts);
  } else if (!strncmp(line, "asset_probe ", 12)) {
    uint32_t token = app_active ? ps_asset_open(&package, line + 12) : 0;
    char hash[65];
    unsigned rows = 0, scratch = 0;
    bool ok = false;
    if (token && bsp_lvgl_lock(1000)) {
      ok = ps_asset_probe(token, hash, &rows, &scratch);
      bsp_lvgl_unlock();
    }
    printf("PASSPORT DECODE ok=%d rows=%u scratch=%u hash=%s\n", ok, rows,
           scratch, ok ? hash : "");
  } else if (!strcmp(line, "screen")) {
    if (bsp_lvgl_lock(1000)) {
      cJSON *v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "title", lv_label_get_text(title));
      cJSON_AddStringToObject(v, "body", lv_label_get_text(body));
      cJSON_AddStringToObject(v, "footer", lv_label_get_text(footer));
      bsp_lvgl_unlock();
      char *out = cJSON_PrintUnformatted(v);
      printf("PASSPORT SCREEN %s\n", out ? out : "{}");
      free(out);
      cJSON_Delete(v);
    }
  } else if (!strcmp(line,"wifi_setup_probe")) {
    printf("PASSPORT WIFISETUP running=%u phone=%u state=%u hotspot=%u\n",ps_wifi_setup_active(),ps_wifi_setup_phone_connected(),ps_wifi_setup_state(),ps_wifi_setup_waiting_hotspot());
  } else if (!strcmp(line,"wifi_qr_probe")) {
    if(bsp_lvgl_lock(1000)) {
      lv_draw_buf_t *q=wifi_qr?lv_canvas_get_draw_buf(wifi_qr):NULL;
      if(q && q->header.w==176 && q->header.h==176) {
        printf("PASSPORT QR width=176 height=176 stride=%u\n",(unsigned)q->header.stride);
        for(unsigned y=0;y<176;y++) {
          const uint8_t *row=(const uint8_t*)q->data+8+y*q->header.stride;
          printf("PASSPORT QRROW %u ",y);for(unsigned x=0;x<22;x++)printf("%02x",row[x]);printf("\n");
        }
      } else printf("PASSPORT QR active=0\n");
      bsp_lvgl_unlock();
    }
  } else if (!strcmp(line, "ota_status")) {
    const esp_partition_t *part = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_err_t rc = esp_ota_get_state_partition(part, &state);
    printf("PASSPORT OTA running=%s state=%d rc=%d\n", part->label,
           rc == ESP_OK ? state : -1, rc);
  } else if (!strcmp(line, "ota_prepare") || !strncmp(line, "ota_trial ", 10)) {
    close_app();
    bool prepare = !strcmp(line, "ota_prepare");
    const esp_partition_t *part = prepare
                                      ? esp_ota_get_running_partition()
                                      : esp_ota_get_next_update_partition(NULL);
    nvs_handle_t h;
    esp_err_t rc = nvs_open("osdev", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
      uint8_t flag = !strcmp(line + (!prepare ? 10 : 0), "reject")  ? 1
                     : !strcmp(line + (!prepare ? 10 : 0), "reset") ? 2
                                                                    : 0;
      rc = nvs_set_u8(h, "ota_test", flag);
      if (rc == ESP_OK)
        rc = nvs_commit(h);
      nvs_close(h);
    }
    if (rc == ESP_OK)
      rc = esp_ota_set_boot_partition(part);
    printf("PASSPORT OTA select=%s rc=%d\n", part ? part->label : "none", rc);
    fflush(stdout);
    if (rc == ESP_OK) {
      vTaskDelay(pdMS_TO_TICKS(100));
      esp_restart();
    }
  } else if (!strcmp(line, "view")) {
    cJSON *v = cJSON_CreateObject();
    cJSON_AddBoolToObject(v, "active", app_active);
    cJSON_AddStringToObject(v, "text", ps_runtime_view()->text);
    cJSON_AddStringToObject(v, "error", ps_runtime_error());
    cJSON_AddNumberToObject(v, "lua", ps_runtime_view()->lua_used);
    cJSON_AddNumberToObject(v, "lua_peak", ps_runtime_view()->lua_peak);
    cJSON_AddBoolToObject(v, "microphone", ps_runtime_view()->microphone);
    const ps_display_layout *layout = ps_runtime_display();
    cJSON_AddStringToObject(v, "display_mode", layout->mode == PS_DISPLAY_FULLSCREEN ? "fullscreen" : "standard");
    cJSON_AddNumberToObject(v, "viewport_width", layout->viewport.width);
    cJSON_AddNumberToObject(v, "viewport_height", layout->viewport.height);
    cJSON_AddNumberToObject(v, "images", ps_runtime_view()->image_count);
    cJSON_AddNumberToObject(v, "asset_bytes", ps_assets_metrics()->current);
    cJSON_AddNumberToObject(v, "asset_peak", ps_assets_metrics()->peak);
    unsigned char canvas_hash[32];
    char digest[65];
    mbedtls_sha256((const unsigned char *)ps_runtime_view()->commands,
                   ps_runtime_view()->count * sizeof(ps_draw), canvas_hash, 0);
    for (int i = 0; i < 32; i++)
      sprintf(digest + i * 2, "%02x", canvas_hash[i]);
    cJSON_AddStringToObject(v, "canvas_sha256", digest);

    char *out = cJSON_PrintUnformatted(v);
    printf("PASSPORT VIEW %s\n", out ? out : "{}");
    free(out);
    cJSON_Delete(v);
  } else if (!strcmp(line, "reconnect")) {
    esp_wifi_disconnect();
    printf("PASSPORT reconnect requested\n");
  } else if (!strncmp(line, "update_source ", 14)) {
    const char *url = line + 14;
    ps_net_cancel();
    if (!strcmp(url, "clear")) {
      ps_net_clear_update_source();
      update_url[0] = 0;
      nvs_handle_t h;
      bool saved = nvs_open("osconfig", NVS_READWRITE, &h) == ESP_OK;
      if (saved) {
        esp_err_t rc = nvs_erase_key(h, "update_url");
        if (rc == ESP_ERR_NVS_NOT_FOUND) rc = ESP_OK;
        if (rc == ESP_OK) rc = nvs_commit(h);
        nvs_close(h);
        saved = rc == ESP_OK;
      }
      printf("PASSPORT update_source %s\n", saved ? "CLEARED" : "FAIL");
    } else {
      char old_url[sizeof(update_url)];
      snprintf(old_url, sizeof(old_url), "%s", update_url);
      if (ps_net_set_update_source(url)) {
        nvs_handle_t h;
        bool saved = nvs_open("osconfig", NVS_READWRITE, &h) == ESP_OK;
        if (saved) {
          esp_err_t rc = nvs_set_str(h, "update_url", url);
          if (rc == ESP_OK) rc = nvs_commit(h);
          nvs_close(h);
          saved = rc == ESP_OK;
        }
        if (saved) snprintf(update_url, sizeof(update_url), "%s", url);
        else {
          if (old_url[0]) ps_net_set_update_source(old_url);
          else ps_net_clear_update_source();
        }
        printf("PASSPORT update_source %s\n", saved ? "OK" : "FAIL");
      } else
        printf("PASSPORT update_source DENIED\n");
    }
  } else if (!strncmp(line, "catalog ", 8)) {
    close_app();
    ps_net_cancel();
    if (ps_net_set_store(line + 8)) {
      snprintf(store_url, sizeof(store_url), "%s", line + 8);
      nvs_handle_t h;
      if (nvs_open("osconfig", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "catalog_url", store_url);
        nvs_commit(h);
        nvs_close(h);
      }
      cJSON_Delete(catalog);
      catalog = NULL;
      page = 1;
      focus = 1;
      snprintf(notice, sizeof(notice), "HTTPS catalog configured. OK browse");
      printf("PASSPORT catalog OK\n");
    } else
      printf("PASSPORT catalog DENIED\n");
  } else if (!strncmp(line, "service ", 8)) {
    unsigned hp, wp;
    char ip[16], extra;
    if (sscanf(line + 8, "%15s %u %u %c", ip, &hp, &wp, &extra) == 3 && hp &&
        wp && hp <= 65535 && wp <= 65535) {
      close_app();
      ps_net_cancel();
      ps_net_local_revoke();
      store_url[0] = 0;
      strcpy(service_ip, ip);
      service_http = hp;
      service_ws = wp;
      service_pending = true;
      service_approved = false;
      page = 1;
      focus = 1;
    }
  } else if (!strncmp(line, "cycle ", 6) ||
             !strncmp(line, "cycle_render ", 13)) {
    bool render = !strncmp(line, "cycle_render ", 13);
    atomic_store(&cancel_cycle, false);
    char id[64];
    unsigned count;
    if (sscanf(line + (render ? 13 : 6), "%63s %u", id, &count) == 2 &&
        count > 0 && count <= 1000) {
      close_app();
      open_app(id);
      if (render) {
        draw();
        if (bsp_lvgl_lock(1000)) {
          lv_refr_now(NULL);
          bsp_lvgl_unlock();
        }
      }
      close_app();
      size_t before = esp_get_free_heap_size();
      unsigned ok = 0;
      int64_t start = esp_timer_get_time();
      for (unsigned i = 0; i < count && !atomic_load(&cancel_cycle); i++) {
        if (open_app(id)) {
          if (render) {
            draw();
            if (bsp_lvgl_lock(1000)) {
              lv_refr_now(NULL);
              bsp_lvgl_unlock();
            }
          }
          ok++;
        }
        close_app();
        vTaskDelay(1);
      }
      printf("PASSPORT CYCLE count=%u ok=%u drift=%d us=%lld\n", count, ok,
             (int)esp_get_free_heap_size() - (int)before,
             (long long)(esp_timer_get_time() - start));
    }
  } else if (!strcmp(line, "stress")) {
    size_t before = esp_get_free_heap_size();
    for (int i = 0; i < 500; i++) {
      ps_session s = {0};
      ps_session_begin(&s);
      ps_session_end(&s);
    }
    printf("PASSPORT stress sessions=500 delta=%d\n",
           (int)esp_get_free_heap_size() - (int)before);
    measure("500_sessions");
  } else if (!strncmp(line, "launch ", 7)) {
    page = 1;
    focus = 0;
    printf("PASSPORT launch %s\n", open_app(line + 7) ? "OK" : "FAIL");
  } else if (!strncmp(line, "offload ", 8)) {
    close_app();
    printf("PASSPORT offload %s\n", ps_store_offload(line + 8) ? "OK" : "FAIL");
  } else if (!strncmp(line, "package_fault ", 14)) {
    char id[64], mode[24];
    close_app();
    reset_preview();
    bool ok = sscanf(line + 14, "%63s %23s", id, mode) == 2 &&
              ps_store_fault_fixture(id, mode);
    printf("PASSPORT PACKAGE_FAULT %s\n", ok ? "OK" : "FAIL");
  } else if (!strncmp(line, "data_hash ", 10)) {
    char id[64], key[16], hash[65];
    bool ok = sscanf(line + 10, "%63s %15s", id, key) == 2 &&
              ps_store_data_hash(id, key, hash);
    printf("PASSPORT DATA_HASH %s\n", ok ? hash : "FAIL");
  } else if (!strncmp(line, "cache_budget ", 13)) {
    size_t bytes = strtoul(line + 13, NULL, 10);
    ps_store_cache_budget(bytes);
    printf("PASSPORT cache budget=%u\n", (unsigned)bytes);
  } else if (!strncmp(line, "pin ", 4)) {
    char id[64], mode[4] = "on", extra;
    int fields = sscanf(line + 4, "%63s %3s %c", id, mode, &extra);
    bool ok = (fields == 1 || fields == 2) &&
              (!strcmp(mode, "on") || !strcmp(mode, "off")) &&
              ps_store_pin(id, !strcmp(mode, "on"));
    printf("PASSPORT pin %s\n", ok ? "OK" : "FAIL");
  } else if (!strncmp(line, "remove ", 7)) {
    close_app();
    printf("PASSPORT remove %s\n",
           ps_store_remove(line + 7, true) ? "OK" : "FAIL");
  } else if (!strcmp(line, "home")) {
    if (wifi_setup_page) ps_wifi_setup_stop();
    wifi_setup_page = false;
    memset(wifi_setup_password, 0, sizeof(wifi_setup_password));
    close_app();
    page = 0;
  } else if (!strncmp(line, "button ", 7)) {
    int key=-1; char name[16],extra;
    bool ok=sscanf(line+7,"%d %15s %c",&key,name,&extra)==2 && key>=0 && key<3;
    int ev=-1;
    static const char *names[]={"press","click","double","long","release"};
    if (ok) for(int i=0;i<5;i++) if(!strcmp(name,names[i])) ev=i;
    if (key!=BSP_BTN_OK && ev==BSP_BTN_DOUBLE) ok=false;
    if (ok && ev>=0) { message m={.kind=KEY,.value=key+ev*8,.sent_us=(uint32_t)esp_timer_get_time()}; ok=xQueueSend(events,&m,0)==pdTRUE; }
    else ok=false;
    printf("PASSPORT button %s\n",ok?"OK":"FAIL");
  } else if (!strncmp(line, "key ", 4)) {
    message m = {.kind = KEY, .value = atoi(line + 4)};
    if (m.value >= 0 && m.value < 3) {
      xQueueSend(events, &m, 0);
      if (m.value == BSP_BTN_OK) {
        m.value += BSP_BTN_CLICK * 8;
        xQueueSend(events, &m, 0);
      }
    }
  } else if (!strcmp(line, "list")) {
    char list[12][64];
    int n = storage_ready ? ps_store_registered(list, 12) : 0;
    for (int i = 0; i < n; i++)
      printf("PASSPORT app %s\n", list[i]);
    printf("PASSPORT list count=%d\n", n);
  }
  cJSON_Delete(j);
  printf("PASSPORT DONE\n");
}
// Keep the 2 KiB receive scratch off app_main's persistent stack frame.
static __attribute__((noinline)) void poll_app_websocket(void) {
  char incoming[2049];
  int opcode; size_t incoming_size;
  /* Drain a bounded batch; the normal FPS/view-change path draws once later.
   * Never wait for LVGL between successive frames in a server audio burst. */
  for (unsigned i=0;i<4 && app_active;i++) {
    if (!ps_net_ws_poll(incoming,sizeof(incoming),&incoming_size,&opcode)) break;
    ps_runtime_message(incoming,incoming_size,opcode);
  }
}

static void allocation_failed(size_t bytes,uint32_t caps,const char *function) {
  /* The failure hook must not allocate or wait on the normal logger. */
  esp_rom_printf("PASSPORT ALLOC_FAIL bytes=%u caps=%x function=%s\n",(unsigned)bytes,(unsigned)caps,function?function:"?");
}
void app_main(void) {
  (void)heap_caps_register_failed_alloc_callback(allocation_failed);
  measure("boot");
  ESP_ERROR_CHECK(nvs_flash_init());
  ps_hid_prepare_storage();
  ps_update_init();
  const esp_partition_t *boot_partition = esp_ota_get_running_partition();
  esp_image_metadata_t image_metadata = {0};
  esp_partition_pos_t image_position = {.offset = boot_partition->address, .size = boot_partition->size};
  if (esp_image_get_metadata(&image_position, &image_metadata) == ESP_OK)
    running_image_bytes = image_metadata.image_len;
  esp_ota_img_states_t boot_state = ESP_OTA_IMG_UNDEFINED;
  esp_ota_get_state_partition(boot_partition, &boot_state);
  ESP_LOGI("passport", "OTA boot=%s state=%d", boot_partition->label,
           boot_state);
  nvs_handle_t ota_test_handle;
  uint8_t ota_test = 0;
  if (nvs_open("osdev", NVS_READWRITE, &ota_test_handle) == ESP_OK) {
    nvs_get_u8(ota_test_handle, "ota_test", &ota_test);
    if (ota_test) {
      nvs_set_u8(ota_test_handle, "ota_test", 0);
      nvs_commit(ota_test_handle);
    }
    nvs_close(ota_test_handle);
  }
  if (boot_state == ESP_OTA_IMG_PENDING_VERIFY && ota_test == 1) {
    ESP_LOGW("passport", "OTA simulated self-test rejection");
    ps_update_clear_pending();
    esp_err_t rollback = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGE("passport", "OTA rollback failed rc=%d", rollback);
  }
  if (boot_state == ESP_OTA_IMG_PENDING_VERIFY && ota_test == 2) {
    ESP_LOGW("passport", "OTA PENDING reset-test window");
    vTaskDelay(pdMS_TO_TICKS(8000));
  }
  if (ps_update_pending_build() &&
      boot_state != ESP_OTA_IMG_PENDING_VERIFY)
    ps_update_clear_pending();

  ESP_ERROR_CHECK(bsp_i2c_init());
  ESP_ERROR_CHECK(bsp_display_init());
  lv_display_t *display = bsp_lvgl_init();
  configASSERT(display);
  configASSERT(bsp_lvgl_lock(1000));
  lv_display_add_event_cb(display, display_event, LV_EVENT_REFR_READY, NULL);
  lv_display_add_event_cb(display, display_event, LV_EVENT_REFR_START, NULL);
  bsp_lvgl_unlock();
  bsp_battery_init();
  upload_gate = xSemaphoreCreateBinary();
  configASSERT(upload_gate);
  events = xQueueCreate(16, sizeof(message));
  configASSERT(events);
  ESP_ERROR_CHECK(bsp_button_init(button, NULL));
  configASSERT(bsp_lvgl_lock(1000));
  lv_obj_t *root = lv_screen_active();
  lv_obj_set_style_bg_color(root, lv_color_hex(0x101a29), 0);
  lv_obj_set_style_text_color(root, lv_color_hex(0xe9f0fa), 0);
  status = lv_label_create(root);
  lv_obj_set_pos(status, 8, 4);
  lv_obj_set_width(status, 84);
  lv_label_set_long_mode(status, LV_LABEL_LONG_CLIP);
  clock_label = lv_label_create(root);
  lv_obj_set_pos(clock_label, 96, 4);
  lv_obj_set_width(clock_label, 48);
  lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, 0);
  battery_label = lv_label_create(root);
  lv_obj_set_pos(battery_label, 190, 4);
  lv_obj_set_width(battery_label, 42);
  lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
  title = lv_label_create(root);
  lv_obj_set_pos(title, 24, 45);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_width(title, 200);
  body = lv_label_create(root);
  lv_obj_set_pos(body, 8, 78);
  lv_obj_set_size(body, 224, 210);
  lv_label_set_long_mode(body, LV_LABEL_LONG_MODE_WRAP);
  canvas = lv_obj_create(root);
  lv_obj_set_pos(canvas, 12, 110);
  lv_obj_set_size(canvas, 216, 180);
  lv_obj_remove_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(canvas, lv_color_hex(0x101a29), 0);
  lv_obj_set_style_border_width(canvas, 0, 0);
  lv_obj_set_style_pad_all(canvas, 0, 0);
  lv_obj_add_event_cb(canvas, paint, LV_EVENT_DRAW_MAIN, NULL);
  home_tiles = lv_obj_create(root);
  lv_obj_set_pos(home_tiles, 12, 80);
  lv_obj_set_size(home_tiles, 216, 200);
  lv_obj_remove_flag(home_tiles, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(home_tiles, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(home_tiles, 0, 0);
  lv_obj_set_style_pad_all(home_tiles, 0, 0);
  lv_obj_add_event_cb(home_tiles, paint_home, LV_EVENT_DRAW_MAIN, NULL);
  app_host = lv_obj_create(root);
  lv_obj_set_pos(app_host, 0, 24); lv_obj_set_size(app_host, 240, 272);
  lv_obj_set_style_bg_opa(app_host, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(app_host, 0, 0); lv_obj_set_style_pad_all(app_host, 0, 0);
  lv_obj_add_flag(app_host, LV_OBJ_FLAG_HIDDEN);
  app_canvas = lv_obj_create(app_host); lv_obj_set_pos(app_canvas, 0, 0); lv_obj_set_size(app_canvas, 240, 272);
  lv_obj_remove_flag(app_canvas, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_style_bg_opa(app_canvas, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(app_canvas, 0, 0); lv_obj_set_style_pad_all(app_canvas, 0, 0);
  lv_obj_add_event_cb(app_canvas, paint, LV_EVENT_DRAW_MAIN, NULL);
  app_body = lv_label_create(app_host); lv_obj_set_pos(app_body, 8, 8); lv_obj_set_width(app_body, 224);
  lv_obj_set_style_text_font(app_body, multilingual_font(), 0);
  lv_obj_set_style_text_color(app_body, lv_color_hex(0xe9f0fa), 0);
  lv_label_set_long_mode(app_body, LV_LABEL_LONG_MODE_WRAP);
  cards_view = lv_obj_create(root); lv_obj_set_pos(cards_view, 12, 80); lv_obj_set_size(cards_view, 216, 192);
  lv_obj_remove_flag(cards_view, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_style_bg_opa(cards_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cards_view, 0, 0); lv_obj_set_style_pad_all(cards_view, 0, 0);
  lv_obj_add_flag(cards_view, LV_OBJ_FLAG_HIDDEN); lv_obj_add_event_cb(cards_view, paint_cards, LV_EVENT_DRAW_MAIN, NULL);
  settings_view = lv_obj_create(root); lv_obj_set_pos(settings_view, 12, 80); lv_obj_set_size(settings_view, 216, 192);
  lv_obj_remove_flag(settings_view, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_style_bg_opa(settings_view, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_view, 0, 0); lv_obj_set_style_pad_all(settings_view, 0, 0);
  lv_obj_add_flag(settings_view, LV_OBJ_FLAG_HIDDEN); lv_obj_add_event_cb(settings_view, paint_settings, LV_EVENT_DRAW_MAIN, NULL);
  storage_bar=lv_obj_create(root);lv_obj_set_pos(storage_bar,12,80);lv_obj_set_size(storage_bar,216,12);
  lv_obj_remove_flag(storage_bar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_opa(storage_bar,LV_OPA_TRANSP,0);
  lv_obj_set_style_border_width(storage_bar,0,0);lv_obj_set_style_pad_all(storage_bar,0,0);
  lv_obj_add_flag(storage_bar,LV_OBJ_FLAG_HIDDEN);lv_obj_add_event_cb(storage_bar,paint_storage,LV_EVENT_DRAW_MAIN,NULL);
  storage_legend=lv_obj_create(root);lv_obj_remove_flag(storage_legend,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(storage_legend,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(storage_legend,0,0);lv_obj_set_style_pad_all(storage_legend,0,0);
  lv_obj_add_flag(storage_legend,LV_OBJ_FLAG_HIDDEN);lv_obj_add_event_cb(storage_legend,paint_storage_legend,LV_EVENT_DRAW_MAIN,NULL);
  footer = lv_label_create(root);
  lv_obj_set_pos(footer, 8, 296);
  lv_obj_set_width(footer, 224);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
  bsp_lvgl_unlock();
  nvs_handle_t brightness_config;
  if (nvs_open("osconfig", NVS_READONLY, &brightness_config) == ESP_OK) {
    nvs_get_u8(brightness_config, "brightness", &settings_brightness);
    uint8_t volume;
    if (nvs_get_u8(brightness_config,"media_volume",&volume)==ESP_OK && volume<=100)
      ps_audio_volume_set(false,volume);
    if (nvs_get_u8(brightness_config,"alarm_volume",&volume)==ESP_OK && volume<=100)
      ps_audio_volume_set(true,volume);
    nvs_get_u16(brightness_config, "screen_timeout", &screen_timeout_seconds);
    uint8_t saved_display_mode = 0;
    if (nvs_get_u8(brightness_config, "display_mode", &saved_display_mode) == ESP_OK)
      system_light_mode = saved_display_mode == 1;
    uint8_t language = PS_LANGUAGE_EN;
    if (nvs_get_u8(brightness_config, "language", &language) == ESP_OK &&
        language < PS_LANGUAGE_COUNT)
      system_language = (ps_language_t)language;
    nvs_close(brightness_config);
  }
  alarm_load();
  ps_runtime_set_system_language(ps_language_code(system_language));
  ps_runtime_set_system_display_mode(system_light_mode ? "light" : "dark");
  if (settings_brightness < 20 || settings_brightness > 100)
    settings_brightness = 65;
  if (screen_timeout_seconds != 0 && screen_timeout_seconds != 15 &&
      screen_timeout_seconds != 30 && screen_timeout_seconds != 60 &&
      screen_timeout_seconds != 120 && screen_timeout_seconds != 300)
    screen_timeout_seconds = 60;
  setenv("TZ", "UTC-8", 1);
  tzset();
  last_activity_us = esp_timer_get_time();
  bsp_display_backlight(settings_brightness);
  measure("lvgl");
  storage_ready = ps_store_init();
  ESP_LOGI("passport", "STORAGE ready=%d", storage_ready);
  default_app_load();
  ps_net_init();
  nvs_handle_t catalog_config;
  if (nvs_open("osconfig", NVS_READONLY, &catalog_config) == ESP_OK) {
    size_t length = sizeof(store_url);
    if (nvs_get_str(catalog_config, "catalog_url", store_url, &length) !=
            ESP_OK || !ps_net_set_store(store_url))
      store_url[0] = 0;
    length = sizeof(update_url);
    if (nvs_get_str(catalog_config, "update_url", update_url, &length) !=
            ESP_OK ||
        !ps_net_set_update_source(update_url))
      update_url[0] = 0;
    nvs_close(catalog_config);
  }
  if (!store_url[0] && PS_DEFAULT_STORE_URL[0] && ps_net_set_store(PS_DEFAULT_STORE_URL))
    snprintf(store_url,sizeof(store_url),"%s",PS_DEFAULT_STORE_URL);
  if (!update_url[0] && PS_DEFAULT_UPDATE_URL[0] && ps_net_set_update_source(PS_DEFAULT_UPDATE_URL))
    snprintf(update_url,sizeof(update_url),"%s",PS_DEFAULT_UPDATE_URL);
  ps_wifi_init();
  measure("wifi");
  usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  usb.rx_buffer_size = 512;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
  configASSERT(xTaskCreate(serial_task, "passport_usb", 4096, NULL, 3, NULL) ==
               pdPASS);
  // Serial recovery remains available independently of signed system OTA.
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    bool update_candidate = ps_update_pending_build() != 0;
    bool self_test = storage_ready &&
                     (!update_candidate || ps_update_confirm_pending());
    if (self_test) {
      esp_err_t valid = esp_ota_mark_app_valid_cancel_rollback();
      ESP_LOGI("passport", "OTA self-test valid rc=%d", valid);
      if (valid != ESP_OK && update_candidate)
        ps_update_clear_pending();
    } else {
      if (update_candidate) ps_update_clear_pending();
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
  }
  ESP_LOGI("passport", "Passport OS ready api=1 dynamic=1");
  if (storage_ready && default_app[0]) {
    char boot_app[sizeof(default_app)];
    snprintf(boot_app, sizeof(boot_app), "%s", default_app);
    bool started = open_app(boot_app);
    if (!started) {
      if (app_active) close_app();
      default_app_store("");
      ESP_LOGW("passport", "Default app %s failed; cleared selection", boot_app);
    } else
      ESP_LOGI("passport", "Default app started: %s", boot_app);
  }
  draw();
  int64_t refresh = 0;
  for (;;) {
    if (!screen_is_off && screen_timeout_seconds && !mic_prompt &&
        !wifi_setup_page && esp_timer_get_time() - last_activity_us >=
                                 (int64_t)screen_timeout_seconds * 1000000) {
      bsp_display_backlight(0);
      screen_is_off = true;
      ESP_LOGI("passport", "DISPLAY timeout after %u seconds",
               screen_timeout_seconds);
    }
    if(ps_storage_usage_take(&storage_usage)){storage_usage_valid=true;if(settings_detail && settings_selection==SET_STORAGE)draw();}
    if(wifi_setup_page) {
      bool was_waiting=ps_wifi_setup_waiting_hotspot();ps_wifi_setup_poll();
      if(was_waiting!=ps_wifi_setup_waiting_hotspot())draw();
      unsigned next=ps_portal_advance(wifi_setup_step,&wifi_setup_auto_advanced,ps_wifi_setup_phone_connected());
      if(next!=wifi_setup_step){wifi_setup_step=next;draw();}
      bool success=ps_wifi_setup_state()==PS_PORTAL_CONNECTED;
      if(success && !wifi_setup_success_shown){wifi_setup_success_shown=true;snprintf(notice,sizeof(notice),"Wi-Fi connected successfully");draw();}
      else if(!success && wifi_setup_success_shown){wifi_setup_success_shown=false;notice[0]=0;draw();}
    }
    if (ps_wifi_setup_ready_to_stop()) {
      ps_wifi_setup_stop();
      wifi_setup_page = false;
      memset(wifi_setup_password, 0, sizeof(wifi_setup_password));
      snprintf(notice, sizeof(notice), "Wi-Fi connected successfully");
      draw();
    }
    message m;
    if (xQueueReceive(events, &m, pdMS_TO_TICKS(10))) {
      if (m.kind == UPLOAD_BEGIN) {
        close_app();
        xSemaphoreGive(upload_gate);
      } else if (m.kind == INSTALL) {
        close_app();
        char id[64];
        bool ok = storage_ready &&
                  (m.value == 1 ? ps_store_install_file(m.data, NULL, id)
                                : ps_store_install(m.data, m.size, id));
        if (m.value == 1)
          unlink(m.data);
        free(m.data);
        atomic_store(&upload_pending, false);
        if (ok)
          reset_preview();
        if (ok) refresh_app_list();
        printf("PASSPORT install %s%s%s\n", ok ? "OK" : "FAIL", ok ? " " : "",
               ok ? id : "");
      } else if (m.kind == COMMAND) {
        command(m.data);
        memset(m.data, 0, strlen(m.data));
        free(m.data);
      } else {
        int b = m.value % 8, ev = m.value / 8;
        if(m.sent_us && ev!=BSP_BTN_RELEASE)atomic_store(&input_pending_us,m.sent_us);
        if (ev == BSP_BTN_PRESS) {
          last_activity_us = esp_timer_get_time();
          if (alarm_ringing) {
            alarm_ringing = false;
            ps_notification_stop();
            notice[0] = 0;
            page = 0;
            settings_detail = false;
          }
          if (app_active && !mic_prompt && b >= 0 && b < BSP_BTN_COUNT) {
            app_button_down[b] = true;
            ps_runtime_event("on_button_down", b);
          }
          if (screen_is_off) {
            screen_is_off = false;
            bsp_display_backlight(settings_brightness);
            draw();
            continue;
          }
        }
        if (ev == BSP_BTN_DOUBLE && b == BSP_BTN_OK) {
          close_app();
          if (wifi_setup_page) ps_wifi_setup_stop();
          wifi_setup_page = false;
          memset(wifi_setup_password, 0, sizeof(wifi_setup_password));
          settings_detail = false;
          page = 0;
          notice[0] = 0;
          store_installed_id[0] = 0;
          publisher_pending = false;
          service_pending = false;
        } else if (ev == BSP_BTN_CLICK && app_active && mic_prompt) {
          if (b == BSP_BTN_OK) {
            mic_prompt = false;
            ps_runtime_audio_allowed(true);
            ps_runtime_event("on_permission", true);
            app_tick_time = esp_timer_get_time();
          } else {
            mic_prompt = false;
            ps_runtime_audio_allowed(false);
            ps_runtime_event("on_permission", false);
            app_tick_time = esp_timer_get_time();
          }
        } else if (ev == BSP_BTN_LONG && app_active && !mic_prompt) {
          ps_runtime_event("on_button_long", b);
        } else if (ev == BSP_BTN_RELEASE && app_active && !mic_prompt &&
                   b >= 0 && b < BSP_BTN_COUNT && app_button_down[b]) {
          ps_runtime_event("on_button_up", b);
          app_button_down[b] = false;
        } else if (ev == BSP_BTN_DOUBLE && app_active && !mic_prompt &&
                   b >= 0 && b < BSP_BTN_COUNT) {
          ps_runtime_event("on_button_double", b);
        } else if (ev == BSP_BTN_CLICK && b == BSP_BTN_OK && focus == 1 &&
                   store_installed_id[0]) {
          char installed_id[sizeof(store_installed_id)];
          snprintf(installed_id, sizeof(installed_id), "%s",
                   store_installed_id);
          store_installed_id[0] = 0;
          notice[0] = 0;
          open_app(installed_id);
        } else if (ev == BSP_BTN_CLICK) {
          if (alarm_ringing) {
            alarm_ringing = false;
            ps_notification_stop();
            notice[0] = 0;
            page = 0;
            settings_detail = false;
            draw();
            continue;
          }
          // A different classified action dismisses the install shortcut.
          store_installed_id[0] = 0;
          if (b == BSP_BTN_OK && focus == 2 && page == 1) {
            if (wifi_setup_page) {
              if(ps_wifi_setup_waiting_hotspot()) {
                ps_wifi_setup_stop();wifi_setup_page=ps_wifi_setup_start(wifi_setup_ssid,sizeof(wifi_setup_ssid),wifi_setup_password,sizeof(wifi_setup_password));
                wifi_setup_step=0;wifi_setup_auto_advanced=false;wifi_setup_success_shown=false;notice[0]=0;
                if(!wifi_setup_page)snprintf(notice,sizeof(notice),"Phone Wi-Fi setup could not start");
              } else wifi_setup_step=(wifi_setup_step+1)%3;
            } else if (!settings_detail) {
              if (settings_selection == SET_ALARM)
                alarm_field = alarm_config.enabled ? 0 : 2;
              if (settings_selection == SET_SOUND) sound_alarm=false;
              if (settings_selection == SET_STORAGE) {storage_usage_page=0;storage_usage_valid=false;ps_storage_usage_request();}
              settings_detail = true;
            }
            else if (settings_selection == SET_WIFI) {
              wifi_setup_page = true;
              wifi_setup_step=0;wifi_setup_auto_advanced=false;wifi_setup_success_shown=false;
              notice[0] = 0;
              if (!ps_wifi_setup_start(wifi_setup_ssid,
                                       sizeof(wifi_setup_ssid),
                                       wifi_setup_password,
                                       sizeof(wifi_setup_password))) {
                wifi_setup_page = false;
                snprintf(notice, sizeof(notice),
                         "Phone Wi-Fi setup could not start");
              }
            } else if (settings_selection == SET_UPDATE) {
              ps_update_info update;
              ps_update_read(&update);
              if (update.state == PS_UPDATE_READY) {
                if (ps_update_install()) {
                  draw();
                  vTaskDelay(pdMS_TO_TICKS(100));
                  esp_restart();
                }
              } else if (update.state == PS_UPDATE_AVAILABLE) {
                ps_update_start_download();
              } else if (update_url[0]) {
                ps_update_check(update_url);
              }
            } else if (settings_selection == SET_STORAGE) {
              ps_storage_usage_request();
            } else if (settings_selection == SET_SOUND) {
              sound_alarm=!sound_alarm;
            } else if (settings_selection == SET_ALARM) {
              alarm_field = (alarm_field + 1) % 3;
            }
          }
          notice[0] = 0;
          if (publisher_pending) {
            publisher_pending = false;
            bool ok = b == BSP_BTN_OK && ps_trust_add(pending_publisher);
            memset(pending_publisher, 0, sizeof(pending_publisher));
            printf("PASSPORT publisher %s\n", ok ? "TRUSTED" : "DENIED");
          } else if (service_pending) {
            service_pending = false;
            if (b == BSP_BTN_OK) {
              service_approved =
                  ps_net_local_grant(service_ip, service_http, service_ws);
              snprintf(notice, sizeof(notice), "Service %s. OK to browse",
                       service_approved ? "approved" : "denied");
            } else
              snprintf(notice, sizeof(notice), "Service denied");
          } else if (app_active) {
            if (!mic_prompt) {
              ps_runtime_event("on_button_short", b);
            }
          } else if (!page) {
            if (b == BSP_BTN_UP)
              focus = (focus + 2) % 3;
            else if (b == BSP_BTN_DOWN)
              focus = (focus + 1) % 3;
            else {
              page = 1;
              selection = 0;
              refresh_app_list();
              if (focus == 1) refresh_store_catalog();
            }
          } else if (focus == 0 && app_count) {
            if (page == 3) {
              if (b == BSP_BTN_OK) {
                bool ok = ps_store_remove(ids[selection], true);
                if (ok && !strcmp(default_app, ids[selection]))
                  default_app_store("");
                snprintf(notice, sizeof(notice), "%s",
                         ok ? "App and data deleted" : "Delete failed");
                refresh_app_list();
                selection = 0;
                page = 1;
              } else
                page = 2;
            } else if (page == 2) {
              if (b == BSP_BTN_UP)
                action_index = (action_index + 4) % 5;
              else if (b == BSP_BTN_DOWN)
                action_index = (action_index + 1) % 5;
              else if (action_index == 0)
                open_app(ids[selection]);
              else if (action_index == 1) {
                bool ok = default_app_select(ids[selection]);
                page = 1;
                snprintf(notice, sizeof(notice), "%s",
                         ok ? (!strcmp(default_app, ids[selection])
                                   ? "Default app saved"
                                   : "Default app cleared")
                            : "Could not set default app");
              } else if (action_index == 2) {
                bool was_default = !strcmp(default_app, ids[selection]);
                bool ok = ps_store_offload(ids[selection]);
                if (ok && was_default) default_app_store("");
                reset_preview();
                if (ok) refresh_app_list();
                page = 1;
                snprintf(notice, sizeof(notice), "%s",
                         ok ? "Code removed; data kept.\nReinstall from Store."
                            : "Code already offloaded");
              } else if (action_index == 3) {
                bool pin = !ps_store_is_pinned(ids[selection]);
                bool ok = ps_store_pin(ids[selection], pin);
                page = 1;
                snprintf(notice, sizeof(notice), "%s",
                         ok ? (pin ? "Offline pin enabled" : "Pin removed")
                            : "Pin failed");
              } else
                page = 3;
            } else if (b == BSP_BTN_OK) {
              page = 2;
              action_index = 0;
            } else
              selection = (selection + (b == BSP_BTN_UP ? app_count - 1 : 1)) %
                          app_count;
          } else if (focus == 1 && !store_request) {
            if (!service_approved && !*store_url)
              service_pending = true;
            else {
              cJSON *apps = cJSON_GetObjectItemCaseSensitive(catalog, "apps");
              int count = cJSON_GetArraySize(apps);
              if (b != BSP_BTN_OK && count)
                catalog_selection =
                    (catalog_selection + (b == BSP_BTN_UP ? count - 1 : 1)) %
                    count;
              else if (b == BSP_BTN_OK) {
                char url[512];
                if (!count) {
                  if (*store_url)
                    snprintf(url, sizeof(url), "%s", store_url);
                  else
                    snprintf(url, sizeof(url), "http://%s:%u/index.json",
                             service_ip, service_http);
                  store_request = 1;
                } else {
                  cJSON *a = cJSON_GetArrayItem(apps, catalog_selection),
                        *v = cJSON_GetObjectItemCaseSensitive(a, "url"),
                        *app_id = cJSON_GetObjectItemCaseSensitive(a, "id"),
                        *app_version = cJSON_GetObjectItemCaseSensitive(a, "version");
                  if (cJSON_IsString(app_id) && cJSON_IsString(app_version) &&
                      installed_app_is_current(app_id->valuestring,
                                               app_version->valuestring)) {
                    snprintf(store_installed_id, sizeof(store_installed_id),
                             "%s", app_id->valuestring);
                    snprintf(notice, sizeof(notice), "%s",
                             "Already up to date. OK: open app.");
                    draw();
                    continue;
                  }
                  const char *target = cJSON_IsString(v) ? v->valuestring : "";
                  char base[512];
                  if (*store_url)
                    snprintf(base, sizeof(base), "%s", store_url);
                  else
                    snprintf(base, sizeof(base), "http://%s:%u/index.json",
                             service_ip, service_http);
                  if (!ps_redirect_url(base, target, url, sizeof(url)))
                    url[0] = 0;
                  store_request = 2;
                }
                bool ok = ps_net_fetch(
                    NULL, url,
                    store_request == 2 ? PS_NET_PACKAGE_GENERATION : 0xffffffff,
                    true);
                if (!ok) store_request=0;
                snprintf(notice, sizeof(notice), "%s",
                         ok ? "Loading Store..."
                            : "Offline / busy / endpoint denied");
              }
            }
          } else if (focus == 2 && wifi_setup_page) {
            if(b==BSP_BTN_UP)wifi_setup_step=(wifi_setup_step+2)%3;
            else if(b==BSP_BTN_DOWN)wifi_setup_step=(wifi_setup_step+1)%3;
          } else if (focus == 2 && !settings_detail) {
            if (b == BSP_BTN_UP)
              settings_selection = (settings_selection + SET_COUNT - 1) % SET_COUNT;
            else if (b == BSP_BTN_DOWN)
              settings_selection = (settings_selection + 1) % SET_COUNT;
          } else if (focus == 2 && settings_detail && settings_selection == SET_STORAGE) {
            unsigned pages=3+storage_usage.count;
            if(b==BSP_BTN_UP)storage_usage_page=(storage_usage_page+pages-1)%pages;
            else if(b==BSP_BTN_DOWN)storage_usage_page=(storage_usage_page+1)%pages;
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_DISPLAY) {
            if (b == BSP_BTN_UP) settings_adjust_brightness(10);
            else if (b == BSP_BTN_DOWN) settings_adjust_brightness(-10);
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_SOUND) {
            if (b == BSP_BTN_UP) settings_adjust_volume(1);
            else if (b == BSP_BTN_DOWN) settings_adjust_volume(-1);
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_TIMEOUT) {
            if (b == BSP_BTN_UP) settings_adjust_timeout(1);
            else if (b == BSP_BTN_DOWN) settings_adjust_timeout(-1);
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_APPEARANCE &&
                     (b == BSP_BTN_UP || b == BSP_BTN_DOWN)) {
            settings_toggle_display_mode();
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_ALARM && b != BSP_BTN_OK) {
            alarm_adjust(alarm_field, b == BSP_BTN_UP ? 1 : -1);
          } else if (focus == 2 && settings_detail &&
                     settings_selection == SET_LANGUAGE &&
                     (b == BSP_BTN_UP || b == BSP_BTN_DOWN)) {
            settings_toggle_language();
          } else if (focus == 2 && settings_detail && b != BSP_BTN_OK) {
            settings_selection = (settings_selection +
                                  (b == BSP_BTN_UP ? SET_COUNT - 1 : 1)) %
                                 SET_COUNT;
          }
        }
      }
      draw();
    }
    ps_net_result result;
    while (ps_net_poll(&result)) {
      if (result.generation == PS_UPDATE_MANIFEST_GENERATION) {
        ps_update_accept_manifest(result.status, result.data, result.size);
        if (result.status != 200) {
          ps_update_info update;
          ps_update_read(&update);
          snprintf(notice, sizeof(notice), "Update check failed: %.64s",
                   update.error);
          ESP_LOGE("passport", "OTA manifest request failed status=%d: %s",
                   result.status, update.error);
        }
        draw();
      } else if (result.generation == PS_UPDATE_IMAGE_GENERATION) {
        ps_update_finish_download(result.status, result.size);
        if (result.status != 200) {
          ps_update_info update;
          ps_update_read(&update);
          snprintf(notice, sizeof(notice), "Update download failed: %.64s",
                   update.error);
          ESP_LOGE("passport",
                   "OTA image request failed status=%d received=%u written=%u/%u: %s",
                   result.status, (unsigned)result.size,
                   (unsigned)update.downloaded, (unsigned)update.image_size,
                   update.error);
        }
        draw();
      } else if (result.generation == 0xffffffff ||
          result.generation == PS_NET_PACKAGE_GENERATION) {
        if (store_request == 1) {
          cJSON *next = result.status == 200
                            ? ps_json_parse(result.data, result.size)
                            : NULL;
          cJSON *apps = cJSON_GetObjectItemCaseSensitive(next, "apps");
          if (cJSON_IsArray(apps) && cJSON_GetArraySize(apps) <= 8) {
            cJSON_Delete(catalog);
            catalog = next;
            notice[0] = 0;
            catalog_selection = 0;
          } else {
            cJSON_Delete(next);
            snprintf(notice, sizeof(notice), "Catalog unavailable / invalid");
          }
        } else if (store_request == 2) {
          close_app();
          page = 1;
          focus = 1;
          char id[64];
          bool ok = false;
          cJSON *a = cJSON_GetArrayItem(
                    cJSON_GetObjectItemCaseSensitive(catalog, "apps"),
                    catalog_selection),
                *sha = cJSON_GetObjectItemCaseSensitive(a, "sha256"),
                *expected = cJSON_GetObjectItemCaseSensitive(a, "id");
          if (result.status == 200 && result.file && cJSON_IsString(sha) &&
              cJSON_IsString(expected)) {
            char hash[65];
            if (ps_package_file_hash(result.data, hash) &&
                !strcmp(hash, sha->valuestring))
              ok =
                  ps_store_install_file(result.data, expected->valuestring, id);
          }
          if (ok)
            reset_preview();
          if (ok) {
            refresh_app_list();
            snprintf(store_installed_id, sizeof(store_installed_id), "%s", id);
            snprintf(notice, sizeof(notice), "%s", "Installed. OK: open app.");
          } else {
            store_installed_id[0] = 0;
            snprintf(notice, sizeof(notice), "%s",
                     "Install failed / signature or hash");
          }
        }
        store_request = 0;
        measure("http_store");
        draw();
      } else if (app_active) {
        ps_runtime_result(result.generation, result.status, result.data,
                          result.size);
        measure("https_app");
        draw();
      }
      if (result.file && result.data)
        unlink(result.data);
      free(result.data);
    }
    poll_app_websocket();
    bool want_audio = app_active && !mic_prompt &&
                      ps_runtime_view()->microphone && !*ps_runtime_error();
    bool audio_session = app_active && !mic_prompt &&
                         ps_runtime_view()->audio_stream && !*ps_runtime_error();
    if (!ps_media_active() && (want_audio || (audio_session && !ps_audio_stream_active()))) {
      if (!(audio_session ? (want_audio ? ps_audio_stream_start() : ps_audio_stream_playback_start()) : ps_audio_start())) {
        ESP_LOGW("passport", "AUDIO start failed; microphone consent remains granted");
        ps_runtime_audio_error(PS_AUDIO_ERROR_START_FAILED);
      } else
        ps_runtime_event("on_audio", ps_audio_level());
    } else if (!audio_session && !ps_audio_stream_prepared() &&
               !ps_audio_stream_active())
      ps_audio_stop();
    if (want_audio && (package.permissions & PS_AUDIO_STREAM)) {
      uint8_t opus_frame[512]; size_t opus_size = 0;
      for (unsigned i = 0; i < 2 &&
           ps_audio_stream_poll(opus_frame, sizeof(opus_frame), &opus_size); i++) {
        ps_runtime_audio_frame(opus_frame, opus_size);
        opus_size = 0;
      }
    }
    int64_t now = esp_timer_get_time();
    if (alarm_dirty && now - alarm_dirty_us >= 1500000) alarm_save();
    struct tm alarm_time;
    if (!alarm_ringing && system_time_valid(&alarm_time) &&
        ps_alarm_due(&alarm_config, &alarm_time, &alarm_last_day)) {
      close_app();
      page = 0;
      settings_detail = false;
      screen_is_off = false;
      bsp_display_backlight(settings_brightness);
      last_activity_us = now;
      alarm_ringing = true;
      snprintf(notice, sizeof(notice), "%s",
               "Alarm is ringing. Press any key to dismiss.");
      alarm_audio_pending=ps_media_active();
      if (!alarm_audio_pending && !ps_notification_start())
        ESP_LOGE("passport", "ALARM audio notification could not start");
      ESP_LOGI("passport", "ALARM fired %04d-%03d %02d:%02d",
               alarm_time.tm_year + 1900, alarm_time.tm_yday + 1,
               alarm_time.tm_hour, alarm_time.tm_min);
      draw();
    }
    if (!alarm_ringing) alarm_audio_pending=false;
    if (alarm_ringing && alarm_audio_pending && !ps_media_active()) {
      alarm_audio_pending=false;
      if (!ps_notification_start())ESP_LOGE("passport","ALARM deferred audio could not start");
    }
    if (app_active && !mic_prompt && now - app_tick_time >= frame_period_us) {
      int64_t begin = esp_timer_get_time();
      ps_runtime_event("on_tick", (now - app_tick_time) / 1000);
      int64_t elapsed = esp_timer_get_time() - begin;
      if (elapsed > frame_max)
        frame_max = elapsed;
      frame_count++;
      app_tick_time = now;
      const ps_view *current = ps_runtime_view();
      if (current->image_count != display_view.image_count ||
          memcmp(current->images, display_view.images,
                 current->image_count * sizeof(ps_image_command)) ||
          current->count != display_view.count ||
          current->text_color != display_view.text_color ||
          memcmp(current->commands, display_view.commands,
                 current->count * sizeof(ps_draw)) ||
          strcmp(current->text, display_view.text))
        draw();
    }
    if (app_active && *ps_runtime_error()) {
      char why[128];
      snprintf(why, sizeof(why), "%s", ps_runtime_error());
      close_app();
      page = 1;
      focus = 0;
      refresh_app_list();
      snprintf(notice, sizeof(notice), "App stopped safely:\n%.120s", why);
      draw();
    }
    if (now - refresh > 1000000) {
      refresh = now;
      if (!app_active)
        draw();
    }
  }
}
