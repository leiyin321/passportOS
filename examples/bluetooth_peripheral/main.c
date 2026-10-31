#include "passport_native_hid.h"
#include "gesture.h"
#include <stddef.h>
static const passport_native_host_hid_v2 *H;
static const passport_native_host_v1 *B;
static int zh, light, menu, mode = -1, pending_reset, last_state = -1;
static unsigned failures;
static uint32_t retry_at;
static bt_gesture gesture;
static char screen[1024];
static const char *tr(const char *en, const char *cn) { return zh ? cn : en; }
static void add(const char *s) {
  unsigned n = 0;
  while (screen[n])
    n++;
  while (*s && n + 1 < sizeof(screen))
    screen[n++] = *s++;
  screen[n] = 0;
}
static void draw(void) {
  static const char *en[] = {"Off",
                             "Starting...",
                             "Pair in iPhone Bluetooth",
                             "Connected; pairing...",
                             "Ready",
                             "Bluetooth error; retry",
                             "Stopping...",
                             "OS update required"};
  static const char *cn[] = {"已关闭",
                             "正在启动...",
                             "请在 iPhone 蓝牙中配对",
                             "已连接，正在配对...",
                             "已就绪",
                             "蓝牙错误，请重试",
                             "正在停止...",
                             "请更新系统"};
  unsigned state = H->hid_state();
  if (state > 7)
    state = 7;
  screen[0] = 0;
  add(tr("BLUETOOTH PERIPHERALS\n", "蓝牙外设\n"));
  add(zh ? cn[state] : en[state]);
  add("\nPassport Remote\n\n");
  if (mode < 0) {
    static const char *items_en[] = {"iPhone gesture buttons",
                                     "Forget pairing"};
    static const char *items_cn[] = {"iPhone 手势按钮映射", "清除配对"};
    for (int i = 0; i < 2; i++) {
      add(i == menu ? "> " : "  ");
      add(zh ? items_cn[i] : items_en[i]);
      add("\n");
    }
    add(tr("\nUP/DOWN select  OK open", "\n上下选择，OK 打开"));
  } else if (mode == 1) {
    add(tr("Forget stored Bluetooth bonds?\nAlso forget Passport Remote\non "
           "the iPhone before pairing.\n\nOK again: confirm\nHold OK: cancel",
           "清除已保存的蓝牙配对？\n请同时在 iPhone 上忽略\nPassport "
           "Remote，再重新配对。\n\n再次按 OK 确认\n长按 OK 取消"));
  } else {
    add(tr(
        "IPHONE GESTURE BUTTONS\nUP: button 2\nDOWN: button 3\nOK: button 1 "
        "(single tap)\n\nIn iPhone AssistiveTouch >\nDevices > Passport "
        "Remote,\nassign 2/3 to saved gestures.\nHold OK: menu",
        "iPhone 手势按钮映射\nUP：按钮 2（上划手势）\nDOWN：按钮 "
        "3（下划手势）\nOK：按钮 1（单击）\n\n请在 iPhone "
        "辅助触控设备中，\n把按钮 2/3 绑定到自定义手势。\n长按 OK 返回菜单"));
  }
  if (failures >= 20)
    add(tr("\nBusy/disconnected; retry", "\n连接忙或已断开，请重试"));
  B->clear(light ? 0xf1f6fb : 0x101a29);
  B->rect(4, 0, 4, 264, 0x4ca6f0);
  B->text(screen, light ? 0x102334 : 0xf4f8ff);
}
static void release_inputs(void) {
  const unsigned char zero[4] = {0};
  H->hid_report(1, zero, 4);
  gesture.active = false;
}
static void start_mode(void) {
  unsigned state = H->hid_state();
  if (state == 0 || state == 5 || state == 7)
    H->hid_start(0);
}
static void event(uint32_t e, int32_t v) {
  uint32_t now = B->millis();
  if (e == PASSPORT_NATIVE_TICK) {
    unsigned state = H->hid_state();
    if (pending_reset && (state == 0 || state == 5) &&
        (int32_t)(now - retry_at) >= 0) {
      retry_at = now + 1000;
      if (H->hid_start(1))
        pending_reset = 0;
    }
    if (gesture.active) {
      if (state != 4) {
        gesture.active = false;
        draw();
      } else {
        unsigned id, n;
        unsigned char data[4];
        if (bt_gesture_packet(&gesture, now, &id, data, &n)) {
          if (H->hid_report(id, data, n)) {
            failures = 0;
            bt_gesture_commit(&gesture, now);
            if (!gesture.active)
              draw();
          } else if (++failures >= 20) {
            release_inputs();
            H->hid_stop();
            draw();
          }
        }
      }
    }
    if ((int)state != last_state) {
      last_state = state;
      draw();
    }
    return;
  }
  if (e == PASSPORT_NATIVE_LONG && v == 2) {
    release_inputs();
    pending_reset = 0;
    mode = -1;
    draw();
    return;
  }
  if (e != PASSPORT_NATIVE_SHORT || v < 0 || v > 2)
    return;
  if (mode < 0) {
    if (v < 2)
      menu = (menu + 1) % 2;
    else {
      mode = menu;
      failures = 0;
      if (mode == 0)
        start_mode();
    }
    draw();
    return;
  }
  if (mode == 1) {
    if (v == 2) {
      pending_reset = 1;
      retry_at = now;
      H->hid_stop();
      mode = 0;
    }
    draw();
    return;
  }
  if (H->hid_state() != 4) {
    if (v == 2)
      start_mode();
    draw();
    return;
  }
  if (gesture.active)
    return;
  failures = 0;
  bt_gesture_begin(&gesture, BT_BUTTON, v == 0 ? 2 : v == 1 ? 4 : 1, now);
  draw();
}
static void stop(void) {
  release_inputs();
  H->hid_stop();
}
int passport_app_entry(const passport_native_host_v1 *host,
                       passport_native_app_v1 *app) {
  if (!host || !app || host->abi != 2 ||
      host->size < sizeof(passport_native_host_hid_v2))
    return -1;
  H = (const void *)host;
  B = host;
  if (!H->hid_start || !H->hid_state || !H->hid_report || !H->hid_stop)
    return -1;
  zh = host->language()[0] == 'z';
  light = host->display_mode()[0] == 'l';
  menu = 0;
  pending_reset = failures = 0;
  mode = -1;
  last_state = -1;
  gesture = (bt_gesture){0};
  passport_native_app_v2 *out = (void *)app;
  *out = (passport_native_app_v2){{2, sizeof(*out), event, stop}, 0};
  draw();
  return 0;
}
