#include "runtime.h"
#include "asset_loader.h"
#include "declarative.h"
#include "icon_registry.h"
#include "language.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "network.h"
#include "audio.h"
#include "store.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#ifdef ESP_PLATFORM
#include "esp_mac.h"
#include "mbedtls/sha256.h"
#endif
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "native.h"
static uint64_t now_us(void) { return esp_timer_get_time(); }
#else
#include <time.h>
static uint64_t now_us(void) {
  return (uint64_t)clock() * 1000000 / CLOCKS_PER_SEC;
}
#endif
static lua_State *L;
static ps_package *app;
static ps_view view;
static char error[128];
static size_t used, peak, budget;
static uint64_t deadline;
static int instructions;
static unsigned event_writes;
static bool audio_allowed;
static uint32_t generation;
static bool declarative;
static bool native;
static ps_language_t system_language = PS_LANGUAGE_EN;
static bool system_light_mode;
static ps_display_layout display_layout;
static void *allocate(void *ud, void *ptr, size_t old, size_t n) {
  (void)ud;
  if (!ptr)
    old = 0;
  if (!n) {
    free(ptr);
    used -= old;
    return NULL;
  }
  if (n > budget || used - old > budget - n)
    return NULL;
  void *p = realloc(ptr, n);
  if (p) {
    used = used - old + n;
    if (used > peak)
      peak = used;
  }
  return p;
}
static void hook(lua_State *s, lua_Debug *ar) {
  (void)ar;
  instructions += 1000;
  if (instructions >= 40000 || now_us() > deadline)
    luaL_error(s, "execution budget exceeded");
}
static void permission(lua_State *s, unsigned p) {
  if (!app || (app->permissions & p) != p)
    luaL_error(s, "permission denied");
}
static int text(lua_State *s) {
  permission(s, PS_UI);
  const char *v = luaL_checkstring(s, 1);
  lua_Integer color = luaL_optinteger(s, 2, view.text_color);
  if (strlen(v) > 500)
    return luaL_error(s, "text too long");
  if (color < 0 || color > 0xffffff)
    return luaL_error(s, "text color bounds");
  snprintf(view.text, sizeof(view.text), "%s", v);
  view.text_color = (uint32_t)color;
  return 0;
}
static int clear(lua_State *s) {
  permission(s, PS_GRAPHICS);
  view.count = 0;
  view.image_count = 0;
  return 0;
}
static int rect(lua_State *s) {
  permission(s, PS_GRAPHICS);
  int x = luaL_checkinteger(s, 1), y = luaL_checkinteger(s, 2),
      w = luaL_checkinteger(s, 3), h = luaL_checkinteger(s, 4);
  lua_Integer color = luaL_checkinteger(s, 5);
  ps_display_rect clipped;
  if (x < 0 || y < 0 || color < 0 || color > 0xffffff ||
      !ps_display_clip(&display_layout, x, y, w, h, &clipped))
    return luaL_error(s, "draw bounds");
  if (view.count >= PS_DRAW_MAX)
    return luaL_error(s, "draw quota");
  view.commands[view.count++] = (ps_draw){.x = clipped.x, .y = clipped.y,
      .w = clipped.width, .h = clipped.height, .color = (uint32_t)color};
  return 0;
}
static int get(lua_State *s) {
  permission(s, PS_STORAGE);
  char v[1025];
  if (ps_data_get(app, luaL_checkstring(s, 1), v, sizeof(v)))
    lua_pushstring(s, v);
  else
    lua_pushnil(s);
  return 1;
}
static int set(lua_State *s) {
  permission(s, PS_STORAGE);
  if (++event_writes > 4)
    return luaL_error(s, "storage write event budget");
  const char *k = luaL_checkstring(s, 1), *v = luaL_checkstring(s, 2);
  lua_pushboolean(s, ps_data_set(app, k, v));
  return 1;
}
static int audio(lua_State *s) {
  permission(s, PS_AUDIO);
  view.microphone = audio_allowed && lua_toboolean(s, 1);
  lua_pushboolean(s, view.microphone);
  return 1;
}
static int audio_stream_start(lua_State *s) {
  permission(s, PS_AUDIO_STREAM);
  bool ok = audio_allowed && ps_audio_stream_start();
  if (ok) { view.microphone = true; view.audio_stream = true; }
  lua_pushboolean(s, ok);
  return 1;
}
static int audio_stream_stop(lua_State *s) {
  permission(s, PS_AUDIO_STREAM);
  (void)ps_audio_stream_stop();
  view.microphone = false;
  lua_pushboolean(s, true);
  return 1;
}
static int audio_read_opus(lua_State *s) {
  permission(s, PS_AUDIO_STREAM);
  uint8_t frame[512]; size_t size = 0;
  if (ps_audio_stream_poll(frame, sizeof(frame), &size))
    lua_pushlstring(s, (const char *)frame, size);
  else lua_pushnil(s);
  return 1;
}
static int audio_play_opus(lua_State *s) {
  permission(s, PS_AUDIO_STREAM);
  size_t size;
  const char *data = luaL_checklstring(s, 1, &size);
  lua_pushboolean(s, audio_allowed &&
      ps_audio_stream_play_opus((const uint8_t *)data, size));
  return 1;
}
static int net_fetch(lua_State *s) {
  permission(s, PS_FETCH);
  lua_pushboolean(s,
                  ps_net_fetch(app, luaL_checkstring(s, 1), generation, false));
  return 1;
}
static bool append_headers(lua_State *s, int index, char *out, size_t cap,
                           size_t *written, unsigned max_count) {
  *written = 0;
  if (lua_isnoneornil(s, index)) return true;
  if (!lua_istable(s, index)) return false;
  int absolute = index > 0 ? index : lua_gettop(s) + index + 1;
  unsigned count = 0;
  lua_pushnil(s);
  while (lua_next(s, absolute)) {
    size_t kn, vn;
    const char *key = lua_type(s, -2) == LUA_TSTRING ? lua_tolstring(s, -2, &kn) : NULL;
    const char *value = lua_type(s, -1) == LUA_TSTRING ? lua_tolstring(s, -1, &vn) : NULL;
    if (!key || !value || ++count > max_count || !kn || kn > 64 || vn > 256 ||
        memchr(key, 0, kn) || memchr(value, 0, vn) ||
        memchr(key, '\r', kn) || memchr(key, '\n', kn) ||
        memchr(key, ':', kn) || memchr(value, '\r', vn) ||
        memchr(value, '\n', vn) ||
        *written + kn + vn + 4 >= cap) {
      lua_pop(s, 2); return false;
    }
    memcpy(out + *written, key, kn); *written += kn;
    memcpy(out + *written, ": ", 2); *written += 2;
    memcpy(out + *written, value, vn); *written += vn;
    memcpy(out + *written, "\r\n", 2); *written += 2;
    lua_pop(s, 1);
  }
  out[*written] = 0;
  return true;
}
static int http_request(lua_State *s) {
  permission(s, PS_FETCH);
  const char *method = luaL_checkstring(s, 1);
  const char *url = luaL_checkstring(s, 2);
  char headers[1025]; size_t headers_size;
  if (!append_headers(s, 3, headers, sizeof(headers), &headers_size, 8)) {
    lua_pushnil(s); lua_pushliteral(s, "invalid/bounded HTTP headers"); return 2;
  }
  size_t body_size = 0;
  const char *body = lua_isnoneornil(s, 4) ? NULL : luaL_checklstring(s, 4, &body_size);
  // Reclaim temporary UI/JSON objects before the worker starts TLS.
  lua_gc(s, LUA_GCCOLLECT);
  lua_pushboolean(s, ps_net_request(app, method, url, headers, headers_size,
                                    body, body_size, generation));
  return 1;
}
static int ws_open(lua_State *s) {
  permission(s, PS_WS);
  char headers[1025]; size_t headers_size;
  if (!append_headers(s, 2, headers, sizeof(headers), &headers_size, 4)) {
    lua_pushnil(s); lua_pushliteral(s, "invalid/bounded WebSocket headers"); return 2;
  }
  lua_gc(s, LUA_GCCOLLECT);
  lua_pushboolean(s, ps_net_ws_open(app, luaL_checkstring(s, 1), headers,
                                    headers_size, generation));
  return 1;
}
static int ws_send(lua_State *s) {
  permission(s, PS_WS);
  size_t n;
  const char *data = luaL_checklstring(s, 1, &n);
  lua_pushboolean(s, ps_net_ws_send(data, n, lua_toboolean(s, 2)));
  return 1;
}
static int ws_connected(lua_State *s) {
  permission(s, PS_WS);
  lua_pushboolean(s, ps_net_ws_connected());
  return 1;
}
static int ws_close(lua_State *s) {
  permission(s, PS_WS);
  ps_net_ws_close();
  return 0;
}
static int icon(lua_State *s) {
  permission(s, PS_UI);
  const char *name = luaL_checkstring(s, 1);
  int size = luaL_optinteger(s, 2, 24);
  int x = luaL_optinteger(s, 3, (view.image_count % 4) * 50),
      y = luaL_optinteger(s, 4, (view.image_count / 4) * 40);
  lua_Integer color = luaL_optinteger(s, 5, 0xe9f0fa);
  if (size < 12 || size > 48 || x < 0 || y < 0 ||
      x > display_layout.viewport.width - size ||
      y > display_layout.viewport.height - size || color < 0 || color > 0xffffff ||
      view.image_count >= PS_IMAGE_MAX)
    return luaL_error(s, "icon bounds/quota");
  view.images[view.image_count++] =
      (ps_image_command){.system = 1,
                         .handle = ps_icon_index(name),
                         .x = x,
                         .y = y,
                         .size = size,
                         .color = color};
  return 0;
}
static int sprite(lua_State *s) {
  permission(s, PS_GRAPHICS);
  const char *uri = luaL_checkstring(s, 1);
  int x = luaL_checkinteger(s, 2), y = luaL_checkinteger(s, 3);
  if (view.image_count >= PS_IMAGE_MAX || x < 0 || y < 0 ||
      x >= display_layout.viewport.width || y >= display_layout.viewport.height)
    return luaL_error(s, "sprite bounds/quota");
  uint32_t token = ps_asset_open(app, uri);
  ps_asset_info info;
  if (token && ps_asset_get(token, &info) &&
      (x > display_layout.viewport.width - info.width || y > display_layout.viewport.height - info.height))
    return luaL_error(s, "sprite bounds");
  view.images[view.image_count++] =
      (ps_image_command){.system = token ? 0 : 1,
                         .handle = token ? token : 0,
                         .x = x,
                         .y = y,
                         .size = 24,
                         .color = 0xe9f0fa};
  lua_pushboolean(s, token != 0);
  return 1;
}
static int get_viewport(lua_State *s) {
  lua_newtable(s);
  lua_pushinteger(s, display_layout.viewport.width); lua_setfield(s, -2, "width");
  lua_pushinteger(s, display_layout.viewport.height); lua_setfield(s, -2, "height");
  lua_pushstring(s, display_layout.mode == PS_DISPLAY_FULLSCREEN ? "fullscreen" : "standard"); lua_setfield(s, -2, "mode");
  lua_newtable(s);
  lua_pushinteger(s, display_layout.safe_left); lua_setfield(s, -2, "left");
  lua_pushinteger(s, display_layout.safe_top); lua_setfield(s, -2, "top");
  lua_pushinteger(s, display_layout.safe_right); lua_setfield(s, -2, "right");
  lua_pushinteger(s, display_layout.safe_bottom); lua_setfield(s, -2, "bottom");
  lua_setfield(s, -2, "safeInsets");
  return 1;
}
static int get_system_language(lua_State *s) {
  lua_pushstring(s, ps_language_code(system_language));
  return 1;
}
static int get_system_display_mode(lua_State *s) {
  lua_pushstring(s, system_light_mode ? "light" : "dark");
  return 1;
}
static char json_null_marker;
static bool json_to_lua(lua_State *s, const cJSON *value, unsigned depth,
                        unsigned *nodes) {
  int top = lua_gettop(s);
  if (!value || depth > 8 || ++*nodes > 256)
    return false;
  if (cJSON_IsNull(value)) {
    lua_pushlightuserdata(s, &json_null_marker);
    return true;
  }
  if (cJSON_IsBool(value)) {
    lua_pushboolean(s, cJSON_IsTrue(value));
    return true;
  }
  if (cJSON_IsNumber(value)) {
    if (!isfinite(value->valuedouble)) return false;
    lua_pushnumber(s, value->valuedouble);
    return true;
  }
  if (cJSON_IsString(value)) {
    size_t n = strlen(value->valuestring);
    if (n > 2048) return false;
    lua_pushlstring(s, value->valuestring, n);
    return true;
  }
  if (cJSON_IsArray(value)) {
    unsigned count = 0;
    for (const cJSON *v = value->child; v; v = v->next)
      if (++count > 256) return false;
    lua_createtable(s, count, 0);
    unsigned i = 1;
    for (const cJSON *v = value->child; v; v = v->next, i++) {
      if (!json_to_lua(s, v, depth + 1, nodes)) { lua_settop(s, top); return false; }
      lua_rawseti(s, -2, i);
    }
    return true;
  }
  if (cJSON_IsObject(value)) {
    unsigned count = 0;
    for (const cJSON *v = value->child; v; v = v->next) {
      if (++count > 256 || !v->string || strlen(v->string) > 2048) return false;
      for (const cJSON *other = value->child; other != v; other = other->next)
        if (other->string && !strcmp(other->string, v->string)) return false;
    }
    lua_createtable(s, 0, count);
    for (const cJSON *v = value->child; v; v = v->next) {
      if (!json_to_lua(s, v, depth + 1, nodes)) { lua_settop(s, top); return false; }
      lua_setfield(s, -2, v->string);
    }
    return true;
  }
  return false;
}
static int decode_json_value(lua_State *s) {
  const cJSON *root = lua_touserdata(s, 1);
  unsigned nodes = 0;
  if (!json_to_lua(s, root, 0, &nodes)) {
    lua_pushnil(s); lua_pushliteral(s, "JSON structure limit or duplicate key");
    return 2;
  }
  return 1;
}
static int json_decode(lua_State *s) {
  size_t n;
  const char *raw = luaL_checklstring(s, 1, &n);
  if (!n || n > 8192 || !ps_json_preflight(raw, n, 8, 256)) {
    lua_pushnil(s); lua_pushliteral(s, "JSON size/structure limit"); return 2;
  }
  // Reserve the C call's stack before owning memory outside the Lua allocator.
  if (!lua_checkstack(s, 4)) return luaL_error(s, "out of memory");
  const char *end = NULL;
  cJSON *root = cJSON_ParseWithLengthOpts(raw, n, &end, false);
  if (!root) {
    lua_pushnil(s); lua_pushliteral(s, "invalid JSON"); return 2;
  }
  while (end && end < raw + n && isspace((unsigned char)*end)) end++;
  if (end != raw + n) {
    cJSON_Delete(root);
    lua_pushnil(s); lua_pushliteral(s, "invalid JSON suffix"); return 2;
  }
  int top = lua_gettop(s);
  lua_pushcfunction(s, decode_json_value);
  lua_pushlightuserdata(s, root);
  int rc = lua_pcall(s, 1, LUA_MULTRET, 0);
  cJSON_Delete(root); // also runs when Lua table/string allocation raises OOM
  if (rc != LUA_OK) return lua_error(s);
  return lua_gettop(s) - top;
}
typedef struct { const void *tables[9]; unsigned depth, nodes; } json_encode_ctx;
static cJSON *lua_to_json(lua_State *s, int index, json_encode_ctx *ctx,
                          const char **why) {
  if (++ctx->nodes > 256) { *why = "JSON value limit"; return NULL; }
  int type = lua_type(s, index);
  if (type == LUA_TNIL || (type == LUA_TLIGHTUSERDATA &&
      lua_touserdata(s, index) == &json_null_marker))
    return cJSON_CreateNull();
  if (type == LUA_TBOOLEAN)
    return cJSON_CreateBool(lua_toboolean(s, index));
  if (type == LUA_TNUMBER) {
    lua_Number number = lua_tonumber(s, index);
    if (!isfinite((double)number)) { *why = "non-finite number"; return NULL; }
    return cJSON_CreateNumber((double)number);
  }
  if (type == LUA_TSTRING) {
    size_t n; const char *str = lua_tolstring(s, index, &n);
    if (n > 2048 || memchr(str, 0, n)) { *why = "JSON string limit/NUL"; return NULL; }
    return cJSON_CreateString(str);
  }
  if (type != LUA_TTABLE || ctx->depth >= 8) {
    *why = "unsupported JSON value/depth"; return NULL;
  }
  const void *identity = lua_topointer(s, index);
  for (unsigned i = 0; i < ctx->depth; i++)
    if (ctx->tables[i] == identity) { *why = "JSON cycle"; return NULL; }
  ctx->tables[ctx->depth++] = identity;
  int absolute = index > 0 ? index : lua_gettop(s) + index + 1;
  unsigned count = 0, max_index = 0;
  bool numeric = false, string_keys = false;
  lua_pushnil(s);
  while (lua_next(s, absolute)) {
    count++;
    if (count > 256) { *why = "JSON value limit"; lua_pop(s, 2); goto fail; }
    int key_type = lua_type(s, -2);
    if (key_type == LUA_TNUMBER && lua_isinteger(s, -2)) {
      lua_Integer k = lua_tointeger(s, -2);
      if (k < 1 || k > 256 || string_keys) { *why = "mixed/invalid JSON table keys"; lua_pop(s, 2); goto fail; }
      numeric = true; if ((unsigned)k > max_index) max_index = (unsigned)k;
    } else if (key_type == LUA_TSTRING) {
      size_t key_size;
      const char *key = lua_tolstring(s, -2, &key_size);
      if (key_size > 2048 || memchr(key, 0, key_size)) {
        *why = "JSON key limit/NUL"; lua_pop(s, 2); goto fail;
      }
      if (numeric) { *why = "mixed JSON table keys"; lua_pop(s, 2); goto fail; }
      string_keys = true;
    } else { *why = "JSON keys must be strings or array indexes"; lua_pop(s, 2); goto fail; }
    lua_pop(s, 1);
  }
  if (numeric && max_index != count) { *why = "sparse JSON array"; goto fail; }
  cJSON *out = numeric ? cJSON_CreateArray() : cJSON_CreateObject();
  if (!out) { *why = "out of memory"; goto fail; }
  lua_pushnil(s);
  while (lua_next(s, absolute)) {
    const char *key = NULL;
    int key_type = lua_type(s, -2);
    if (key_type == LUA_TSTRING) key = lua_tostring(s, -2);
    cJSON *child = lua_to_json(s, -1, ctx, why);
    if (!child || (numeric ? !cJSON_AddItemToArray(out, child)
                          : !cJSON_AddItemToObject(out, key, child))) {
      cJSON_Delete(child);
      lua_pop(s, 2); cJSON_Delete(out); goto fail;
    }
    lua_pop(s, 1);
  }
  ctx->depth--;
  return out;
fail:
  ctx->depth--;
  return NULL;
}
static int push_json_text(lua_State *s) {
  const char *raw = lua_touserdata(s, 1);
  lua_pushlstring(s, raw, (size_t)lua_tointeger(s, 2));
  return 1;
}
static int json_encode(lua_State *s) {
  if (!lua_checkstack(s, 32)) return luaL_error(s, "out of memory");
  json_encode_ctx ctx = {0};
  const char *why = "invalid JSON value";
  cJSON *root = lua_to_json(s, 1, &ctx, &why);
  if (!root) { lua_pushnil(s); lua_pushstring(s, why); return 2; }
  char *raw = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!raw) { lua_pushnil(s); lua_pushliteral(s, "out of memory"); return 2; }
  size_t n = strlen(raw);
  if (n > 8192) {
    cJSON_free(raw); lua_pushnil(s); lua_pushliteral(s, "JSON output limit"); return 2;
  }
  lua_pushcfunction(s, push_json_text);
  lua_pushlightuserdata(s, raw);
  lua_pushinteger(s, (lua_Integer)n);
  int rc = lua_pcall(s, 2, 1, 0);
  cJSON_free(raw);
  if (rc != LUA_OK) return lua_error(s);
  return 1;
}
bool ps_runtime_identity(const ps_package *package, char *device, size_t dc,
                         char *client, size_t cc) {
  if (!package || !(package->permissions & PS_DEVICE_IDENTITY) || !device || !client || dc<18 || cc<37) return false;
#ifdef ESP_PLATFORM
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) return false;
  char seed[128];
  snprintf(device,dc,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  snprintf(seed,sizeof(seed),"passport-miniapp-client:%s:%s",package->id,device);
  unsigned char digest[32];
  mbedtls_sha256((const unsigned char *)seed,strlen(seed),digest,0);
  snprintf(client,cc,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",digest[0],digest[1],digest[2],digest[3],digest[4],digest[5],digest[6],digest[7],digest[8],digest[9],digest[10],digest[11],digest[12],digest[13],digest[14],digest[15]);
  client[14]='5'; client[19]="89ab"[digest[8]&3];
#else
  snprintf(device,dc,"02:00:00:00:00:01");
  snprintf(client,cc,"3d2d623e-ea11-5ba2-a941-7f72c55f8122");
#endif
  return true;
}
static int public_device_identity(lua_State *s) {
  permission(s,PS_DEVICE_IDENTITY);
  char device[18],client[37];
  if (!ps_runtime_identity(app,device,sizeof(device),client,sizeof(client))) { lua_pushnil(s); return 1; }
  lua_newtable(s);
  lua_pushstring(s,device); lua_setfield(s,-2,"device_id");
  lua_pushstring(s,client); lua_setfield(s,-2,"client_id");
  return 1;
}

static const luaL_Reg api[] = {{"icon", icon},        {"sprite", sprite},
                               {"fetch", net_fetch},  {"http_request", http_request},
                               {"ws_connect", ws_open},
                               {"ws_send", ws_send},  {"ws_close", ws_close},
                               {"ws_connected", ws_connected},
                               {"text", text},
                               {"clear", clear},      {"rect", rect},
                               {"get", get},          {"set", set},
                               {"microphone", audio}, {NULL, NULL}};

typedef struct {
  const char *name;
  int value;
  const char *data;
  size_t size;
  int args;
} invocation;
static invocation pending;
static int dispatch(lua_State *s) {
  lua_gc(s, LUA_GCCOLLECT);
  lua_getglobal(s, pending.name);
  if (!lua_isfunction(s, -1)) {
    lua_pop(s, 1);
    return 0;
  }
  if (pending.args == 2) {
    lua_pushinteger(s, pending.value);
    lua_pushlstring(s, pending.data ? pending.data : "", pending.size);
  } else if (pending.args == 3) {
    lua_pushlstring(s, pending.data ? pending.data : "", pending.size);
    lua_pushinteger(s, pending.value);
  } else if (pending.args == 4) {
    lua_pushlstring(s, pending.data ? pending.data : "", pending.size);
  } else
    lua_pushinteger(s, pending.value);
  lua_call(s, pending.args == 1 ? 1 : pending.args == 4 ? 1 : 2, 0);
  return 0;
}
static bool invoke(const char *name, int value, const char *data, size_t size,
                   int args) {
  int top = lua_gettop(L);
  pending = (invocation){name, value, data, size, args};
  deadline = now_us() + 12000;
  instructions = 0;
  event_writes = 0;
  lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
  lua_pushcfunction(L, dispatch);
  int rc = lua_pcall(L, 0, 0, 0);
  lua_sethook(L, NULL, 0, 0);
  if (rc) {
    const char *why = lua_tostring(L, -1);
    snprintf(error, sizeof(error), "%s: %.90s", name,
             why ? why : "non-string error");
    view.microphone = false;
  }
  lua_settop(L, top);
  view.lua_used = used;
  view.lua_peak = peak;
  return rc == LUA_OK;
}
static bool call(const char *name, int value) {
  return invoke(name, value, NULL, 0, 1);
}
static bool has_callback(const char *name) {
  int top = lua_gettop(L);
  lua_getglobal(L, name);
  bool found = lua_isfunction(L, -1);
  lua_settop(L, top);
  return found;
}
static int safe_find(lua_State *s) {
  size_t haystack, needle;
  luaL_checklstring(s, 1, &haystack);
  luaL_checklstring(s, 2, &needle);
  if (haystack > 4096 || needle > 64)
    return luaL_error(s, "find bounds");
  if (!lua_toboolean(s, 4))
    return luaL_error(s, "patterns disabled; use plain find");
  int n = lua_gettop(s);
  lua_pushvalue(s, lua_upvalueindex(1));
  lua_insert(s, 1);
  lua_call(s, n, LUA_MULTRET);
  return lua_gettop(s);
}
bool ps_runtime_start(ps_package *p) {
  ps_runtime_stop();
  if (!p || !p->manifest || !p->code) {
    snprintf(error, sizeof(error), "Package source unavailable");
    return false;
  }
  app = p;
  memset(&view, 0, sizeof(view));
  view.text_color = system_light_mode ? 0x172334 : 0xe9f0fa;
  error[0] = 0;
  audio_allowed = false;
  if (!ps_display_layout_read(p->manifest, &display_layout)) {
    snprintf(error, sizeof(error), "Invalid display mode/orientation");
    return false;
  }
  cJSON *limits_images =
            cJSON_GetObjectItemCaseSensitive(p->manifest, "limits"),
        *image_budget =
            cJSON_GetObjectItemCaseSensitive(limits_images, "image_cache_kib");
  ps_assets_budget(cJSON_IsNumber(image_budget) ? image_budget->valueint * 1024
                                                : 8192);
  generation++;
  native = !strcmp(p->runtime, "native-elf");
  if (native) {
#ifdef ESP_PLATFORM
    return ps_native_start(p, &view, &display_layout, ps_language_code(system_language),
                           system_light_mode ? "light" : "dark", generation, error, sizeof(error));
#else
    snprintf(error, sizeof(error), "Native ELF requires ESP32-C3 hardware");
    return false;
#endif
  }
  declarative = !strcmp(p->runtime, "declarative");
  if (declarative) {
    bool ok = ps_declarative_start(p, &view, generation, &display_layout);
    free(p->code);
    p->code = NULL;
    if (!ok)
      snprintf(error, sizeof(error), "Invalid declarative document");
    return ok;
  }
  const cJSON *limits = cJSON_GetObjectItemCaseSensitive(p->manifest, "limits");
  budget =
      cJSON_GetObjectItemCaseSensitive(limits, "script_heap_kib")->valueint *
      1024;
  used = peak = 0;
  L = lua_newstate(allocate, NULL);
  if (!L) {
    snprintf(error, sizeof(error), "VM allocation failed");
    return false;
  }
  // Load libraries inside a protected call: allocator exhaustion cannot panic
  // the OS.
  extern int ps_lua_initialize(lua_State * s);
  lua_pushcfunction(L, ps_lua_initialize);
  if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    goto fail;
  int load_result = luaL_loadbufferx(L, p->code, strlen(p->code), p->id, "t");
  // Lua now owns the compiled chunk; release its separately allocated source
  // before on_start can allocate network/TLS resources.
  free(p->code);
  p->code = NULL;
  if (load_result != LUA_OK)
    goto fail;
  deadline = now_us() + 12000;
  instructions = 0;
  event_writes = 0;
  lua_sethook(L, hook, LUA_MASKCOUNT, 1000);
  if (lua_pcall(L, 0, 0, 0) != LUA_OK)
    goto fail;
  lua_sethook(L, NULL, 0, 0);
  return call("on_start", 0);
fail:
  snprintf(error, sizeof(error), "Lua: %.110s", lua_tostring(L, -1));
  lua_close(L);
  L = NULL;
  view.microphone = false;
  view.audio_stream = false;
  return false;
}
int ps_lua_initialize(lua_State *s) {
  luaL_requiref(s, "_G", luaopen_base, 1);
  lua_pop(s, 1);
  luaL_requiref(s, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(s, 1);
  luaL_requiref(s, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(s, 1);
  luaL_requiref(s, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(s, 1);
  const char *unsafe[] = {
      "dofile", "loadfile", "load",         "collectgarbage", "print", "warn",
      "pcall",  "xpcall",   "setmetatable", "getmetatable",   NULL};
  for (int i = 0; unsafe[i]; i++) {
    lua_pushnil(s);
    lua_setglobal(s, unsafe[i]);
  }
  const char *restricted[] = {"pcall",        "xpcall", "setmetatable",
                              "getmetatable", "warn",   NULL};
  for (int i = 0; restricted[i]; i++) {
    lua_pushnil(s);
    lua_setglobal(s, restricted[i]);
  }
  lua_getglobal(s, "string");
  lua_getfield(s, -1, "find");
  lua_pushcclosure(s, safe_find, 1);
  lua_setfield(s, -2, "find");
  const char *patterns[] = {"match", "gmatch", "gsub",   "dump", "format",
                            "rep",   "pack",   "unpack", NULL};
  for (int i = 0; patterns[i]; i++) {
    lua_pushnil(s);
    lua_setfield(s, -2, patterns[i]);
  }
  lua_pop(s, 1);
  luaL_newlib(s, api);
  lua_newtable(s);
  lua_pushcfunction(s, get_viewport);
  lua_setfield(s, -2, "get_viewport");
  lua_pushcfunction(s, get_viewport);
  lua_setfield(s, -2, "getViewport");
  lua_setfield(s, -2, "display");
  lua_newtable(s);
  lua_pushcfunction(s, icon);
  lua_setfield(s, -2, "icon");
  lua_setfield(s, -2, "ui");
  lua_newtable(s);
  lua_pushcfunction(s, sprite);
  lua_setfield(s, -2, "sprite");
  lua_setfield(s, -2, "graphics");
  lua_newtable(s);
  lua_pushcfunction(s, get_system_language);
  lua_setfield(s, -2, "language");
  lua_pushcfunction(s, public_device_identity);
  lua_setfield(s, -2, "public_device_identity");
  lua_pushcfunction(s, get_system_display_mode);
  lua_setfield(s, -2, "display_mode");
  lua_setfield(s, -2, "system");
  lua_newtable(s);
  lua_pushcfunction(s, json_decode); lua_setfield(s, -2, "decode");
  lua_pushcfunction(s, json_encode); lua_setfield(s, -2, "encode");
  lua_pushlightuserdata(s, &json_null_marker); lua_setfield(s, -2, "null");
  lua_setfield(s, -2, "json");
  lua_newtable(s);
  lua_pushcfunction(s, http_request); lua_setfield(s, -2, "request");
  lua_setfield(s, -2, "http");
  lua_newtable(s);
  lua_pushcfunction(s, audio_stream_start); lua_setfield(s, -2, "stream_start");
  lua_pushcfunction(s, audio_stream_stop); lua_setfield(s, -2, "stream_stop");
  lua_pushcfunction(s, audio_read_opus); lua_setfield(s, -2, "read_opus");
  lua_pushcfunction(s, audio_play_opus); lua_setfield(s, -2, "play_opus");
  lua_setfield(s, -2, "audio");
  lua_setglobal(s, "passport");
  return 0;
}
bool ps_runtime_event(const char *callback, int value) {
  if (!app || *error)
    return false;
  if ((!strcmp(callback, "on_button") ||
       !strcmp(callback, "on_button_down") ||
       !strcmp(callback, "on_button_short") ||
       !strcmp(callback, "on_button_double") ||
       !strcmp(callback, "on_button_long") ||
       !strcmp(callback, "on_button_up") || !strcmp(callback, "on_long")) &&
      !(app->permissions & PS_INPUT))
    return false;
  if (!strcmp(callback, "on_tick") && !(app->permissions & PS_TIMER) &&
      !declarative)
    return false;
#ifdef ESP_PLATFORM
  if (native) return ps_native_event(callback, value);
#endif
  if (L) {
    if (!strcmp(callback, "on_button_short"))
      return call(has_callback(callback) ? callback : "on_button", value);
    if (!strcmp(callback, "on_button_long"))
      return call(has_callback(callback) ? callback : "on_long", value);
    return call(callback, value);
  }
  if (declarative && !strcmp(callback, "on_button_short"))
    return ps_declarative_event("on_button", value);
  if (declarative)
    return ps_declarative_event(callback, value);
  return !*error;
}
void ps_runtime_stop(void) {
#ifdef ESP_PLATFORM
  if (native) ps_native_stop();
#endif
  native = false;
  if (L) {
    if (!*error)
      call("on_stop", 0);
    lua_close(L);
    L = NULL;
  }
  ps_declarative_stop();
  declarative = false;
  app = NULL;
  ps_display_layout_default(&display_layout, PS_DISPLAY_STANDARD);
  view.microphone = false;
  view.audio_stream = false;
  view.count = 0;
  view.image_count = 0;
  view.text[0] = 0;
  view.lua_used = 0;
}
const ps_view *ps_runtime_view(void) { return &view; }
const char *ps_runtime_error(void) { return error; }
const ps_display_layout *ps_runtime_display(void) { return &display_layout; }
void ps_runtime_audio_error(int code) {
#ifdef ESP_PLATFORM
  if (native) ps_native_event("on_audio_error",code);
#endif
  view.microphone = false;
  view.audio_stream = false;
  if (app && L && !*error)
    call("on_audio_error", code);
}
void ps_runtime_audio_allowed(bool allowed) {
  audio_allowed = allowed;
#ifdef ESP_PLATFORM
  if (native) ps_native_audio_allowed(allowed);
#endif
  if (!allowed) {
    view.microphone = false;
    view.audio_stream = false;
    if (ps_audio_stream_prepared()) (void)ps_audio_stop();
    else (void)ps_audio_stream_stop();
  }
}
void ps_runtime_set_system_language(const char *language) {
  system_language = ps_language_parse(language);
}
void ps_runtime_set_system_display_mode(const char *mode) {
  system_light_mode = mode && strcmp(mode, "light") == 0;
}

bool ps_runtime_result(uint32_t gen, int status, const char *data,
                       size_t size) {
  if (!app || gen != generation)
    return false;
  #ifdef ESP_PLATFORM
  if (native) { ps_native_data(1,status,data,size); return true; }
#endif
  if (declarative) {
    ps_declarative_result(status, data, size);
    return true;
  }
  if (L && !*error)
    invoke("on_http", status, data, size, 2);
  return true;
}
void ps_runtime_message(const char *data, size_t size, int opcode) {
#ifdef ESP_PLATFORM
  if (native) { ps_native_data(opcode==2?3:2,opcode,data,size); return; }
#endif
  if (L && !*error)
    invoke("on_message", opcode, data, size, 3);
}
void ps_runtime_audio_frame(const uint8_t *data, size_t size) {
#ifdef ESP_PLATFORM
  if (native) { ps_native_data(4,0,data,size); return; }
#endif
  if (app && L && !*error && (app->permissions & PS_AUDIO_STREAM) &&
      has_callback("on_audio_frame"))
    invoke("on_audio_frame", 0, (const char *)data, size, 4);
}
