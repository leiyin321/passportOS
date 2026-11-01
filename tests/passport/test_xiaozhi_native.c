#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "passport_native_v2.h"
#include "native_json.h"
static char ui[512], last[2049], headers_copy[1025];
static int online, connected, requests, sends, binaries, audio, plays, zh_ui,
    light_ui;
static uint32_t foreground;
static unsigned draw_count, ws_opens;
static unsigned char pixels[320][240];
static int text(const char *s, uint32_t c) {
  foreground = c;
  snprintf(ui, sizeof(ui), "%s", s);
  return 0;
}
static int clear(uint32_t c) {
  draw_count = 1;
  memset(pixels, c == 0x090f1c ? 0 : 240, sizeof(pixels));
  return 0;
}
static int rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= 240 && y + h <= 320 &&
         c <= 0xffffff);
  assert(++draw_count <= 192);
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
      pixels[yy][xx] = c == 0xffffff ? 255 : 0;
  return 0;
}
static uint32_t millis(void) { return 0; }
static const char *language(void) { return zh_ui ? "zh-CN" : "en"; }
static const char *mode(void) { return light_ui ? "light" : "dark"; }
static int network(void) { return online; }
static int http(const char *m, const char *u, const char *h, const void *b,
                uint32_t n) {
  assert(!strcmp(m, "POST") && strstr(u, "/xiaozhi/ota/") &&
         strstr(h, "Device-Id:") && b && n);
  requests++;
  return 1;
}
static int wsopen(const char *u, const char *h) {
  ws_opens++;
  assert(!strncmp(u, "wss://", 6));
  snprintf(headers_copy, sizeof(headers_copy), "%s", h);
  return 1;
}
static int send(const void *d, uint32_t n, int binary) {
  assert(d && n <= 2048);
  sends++;
  if (binary)
    binaries++;
  else {
    memcpy(last, d, n);
    last[n] = 0;
  }
  return 1;
}
static int wsconnected(void) { return connected; }
static void wsclose(void) { connected = 0; }
static int identity(char *d, uint32_t dc, char *c, uint32_t cc) {
  snprintf(d, dc, "00:11:22:33:44:55");
  snprintf(c, cc, "test-client");
  return 1;
}
static int get(const char *k, char *out, uint32_t n) {
  (void)k;
  (void)out;
  (void)n;
  return 0;
}
static int set(const char *k, const char *v) {
  (void)k;
  (void)v;
  return 1;
}
static int audiomode(int capture) {
  audio = capture;
  return 1;
}
static int play(const void *d, uint32_t n) {
  assert(d && n);
  plays++;
  return 1;
}
static const passport_native_host_v2 host = {
    {2, sizeof(host), text, clear, rect, millis, language, mode},
    network,
    http,
    wsopen,
    send,
    wsconnected,
    wsclose,
    ps_native_json_get,
    identity,
    get,
    set,
    audiomode,
    play};
