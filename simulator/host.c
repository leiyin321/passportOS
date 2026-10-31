#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Native simulator: compiles the same runtime and Lua configuration as
// firmware.
#include "asset_loader.h"
#include "icon_registry.h"
#include "language.h"
#include "network.h"
#include "runtime.h"
#include "store.h"
static bool mock_offline, mock_http, mock_ws, mock_http_custom, mock_ws_pending;
static bool mock_http_reject_once;
static unsigned mock_ws_open_count;
static bool mock_auth_headers;
static uint32_t mock_generation;
static char mock_message[2049], mock_last_text[2049];
static size_t mock_message_size;
static int mock_opcode;
static unsigned mock_binary_count;
static bool mock_hello_sent, mock_listen_start_sent;
bool ps_net_ws_open(const ps_package *p, const char *u, const char *headers,
                    size_t headers_size, uint32_t g) {
  (void)g;
  mock_ws_open_count++;
  mock_auth_headers = headers && headers_size &&
      strstr(headers, "Authorization: Bearer sim-token\r\n") &&
      strstr(headers, "Protocol-Version: 1\r\n") &&
      strstr(headers, "Device-Id: 02:00:00:00:00:01\r\n") &&
      strstr(headers, "Client-Id: 3d2d623e-ea11-5ba2-a941-7f72c55f8122\r\n");
  mock_ws = !mock_offline && (p->permissions & PS_WS) &&
            !strncmp(u, "wss://api.tenclass.net/", 23);
  mock_generation = g;
  mock_ws_pending = mock_ws;
  return mock_ws;
}
bool ps_net_ws_send(const char *d, size_t n, bool binary) {
  if (!mock_ws || n > 2048)
    return false;
  if (binary) mock_binary_count++;
  else {
    memcpy(mock_last_text, d, n); mock_last_text[n] = 0;
    if (strstr(mock_last_text, "\"type\":\"hello\"")) mock_hello_sent = true;
    if (strstr(mock_last_text, "\"type\":\"listen\"") &&
        strstr(mock_last_text, "\"state\":\"start\"")) mock_listen_start_sent = true;
    memcpy(mock_message, d, n); mock_message[n] = 0;
    mock_message_size = n; mock_opcode = 1;
  }
  return true;
}
bool ps_net_ws_connected(void) { return mock_ws && !getenv("PASSPORT_SIM_WS_CONNECTING"); }
bool ps_net_request(const ps_package *p, const char *method, const char *url,
                    const char *headers, size_t headers_size, const char *body,
                    size_t body_size, uint32_t g) {
  (void)method; (void)headers; (void)headers_size; (void)body; (void)body_size;
  if (mock_http_reject_once) {
    mock_http_reject_once = false;
    return false;
  }
  if (mock_offline || !(p->permissions & PS_FETCH) ||
      strcmp(url, "https://api.tenclass.net/xiaozhi/ota/")) return false;
  mock_http = true; mock_http_custom = true; mock_generation = g; return true;
}
void ps_net_ws_close(void) { mock_ws = false; mock_ws_pending = false; mock_message_size = 0; }
void ps_net_cancel(void) { mock_http = false; mock_http_custom = false; }
bool ps_audio_start(void) { return true; }
static bool mock_audio_prepared, mock_audio_active;
static unsigned mock_audio_start_count;
bool ps_audio_stop(void) { mock_audio_prepared = false; mock_audio_active = false; return true; }
bool ps_audio_stream_prepare(void) { mock_audio_prepared = true; return true; }
bool ps_audio_stream_prepared(void) { return mock_audio_prepared; }
bool ps_audio_stream_active(void) { return mock_audio_active; }
bool ps_audio_stream_start(void) {
  mock_audio_start_count++;
  if (getenv("PASSPORT_SIM_AUDIO_START_FAIL")) return false;
  mock_audio_prepared = false; mock_audio_active = true; return true;
}
bool ps_audio_stream_stop(void) { return true; }
bool ps_audio_stream_poll(uint8_t *data, size_t capacity, size_t *size) {
  (void)data; (void)capacity; (void)size; return false;
}
bool ps_audio_stream_play_opus(const uint8_t *data, size_t size) {
  (void)data; return size <= 1024;
}
bool ps_net_fetch(const ps_package *p, const char *u, uint32_t g, bool system) {
  (void)system;
  if (mock_offline || !(p->permissions & PS_FETCH) ||
      strncmp(u, "https://api.open-meteo.com/", 27))
    return false;
  mock_http = true;
  mock_generation = g;
  return true;
}
static void mock_poll(void) {
  if (mock_ws_pending) {
    mock_ws_pending = false;
    ps_runtime_result(mock_generation, 101, NULL, 0);
  }
  if (mock_http) {
    mock_http = false;
    const char *data = mock_http_custom
        ? "{\"websocket\":{\"url\":\"wss://api.tenclass.net/xiaozhi/v1/\",\"token\":\"sim-token\"},\"firmware\":{\"url\":\"https://example.invalid/firmware.bin\"}}"
        : "{\"current\":{\"temperature_2m\":22.5,\"weather_code\":3,\"time\":\"SIMULATED\"}}";
    if (mock_http_custom && getenv("PASSPORT_SIM_HTTP_BODY"))
      data = getenv("PASSPORT_SIM_HTTP_BODY");
    int status = mock_http_custom && getenv("PASSPORT_SIM_HTTP_STATUS")
        ? atoi(getenv("PASSPORT_SIM_HTTP_STATUS")) : 200;
    mock_http_custom = false;
    ps_runtime_result(mock_generation, status, data, strlen(data));
  }
  if (mock_message_size) {
    static const char hello_ack[] = "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"sim-session\",\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1}}";
    if (mock_opcode == 1 && strstr(mock_message, "\"type\":\"hello\"")) {
      const char *reply = getenv("PASSPORT_SIM_HELLO");
      if (!reply) reply = hello_ack;
      mock_message_size = strlen(reply);
      assert(mock_message_size < sizeof(mock_message));
      memcpy(mock_message, reply, mock_message_size);
      mock_message[mock_message_size] = 0;
    }
    ps_runtime_message(mock_message, mock_message_size, mock_opcode);
    mock_message_size = 0;
  }
}
static const char *data_dir;
static bool keyok(const char *k) {
  if (!*k || strlen(k) > 15)
    return false;
  for (; *k; k++)
    if (!((*k >= 'a' && *k <= 'z') || (*k >= '0' && *k <= '9') || *k == '_'))
      return false;
  return true;
}
bool ps_data_get(const ps_package *p, const char *k, char *v, size_t n) {
  if (!(p->permissions & PS_STORAGE) || !keyok(k))
    return false;
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", data_dir, k);
  FILE *f = fopen(path, "r");
  if (!f)
    return false;
  size_t r = fread(v, 1, n - 1, f);
  v[r] = 0;
  fclose(f);
  return true;
}
bool ps_data_set(const ps_package *p, const char *k, const char *v) {
  if (!(p->permissions & PS_STORAGE) || !keyok(k) || strlen(v) > 1024)
    return false;
  char path[512], tmp[520];
  snprintf(path, sizeof(path), "%s/%s", data_dir, k);
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f)
    return false;
  bool ok = fwrite(v, 1, strlen(v), f) == strlen(v);
  fclose(f);
  return ok && rename(tmp, path) == 0;
}
static char *readfile(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  char *b = calloc(n + 1, 1);
  if (b)
    fread(b, 1, n, f);
  fclose(f);
  return b;
}
static void snapshot(void) {
  const ps_view *v = ps_runtime_view();
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "text", v->text);
  cJSON_AddNumberToObject(j, "text_color", v->text_color);
  cJSON_AddStringToObject(j, "error", ps_runtime_error());
  cJSON_AddNumberToObject(j, "heap", v->lua_used);
  cJSON_AddNumberToObject(j, "peak", v->lua_peak);
  cJSON_AddBoolToObject(j, "microphone", v->microphone);
  cJSON_AddBoolToObject(j, "audio_stream", v->audio_stream);
  cJSON_AddBoolToObject(j, "audio_prepared", mock_audio_prepared);
  cJSON_AddNumberToObject(j, "audio_start_count", mock_audio_start_count);
  cJSON_AddBoolToObject(j, "ws_connected", mock_ws);
  cJSON_AddStringToObject(j, "last_ws_text", mock_last_text);
  cJSON_AddNumberToObject(j, "ws_binary_count", mock_binary_count);
  cJSON_AddBoolToObject(j, "hello_sent", mock_hello_sent);
  cJSON_AddBoolToObject(j, "listen_start_sent", mock_listen_start_sent);
  cJSON_AddBoolToObject(j, "ws_auth_headers", mock_auth_headers);
  cJSON_AddNumberToObject(j, "ws_open_count", mock_ws_open_count);
  const ps_display_layout *layout = ps_runtime_display();
  cJSON *vp = cJSON_AddObjectToObject(j, "viewport");
  cJSON_AddNumberToObject(vp, "width", layout->viewport.width);
  cJSON_AddNumberToObject(vp, "height", layout->viewport.height);
  cJSON_AddStringToObject(vp, "mode", layout->mode == PS_DISPLAY_FULLSCREEN ? "fullscreen" : "standard");
  cJSON_AddNumberToObject(j, "asset_bytes", ps_assets_metrics()->current);
  cJSON_AddNumberToObject(j, "asset_peak", ps_assets_metrics()->peak);
  cJSON *images = cJSON_AddArrayToObject(j, "images");
  ps_assets_unpin();
  for (unsigned i = 0; i < v->image_count; i++) {
    const ps_image_command *image = &v->images[i];
    cJSON *item = cJSON_CreateObject();
    cJSON_AddItemToArray(images, item);
    cJSON_AddNumberToObject(item, "x", image->x);
    cJSON_AddNumberToObject(item, "y", image->y);
    cJSON_AddNumberToObject(item, "size", image->size);
    cJSON_AddNumberToObject(item, "color", image->color);
    cJSON_AddBoolToObject(item, "system", image->system);
    cJSON_AddStringToObject(item, "id",
                            image->system ? ps_icon_get(image->handle)->id
                                          : ps_asset_name(image->handle));
    if (!image->system)
      ps_asset_pin(image->handle);
  }

  cJSON *draw = cJSON_AddArrayToObject(j, "draw");
  for (unsigned i = 0; i < v->count; i++) {
    const ps_draw *d = &v->commands[i];
    int vals[] = {d->x, d->y, d->w, d->h, d->color};
    cJSON_AddItemToArray(draw, cJSON_CreateIntArray(vals, 5));
  }
  char *s = cJSON_PrintUnformatted(j);
  puts(s);
  fflush(stdout);
  free(s);
  cJSON_Delete(j);
}
int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  data_dir = argv[3];
  mock_http_reject_once = getenv("PASSPORT_SIM_HTTP_REJECT_ONCE") != NULL;
  const char *system_language = getenv("PASSPORT_SYSTEM_LANGUAGE");
  if (system_language)
    ps_runtime_set_system_language(system_language);
  const char *system_display_mode = getenv("PASSPORT_SYSTEM_DISPLAY_MODE");
  if (system_display_mode)
    ps_runtime_set_system_display_mode(system_display_mode);
  char *m = readfile(argv[1]);
  ps_package p = {0};
  p.manifest = cJSON_Parse(m);
  free(m);
  if (!p.manifest)
    return 2;
  p.code = readfile(argv[2]);
  if (!p.code)
    return 2;
  snprintf(p.id, sizeof(p.id), "%s",
           cJSON_GetObjectItemCaseSensitive(p.manifest, "id")->valuestring);
  snprintf(
      p.runtime, sizeof(p.runtime), "%s",
      cJSON_GetObjectItemCaseSensitive(p.manifest, "runtime")->valuestring);
  cJSON *perms = cJSON_GetObjectItemCaseSensitive(p.manifest, "permissions");
  const char *names[] = {"ui",
                         "graphics",
                         "input",
                         "timer",
                         "storage.private",
                         "network.fetch",
                         "network.websocket",
                         "audio.features",
                         "audio.stream",
                         "device.identity"};
  for (cJSON *c = perms->child; c; c = c->next)
    for (int i = 0; i < 10; i++)
      if (!strcmp(c->valuestring, names[i]))
        p.permissions |= 1u << i;
  snprintf(p.identity, sizeof(p.identity), "simulator");
  if (cJSON_GetObjectItemCaseSensitive(p.manifest, "assets")) {
    const char *slash = strrchr(argv[1], '/');
    if (slash) {
      snprintf(p.package_path, sizeof(p.package_path), "%.*s/package.pap",
               (int)(slash - argv[1]), argv[1]);
      FILE *f = fopen(p.package_path, "rb");
      if (f) {
        unsigned char header[16];
        if (fread(header, 1, 16, f) == 16) {
          unsigned ml =
              header[4] | header[5] << 8 | header[6] << 16 | header[7] << 24;
          unsigned pl =
              header[8] | header[9] << 8 | header[10] << 16 | header[11] << 24;
          p.asset_base = 16 + ml + 4 + strlen(p.code);
          p.asset_bytes = pl - 4 - strlen(p.code);
        }
        fclose(f);
      }
    }
  }
  bool started = ps_runtime_start(&p);
  if (started) assert(p.code == NULL); // source ownership/reclamation contract
  mock_poll();
  snapshot();
  char line[2048];
  while (fgets(line, sizeof(line), stdin)) {
    char name[32];
    int value;
    if (sscanf(line, "%31s %d", name, &value) == 2) {
      if (!strcmp(name, "permission")) {
        ps_runtime_audio_allowed(value != 0);
        ps_runtime_event("on_permission", value);
      } else if (!strcmp(name, "audio_failed"))
        ps_runtime_audio_error(value);
      else if (!strcmp(name, "offline"))
        mock_offline = value != 0;
      else
        ps_runtime_event(name, value);
    } else if (!strncmp(line, "stop", 4))
      break;
    mock_poll();
    snapshot();
  }
  ps_runtime_stop();
  ps_assets_unpin();
  ps_assets_close();
  free(p.code);
  cJSON_Delete(p.manifest);
  return 0;
}
