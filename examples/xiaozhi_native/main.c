/* XiaoZhi protocol belongs to this application. OS exports generic services
 * only. */
#include "passport_native_v2.h"
static const passport_native_host_v2 *H;
static int zh, light, page, consent, held, listening, speaking, hello_sent,
    attempts;
static uint32_t elapsed, deadline;
enum { CONSENT, WIFI, CONFIG, CONNECT, HELLO, READY, FAILED, RETRY };
static int phase;
static char device[18], client[37], url[501], token[257], session[96];
static char activation[32], heard[128], answer[192], detail[100];
static char scratch[1200], screen[512];
static unsigned length(const char *s) {
  unsigned n = 0;
  while (s[n])
    n++;
  return n;
}
static int equal(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}
static void copy(char *out, unsigned cap, const char *s) {
  unsigned n = length(s);
  if (n >= cap)
    n = cap - 1;
  while (n && ((unsigned char)s[n] & 0xc0) == 0x80)
    n--;
  for (unsigned i = 0; i < n; i++)
    out[i] = s[i];
  out[n] = 0;
}
static int append(char *out, unsigned cap, const char *s) {
  unsigned n = length(out), m = length(s);
  if (n + m >= cap)
    return 0;
  for (unsigned i = 0; i <= m; i++)
    out[n + i] = s[i];
  return 1;
}
static int quote(char *out, unsigned cap, const char *s) {
  if (!append(out, cap, "\""))
    return 0;
  for (; *s; s++) {
    char c[7] = {*s, 0};
    if (*s == '"' || *s == '\\') {
      c[0] = '\\';
      c[1] = *s;
      c[2] = 0;
    } else if ((unsigned char)*s < 32) {
      const char *hex = "0123456789abcdef";
      c[0] = '\\';
      c[1] = 'u';
      c[2] = '0';
      c[3] = '0';
      c[4] = hex[((unsigned char)*s) >> 4];
      c[5] = hex[((unsigned char)*s) & 15];
      c[6] = 0;
    }
    if (!append(out, cap, c))
      return 0;
  }
  return append(out, cap, "\"");
}
static const char *tr(const char *en, const char *cn) { return zh ? cn : en; }
static void draw(void) {
  static const char *en[] = {"Microphone permission",    "Waiting for Wi-Fi",
                             "Checking XiaoZhi service", "Connecting WSS",
                             "Waiting for hello",        "Ready: hold OK",
                             "Connection failed",        "Retrying service..."};
  static const char *cn[] = {
      "麦克风权限",  "等待网络...",  "正在检查小智服务...",
      "正在连接...", "等待服务响应", "按住 OK 说话",
      "连接失败",    "正在重试..."};
  screen[0] = 0;
  append(screen, sizeof(screen), "XIAOZHI NATIVE\n");
  append(screen, sizeof(screen),
         page == 0   ? tr("VOICE COMPANION", "语音对话")
         : page == 1 ? tr("CONVERSATION", "对话记录")
                     : tr("SERVICE", "服务设置"));
  append(screen, sizeof(screen), "\n\n");
  append(screen, sizeof(screen),
         speaking ? tr("Speaking...", "正在播放...")
         : listening ? tr("Listening... release OK", "正在聆听，松开 OK 停止")
         : zh ? cn[phase]
              : en[phase]);
  char clipped[79];
  copy(clipped, sizeof(clipped), detail);
  append(screen, sizeof(screen), "\n");
  append(screen, sizeof(screen), clipped);
  if (activation[0]) {
    append(screen, sizeof(screen), tr("\nActivation: ", "\n激活码："));
    append(screen, sizeof(screen), activation);
  }
  append(screen, sizeof(screen), "\n\n");
  if (page == 2) {
    append(screen, sizeof(screen),
           "api.tenclass.net\n\nOS 1.0.41+ / Native ABI 2\nOpus mono / 16k / "
           "60ms\n\n");
    append(screen, sizeof(screen), tr("OK: retry service", "OK：重新连接服务"));
  } else {
    append(screen, sizeof(screen), tr("YOU: ", "我："));
    copy(clipped, sizeof(clipped), heard);
    append(screen, sizeof(screen), clipped);
    append(screen, sizeof(screen), "\n\n");
    append(screen, sizeof(screen), tr("AI: ", "小智："));
    /* Keep footer inside the managed text budget, with a UTF-8 boundary. */
    copy(clipped, sizeof(clipped), answer);
    append(screen, sizeof(screen), clipped);
  }
  if (page == 1)
    append(screen, sizeof(screen), tr("\nOK: say hello", "\nOK：发送你好"));
  append(screen, sizeof(screen),
         tr("\n\nHold OK: talk\nUP/DOWN: pages\nDouble OK: home",
            "\n\n按住 OK 说话，松开停止\n上下键切换页面\n双击 OK 返回"));
  H->base.clear(light ? 0xf1f6fb : 0x090f1c);
  H->base.rect(6, 52, 228, 70, light ? 0xdceef7 : 0x142d42);
  H->base.rect(6, 52, 4, 70,
               speaking    ? 0xffc56e
               : listening ? 0xc59bff
                           : 0x54d9bd);
  H->base.rect(6, 132, 228, 128, light ? 0xffffff : 0x152033);
  H->base.text(screen, light ? 0x102334 : 0xf6faff);
}
static void fail(const char *en, const char *cn) {
  phase = FAILED;
  held = 0;
  listening = 0;
  speaking = 0;
  if (consent)
    H->audio_mode(-1);
  copy(detail, sizeof(detail), tr(en, cn));
  draw();
}
static void fail_code(const char *en, const char *cn, int32_t status) {
  fail(en, cn);
  char number[13];
  unsigned n = 0;
  uint32_t v = status < 0 ? 0u - (uint32_t)status : (uint32_t)status;
  do {
    number[n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  append(detail, sizeof(detail), " [");
  if (status < 0)
    append(detail, sizeof(detail), "-");
  while (n) {
    char one[2] = {number[--n], 0};
    append(detail, sizeof(detail), one);
  }
  append(detail, sizeof(detail), "]");
  draw();
}
static int message(const char *type, const char *state) {
  scratch[0] = 0;
  if (!append(scratch, sizeof(scratch), "{\"type\":") ||
      !quote(scratch, sizeof(scratch), type) ||
      !append(scratch, sizeof(scratch), ",\"session_id\":") ||
      !quote(scratch, sizeof(scratch), session))
    return 0;
  if (state && (!append(scratch, sizeof(scratch), ",\"state\":") ||
                !quote(scratch, sizeof(scratch), state)))
    return 0;
  if (state && equal(state, "start"))
    append(scratch, sizeof(scratch), ",\"mode\":\"manual\"");
  if (state && equal(state, "detect")) {
    if (!append(scratch, sizeof(scratch), ",\"text\":") ||
        !quote(scratch, sizeof(scratch), tr("Hi XiaoZhi", "你好小智")))
      return 0;
  }
  return append(scratch, sizeof(scratch), "}") &&
         H->ws_send(scratch, length(scratch), 0);
}
static void begin_listen(void) {
  if (phase != READY || !held || listening)
    return;
  if (!H->ws_connected()) {
    fail("WSS disconnected; OK to retry", "连接已断开，按 OK 重试");
    return;
  }
  if (speaking) {
    message("abort", 0);
    speaking = 0;
  }
  if (!consent) {
    fail("Microphone permission denied", "麦克风权限被拒绝");
    return;
  }
  if (!H->audio_mode(1)) {
    fail("Audio unavailable; release OK", "麦克风暂不可用，请重试");
    return;
  }
  if (!message("listen", "start")) {
    H->audio_mode(-1);
    fail("Listen request failed", "连接失败，请重试");
    return;
  }
  listening = 1;
  detail[0] = 0;
  draw();
}
static void configure(void) {
  if (!consent)
    return;
  if (!H->network_ready()) {
    phase = WIFI;
    deadline = elapsed + 1000;
    draw();
    return;
  }
  if (!H->identity(device, sizeof(device), client, sizeof(client))) {
    fail("Device identity unavailable", "设备信息不可用");
    return;
  }
  attempts++;
  phase = CONFIG;
  hello_sent = 0;
  session[0] = 0;
  activation[0] = 0;
  detail[0] = 0;
  scratch[0] = 0;
  append(scratch, sizeof(scratch), "Activation-Version: 1\r\nDevice-Id: ");
  append(scratch, sizeof(scratch), device);
  append(scratch, sizeof(scratch), "\r\nClient-Id: ");
  append(scratch, sizeof(scratch), client);
  append(scratch, sizeof(scratch),
         "\r\nUser-Agent: PassportOS-Native/1.0.4\r\nAccept-Language: ");
  append(scratch, sizeof(scratch), zh ? "zh-CN" : "en");
  append(scratch, sizeof(scratch), "\r\nContent-Type: application/json\r\n");
  static const char body[] =
      "{\"application\":{\"name\":\"Passport OS XiaoZhi "
      "Native\",\"version\":\"1.0.4\"},\"board\":{\"type\":\"ai-passport\","
      "\"name\":\"FoloToy AI Passport\"}}";
  if (!H->http_request("POST", "https://api.tenclass.net/xiaozhi/ota/", scratch,
                       body, sizeof(body) - 1)) {
    fail("Service request unavailable; OK retry", "服务暂不可用，按 OK 重试");
    return;
  }
  deadline = elapsed + 15000;
  draw();
}
static void open_socket(void) {
  scratch[0] = 0;
  if (token[0]) {
    append(scratch, sizeof(scratch), "Authorization: ");
    if (!(token[0] == 'B' && token[1] == 'e' && token[2] == 'a' &&
          token[3] == 'r' && token[4] == 'e' && token[5] == 'r' &&
          token[6] == ' '))
      append(scratch, sizeof(scratch), "Bearer ");
    append(scratch, sizeof(scratch), token);
    append(scratch, sizeof(scratch), "\r\n");
  }
  append(scratch, sizeof(scratch), "Protocol-Version: 1\r\nDevice-Id: ");
  append(scratch, sizeof(scratch), device);
  append(scratch, sizeof(scratch), "\r\nClient-Id: ");
  append(scratch, sizeof(scratch), client);
  append(scratch, sizeof(scratch), "\r\n");
  if (!H->ws_open(url, scratch)) {
    fail("WSS request rejected; OK retry", "连接失败，按 OK 重试");
    return;
  }
  phase = CONNECT;
  deadline = elapsed + 15000;
  draw();
}
static void receive(uint32_t kind, int32_t status, const void *data,
                    uint32_t bytes) {
  if (kind == PASSPORT_NATIVE_AUDIO_OPUS) {
    if (listening && held && !H->ws_send(data, bytes, 1))
      fail("Audio send failed", "语音发送失败");
    return;
  }
  if (kind == PASSPORT_NATIVE_WS_BINARY) {
    if (speaking && consent)
      H->audio_play_opus(data, bytes);
    return;
  }
  if (kind == PASSPORT_NATIVE_HTTP) {
    if (phase == CONNECT) {
      if (status != 101)
        fail_code("WSS handshake failed; OK retry", "连接失败，按 OK 重试",
                  status);
      return;
    }
    if (phase != CONFIG)
      return;
    if (status != 200) {
      if (attempts < 3 &&
          (status < 0 || status == 408 || status == 429 || status >= 500)) {
        phase = RETRY;
        deadline = elapsed + (uint32_t)attempts * 1000;
        copy(detail, sizeof(detail),
             tr("Temporary service error", "服务暂不可用"));
        draw();
      } else
        fail_code("Service HTTP error; OK retry", "服务连接失败，按 OK 重试",
                  status);
      return;
    }
    H->json_get(data, bytes, "activation.code", activation, sizeof(activation));
    if (H->json_get(data, bytes, "websocket.url", url, sizeof(url)) <= 0) {
      fail(activation[0] ? "Activate at xiaozhi.me; OK retry"
                         : "Missing websocket URL",
           activation[0] ? "请在 xiaozhi.me 激活后重试" : "服务配置不可用");
      return;
    }
    token[0] = 0;
    int result =
        H->json_get(data, bytes, "websocket.token", token, sizeof(token));
    if (result == -2) {
      fail("Authentication token too long", "服务配置不可用");
      return;
    }
    open_socket();
    return;
  }
  if (kind != PASSPORT_NATIVE_WS_TEXT)
    return;
  char type[24], state[24];
  if (H->json_get(data, bytes, "type", type, sizeof(type)) <= 0)
    return;
  if (equal(type, "hello")) {
    if (phase != HELLO)
      return;
    char format[12], channels[8], duration[8], rate[12], transport[16];
    if (H->json_get(data, bytes, "transport", transport, sizeof(transport)) <=
            0 ||
        !equal(transport, "websocket") ||
        H->json_get(data, bytes, "audio_params.format", format,
                    sizeof(format)) <= 0 ||
        !equal(format, "opus") ||
        H->json_get(data, bytes, "audio_params.channels", channels,
                    sizeof(channels)) <= 0 ||
        !equal(channels, "1")) {
      fail("Unsupported server audio", "服务音频格式不支持");
      return;
    }
    int has_rate = H->json_get(data, bytes, "audio_params.sample_rate", rate,
                               sizeof(rate));
    if (has_rate <= 0 ||
        !(equal(rate, "8000") || equal(rate, "12000") || equal(rate, "16000") ||
          equal(rate, "24000") || equal(rate, "48000"))) {
      fail("Unsupported audio rate", "服务音频格式不支持");
      return;
    }
    int has_duration = H->json_get(data, bytes, "audio_params.frame_duration",
                                   duration, sizeof(duration));
    if (has_duration != -1 &&
        (has_duration < 0 || !(equal(duration, "20") || equal(duration, "40") ||
                               equal(duration, "60")))) {
      fail("Unsupported audio frame", "服务音频格式不支持");
      return;
    }
    if (H->json_get(data, bytes, "session_id", session, sizeof(session)) <= 0) {
      fail("Missing session ID", "服务配置不可用");
      return;
    }
    phase = READY;
    copy(detail, sizeof(detail), "Opus / Native ELF");
    if (consent)
      H->audio_mode(0);
    begin_listen();
    draw();
  } else if (equal(type, "stt")) {
    char text[512];
    if (H->json_get(data, bytes, "text", text, sizeof(text)) >= 0)
      copy(heard, sizeof(heard), text);
    draw();
  } else if (equal(type, "tts")) {
    if (H->json_get(data, bytes, "state", state, sizeof(state)) < 0)
      return;
    if (equal(state, "start")) {
      speaking = 1;
      if (consent)
        H->audio_mode(0);
      listening = 0;
    } else if (equal(state, "stop")) {
      speaking = 0;
    } else if (equal(state, "sentence_start")) {
      char text[512];
      if (H->json_get(data, bytes, "text", text, sizeof(text)) >= 0)
        copy(answer, sizeof(answer), text);
    }
    draw();
  } else if (equal(type, "goodbye")) {
    H->ws_close();
    fail("Session ended; OK retry", "连接已断开，按 OK 重试");
  } else if (equal(type, "error")) {
    fail("Server error; OK retry", "服务连接失败，按 OK 重试");
  }
}
static void stop(void) {
  held = 0;
  listening = 0;
  speaking = 0;
  H->audio_mode(-1);
  H->ws_close();
}
static void event(uint32_t event, int32_t value) {
  if (event == PASSPORT_NATIVE_PERMISSION) {
    consent = value == 1;
    if (consent)
      configure();
    else
      fail("Microphone permission denied", "麦克风权限被拒绝");
  } else if (event == PASSPORT_NATIVE_AUDIO_ERROR) {
    listening = 0;
    held = 0;
    H->audio_mode(-1);
    fail("Audio unavailable; release OK and retry", "麦克风暂不可用，请重试");
  } else if (event == PASSPORT_NATIVE_PRESS && value == 2 && page == 0) {
    held = 1;
    begin_listen();
  } else if (event == PASSPORT_NATIVE_RELEASE && value == 2) {
    held = 0;
    if (listening) {
      message("listen", "stop");
      H->audio_mode(0);
      listening = 0;
      draw();
    }
  } else if (event == PASSPORT_NATIVE_SHORT) {
    if (value < 2) {
      if (listening) {
        held = 0;
        message("listen", "stop");
        H->audio_mode(0);
        listening = 0;
      }
      page = (page + (value == 0 ? 2 : 1)) % 3;
      draw();
    } else if (page == 1 && phase == READY && H->ws_connected()) {
      H->audio_mode(0);
      if (!message("listen", "detect"))
        fail("Greeting send failed", "连接失败，请重试");
    } else if (page == 2 || phase == FAILED) {
      stop();
      attempts = 0;
      configure();
    }
  } else if (event == PASSPORT_NATIVE_TICK) {
    elapsed += (uint32_t)value;
    if ((phase == WIFI || phase == RETRY) && (int32_t)(elapsed - deadline) >= 0)
      configure();
    if (phase == CONNECT && H->ws_connected() && !hello_sent) {
      static const char hello[] =
          "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
          "\"features\":{\"mcp\":false},\"audio_params\":{\"format\":\"opus\","
          "\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
      if (!H->ws_send(hello, sizeof(hello) - 1, 0)) {
        fail("Hello send failed", "连接失败，按 OK 重试");
        return;
      }
      hello_sent = 1;
      phase = HELLO;
      deadline = elapsed + 10000;
      draw();
    }
    if ((phase == CONFIG || phase == CONNECT || phase == HELLO) &&
        (int32_t)(elapsed - deadline) >= 0)
      fail("Service timeout; OK retry", "服务超时，按 OK 重试");
    if (phase == READY && !H->ws_connected())
      fail("WSS disconnected; OK retry", "连接已断开，按 OK 重试");
  }
}
int passport_app_entry(const passport_native_host_v1 *host,
                       passport_native_app_v1 *app) {
  if (!host || host->abi != 2 || host->size < sizeof(passport_native_host_v2))
    return -1;
  H = (const passport_native_host_v2 *)host;
  zh = equal(H->base.language(), "zh-CN");
  light = equal(H->base.display_mode(), "light");
  phase = CONSENT;
  page = 0;
  consent = 0;
  elapsed = 0;
  attempts = 0;
  held = listening = speaking = 0;
  *app =
      (passport_native_app_v1){2, sizeof(passport_native_app_v2), event, stop};
  ((passport_native_app_v2 *)app)->data = receive;
  draw();
  return 0;
}