static void deliver(passport_native_app_v2 *a, unsigned kind, int status,
                    const char *s) {
  a->data(kind, status, s, strlen(s));
}
static void configure(passport_native_app_v2 *a) {
  deliver(a, 1, 200,
          "{\"websocket\":{\"url\":\"wss://api.tenclass.net/xiaozhi/v1/"
          "\",\"token\":\"Bearer fixture-token\"}}");
  assert(strstr(headers_copy, "Authorization: Bearer fixture-token\r\n"));
  assert(!strstr(headers_copy, "Bearer Bearer"));
  connected = 1;
  deliver(a, 1, 101, "");
  a->base.event(PASSPORT_NATIVE_TICK, 100);
  assert(strstr(last, "\"type\":\"hello\""));
}
static void hello(passport_native_app_v2 *a) {
  deliver(a, 2, 1,
          "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":"
          "\"fixture-session\",\"audio_params\":{\"format\":\"opus\","
          "\"channels\":1,\"sample_rate\":24000,\"frame_duration\":60}}");
  assert(strstr(ui, "Ready: hold OK"));
}
int main(int argc, char **argv) {
  passport_native_app_v2 app = {0};
  passport_native_host_v1 old = {.abi = 1, .size = sizeof(old)};
  assert(passport_app_entry(&old, &app.base) < 0);
  assert(passport_app_entry(&host.base, &app.base) == 0 && app.base.abi == 2 &&
         app.data);
  app.base.event(PASSPORT_NATIVE_PERMISSION, 0);
  assert(strstr(ui, "permission denied"));
  app.base.stop();
  assert(passport_app_entry(&host.base, &app.base) == 0);
  app.base.event(PASSPORT_NATIVE_PERMISSION, 1);
  assert(strstr(ui, "Waiting for Wi-Fi") && requests == 0);
  online = 1;
  app.base.event(PASSPORT_NATIVE_TICK, 1000);
  assert(requests == 1);
  deliver(&app, 1, 500, "");
  assert(strstr(ui, "Retrying service"));
  app.base.event(PASSPORT_NATIVE_TICK, 1000);
  assert(requests == 2);
  configure(&app);
  hello(&app);
  assert(audio == 0);
  app.base.event(PASSPORT_NATIVE_SHORT, 1);
  app.base.event(PASSPORT_NATIVE_SHORT, 2);
  assert(strstr(last, "\"state\":\"detect\"") && strstr(last, "Hi XiaoZhi"));
  app.base.event(PASSPORT_NATIVE_SHORT, 0);
  app.base.event(PASSPORT_NATIVE_PRESS, 2);
  assert(audio == 1 && strstr(last, "\"state\":\"start\""));
  deliver(&app, 4, 0, "opus-frame");
  assert(binaries == 1);
  app.base.event(PASSPORT_NATIVE_RELEASE, 2);
  assert(audio == 0 && strstr(last, "\"state\":\"stop\""));
  deliver(&app, 4, 0, "after-release");
  assert(binaries == 1);
  deliver(&app, 2, 1, "{\"type\":\"stt\",\"text\":\"hello native\"}");
  assert(strstr(ui, "hello native"));
  deliver(&app, 2, 1, "{\"type\":\"tts\",\"state\":\"start\"}");
  deliver(&app, 2, 1,
          "{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"native "
          "reply\"}");
  assert(strstr(ui, "native reply"));
  deliver(&app, 3, 2, "opus-reply");
  assert(plays == 1);
  deliver(&app, 2, 1, "{\"type\":\"tts\",\"state\":\"stop\"}");
  deliver(&app, 3, 2, "late-frame");
  assert(plays == 1);
  app.base.event(PASSPORT_NATIVE_AUDIO_ERROR, 1);
  assert(audio == -1 && strstr(ui, "Audio unavailable"));
  app.base.event(PASSPORT_NATIVE_SHORT, 2);
  configure(&app);
  deliver(&app, 2, 1,
          "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":"
          "\"x\",\"audio_params\":{\"format\":\"opus\",\"channels\":2,\"sample_"
          "rate\":24000}}");
  assert(strstr(ui, "Unsupported"));
  app.base.event(PASSPORT_NATIVE_SHORT, 2);
  app.base.event(PASSPORT_NATIVE_TICK, 16000);
  assert(strstr(ui, "timeout"));
  int before = requests;
  app.base.event(PASSPORT_NATIVE_TICK, 60000);
  assert(requests == before);
  app.base.stop();
  assert(!connected && audio == -1);
  zh_ui = light_ui = 1;
  assert(passport_app_entry(&host.base, &app.base) == 0);
  assert(strstr(ui, "语音对话") && foreground == 0x102334);
  app.base.event(PASSPORT_NATIVE_PERMISSION, 0);
  assert(strstr(ui, "麦克风权限被拒绝"));
  app.base.stop();
  for (int language_mode = 0; language_mode < 2; language_mode++)
    for (int theme = 0; theme < 2; theme++) {
      zh_ui = language_mode;
      light_ui = theme;
      online = 1;
      assert(passport_app_entry(&host.base, &app.base) == 0);
      app.base.event(PASSPORT_NATIVE_PERMISSION, 1);
      unsigned opened = ws_opens;
      deliver(&app, 1, 200,
              "{\"activation\":{\"code\":\"123456\"},\"websocket\":{\"url\":"
              "\"wss://api.tenclass.net/xiaozhi/v1/\"}}");
      assert(strstr(ui, "123456") && strstr(ui, "https://xiaozhi.me/console/"));
      assert(
          strstr(ui, zh_ui ? "登录后台输入激活码" : "Log in, enter this code"));
      assert(strstr(ui, zh_ui ? "扫码或输入上方网址" : "Scan or type the URL"));
      assert(draw_count == 163 && ws_opens == opened &&
             pixels[148][37] == 255 && pixels[168][57] == 0);
      int prior_audio = audio;
      app.base.event(PASSPORT_NATIVE_PRESS, 2);
      app.base.event(PASSPORT_NATIVE_RELEASE, 2);
      assert(audio == prior_audio);
      app.base.event(PASSPORT_NATIVE_AUDIO_ERROR, 1);
      assert(strstr(ui, "123456"));
      if (argc > 1 && !language_mode && !theme) {
        FILE *f = fopen(argv[1], "wb");
        assert(f);
        fprintf(f, "P5\n240 320\n255\n");
        assert(fwrite(pixels, 1, sizeof(pixels), f) == sizeof(pixels));
        fclose(f);
      }
      int prior_requests = requests;
      app.base.event(PASSPORT_NATIVE_SHORT, 2);
      assert(requests == prior_requests + 1);
      configure(&app);
      assert(!strstr(ui, "123456"));
      app.base.stop();
    }
  puts("Native XiaoZhi PASS "
       "consent/offline/HTTP/retry/auth/hello/PTT/Opus/STT/TTS/errors/timeouts/"
       "stop");
}
