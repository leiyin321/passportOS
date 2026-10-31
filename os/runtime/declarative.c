#include "declarative.h"
#include "asset_loader.h"
#include "icon_registry.h"
#include "network.h"
#include "store.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static cJSON *document, *state;
static ps_view *output;
static ps_package *package;
static uint32_t generation;
static unsigned timer_ms;
static const ps_display_layout *display_layout;
static const char *text(const cJSON *o, const char *key) {
  const cJSON *j = cJSON_GetObjectItemCaseSensitive(o, key);
  return cJSON_IsString(j) ? j->valuestring : "";
}
static cJSON *field(cJSON *root, const char *path) {
  if (!path || strlen(path) > 128)
    return NULL;
  const char *p = path;
  cJSON *value = root;
  unsigned depth = 0;
  while (*p && value) {
    if (++depth > 12)
      return NULL;
    char key[65];
    unsigned n = 0;
    while (*p && *p != '.' && *p != '[') {
      if (n >= 64)
        return NULL;
      char c = *p++;
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_'))
        return NULL;
      key[n++] = c;
    }
    key[n] = 0;
    if (n)
      value = cJSON_GetObjectItemCaseSensitive(value, key);
    while (*p == '[' && value) {
      p++;
      unsigned i = 0, digits = 0;
      while (*p >= '0' && *p <= '9') {
        if (++digits > 3)
          return NULL;
        i = i * 10 + *p++ - '0';
      }
      if (!digits || *p++ != ']')
        return NULL;
      value = cJSON_GetArrayItem(value, i);
    }
    if (*p == '.')
      p++;
    else if (*p)
      return NULL;
  }
  return value;
}
static void string_value(const cJSON *v, char *out, size_t cap) {
  if (cJSON_IsString(v))
    snprintf(out, cap, "%.200s", v->valuestring);
  else if (cJSON_IsNumber(v) && isfinite(v->valuedouble))
    snprintf(out, cap, "%.8g", v->valuedouble);
  else if (cJSON_IsBool(v))
    snprintf(out, cap, "%s", cJSON_IsTrue(v) ? "yes" : "no");
  else
    snprintf(out, cap, "--");
}
static void put(const char *key, cJSON *value) {
  if (!key || !*key || strlen(key) > 64 || !value ||
      (!strncmp(key, "display.", 8) || !strcmp(key, "display"))) {
    cJSON_Delete(value);
    return;
  }
  cJSON_DeleteItemFromObjectCaseSensitive(state, key);
  cJSON_AddItemToObject(state, key, value);
}
static void append(const char *s) {
  size_t n = strlen(output->text);
  if (n < sizeof(output->text) - 1)
    snprintf(output->text + n, sizeof(output->text) - n, "%s", s);
}
static bool nodes(cJSON *list, unsigned depth, unsigned *count) {
  if (depth > 8 || !cJSON_IsArray(list))
    return false;
  for (cJSON *node = list->child; node; node = node->next) {
    if (++*count > 64)
      return false;
    const char *type = text(node, "type");
    if (!strcmp(type, "icon") || !strcmp(type, "image")) {
      if (output->image_count >= PS_IMAGE_MAX)
        return false;
      cJSON *xj = cJSON_GetObjectItemCaseSensitive(node, "x"),
            *yj = cJSON_GetObjectItemCaseSensitive(node, "y"),
            *sj = cJSON_GetObjectItemCaseSensitive(node, "size");
      int x = cJSON_IsNumber(xj) ? xj->valueint
                                 : (output->image_count % 4) * 50,
          y = cJSON_IsNumber(yj) ? yj->valueint
                                 : (output->image_count / 4) * 40,
          size = cJSON_IsNumber(sj) ? sj->valueint : 24;
      if (x < 0 || y < 0 || x > 192 || y > 156 || size < 12 || size > 48)
        return false;
      if (!strcmp(type, "icon")) {
        output->images[output->image_count++] =
            (ps_image_command){.system = 1,
                               .handle = ps_icon_index(text(node, "name")),
                               .x = x,
                               .y = y,
                               .size = size,
                               .color = 0x68d5e8};
      } else {
        uint32_t handle = ps_asset_open(package, text(node, "src"));
        ps_asset_info info;
        if (handle && ps_asset_get(handle, &info) &&
            (x > display_layout->viewport.width - info.width || y > display_layout->viewport.height - info.height))
          return false;
        output->images[output->image_count++] =
            (ps_image_command){.system = handle ? 0 : 1,
                               .handle = handle,
                               .x = x,
                               .y = y,
                               .size = 24,
                               .color = 0xe9f0fa};
      }
    } else if (!strcmp(type, "row") || !strcmp(type, "column") ||
               !strcmp(type, "dialog")) {
      if (!nodes(cJSON_GetObjectItemCaseSensitive(node, "children"), depth + 1,
                 count))
        return false;
    } else if (!strcmp(type, "text") || !strcmp(type, "button") ||
               !strcmp(type, "progress") || !strcmp(type, "select") ||
               !strcmp(type, "list")) {
      append(text(node, "label"));
      const char *binding = text(node, "bind");
      char v[224];
      if (*binding) {
        string_value(field(state, binding), v, sizeof(v));
        append(v);
      } else
        append(text(node, "text"));
      append("\n");
    } else
      return false;
  }
  return true;
}
static void render(void) {
  output->text[0] = 0;
  output->image_count = 0;
  unsigned count = 0;
  if (!nodes(cJSON_GetObjectItemCaseSensitive(document, "ui"), 0, &count))
    snprintf(output->text, sizeof(output->text), "Invalid UI tree");
}
static bool save(void) {
  if (!(package->permissions & PS_STORAGE))
    return false;
  cJSON *persistent = cJSON_Duplicate(state, true);
  if (!persistent) return false;
  cJSON_DeleteItemFromObjectCaseSensitive(persistent, "display");
  char *s = cJSON_PrintUnformatted(persistent);
  cJSON_Delete(persistent);
  if (!s) return false;
  bool ok = ps_data_set(package, "state", s);
  free(s);
  return ok;
}
static bool expand(const char *template, char out[512]) {
  size_t n = 0;
  for (const char *p = template; *p;) {
    if (n >= 500)
      return false;
    if (*p == '{') {
      p++;
      char key[129];
      size_t k = 0;
      while (*p && *p != '}') {
        if (k >= 128)
          return false;
        key[k++] = *p++;
      }
      if (*p++ != '}')
        return false;
      key[k] = 0;
      char v[224];
      string_value(field(state, key), v, sizeof(v));
      for (char *c = v; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
              (*c >= '0' && *c <= '9') || *c == '-' || *c == '.' || *c == '_'))
          return false;
      size_t l = strlen(v);
      if (n + l > 500)
        return false;
      memcpy(out + n, v, l);
      n += l;
    } else
      out[n++] = *p++;
  }
  out[n] = 0;
  return true;
}
static bool fetch(void) {
  cJSON *ds = cJSON_GetObjectItemCaseSensitive(document, "datasource");
  char url[512];
  if (!ds || !expand(text(ds, "url"), url))
    return false;
  bool ok = ps_net_fetch(package, url, generation, false);
  put("_status",
      cJSON_CreateString(ok ? "Loading..." : "Offline / request denied"));
  return ok;
}
static bool action(cJSON *a, unsigned depth) {
  if (!a || depth > 8)
    return false;
  if (cJSON_IsArray(a)) {
    for (cJSON *v = a->child; v; v = v->next)
      if (!action(v, depth + 1))
        return false;
    return true;
  }
  const char *type = text(a, "type"), *key = text(a, "field");
  cJSON *value = field(state, key);
  if (!strcmp(type, "increment")) {
    cJSON *amount = cJSON_GetObjectItemCaseSensitive(a, "amount");
    double v = cJSON_IsNumber(value) ? value->valuedouble : 0;
    v += cJSON_IsNumber(amount) ? amount->valuedouble : 1;
    if (!isfinite(v) || fabs(v) > 999999)
      return false;
    put(key, cJSON_CreateNumber(v));
    return true;
  }
  if (!strcmp(type, "set")) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(a, "value");
    if (!v)
      return false;
    put(key, cJSON_Duplicate(v, true));
    return true;
  }
  if (!strcmp(type, "save")) {
    bool ok = save();
    put("_status", cJSON_CreateString(ok ? "Saved" : "Save failed / quota"));
    return ok;
  }
  if (!strcmp(type, "fetch")) {
    fetch();
    return true;
  }
  if (!strcmp(type, "select")) {
    cJSON *options =
        cJSON_GetObjectItemCaseSensitive(document, text(a, "options"));
    int n = cJSON_GetArraySize(options);
    if (n < 1 || n > 32)
      return false;
    const char *index = text(a, "index");
    cJSON *i = field(state, index),
          *step = cJSON_GetObjectItemCaseSensitive(a, "step");
    int current = cJSON_IsNumber(i) ? i->valueint : 0;
    current = (current + (cJSON_IsNumber(step) ? step->valueint : 1) + n) % n;
    put(index, cJSON_CreateNumber(current));
    put(key, cJSON_Duplicate(cJSON_GetArrayItem(options, current), true));
    return true;
  }
  if (!strcmp(type, "conditional")) {
    return action(cJSON_GetObjectItemCaseSensitive(
                      a, cJSON_IsTrue(value) ? "then" : "else"),
                  depth + 1);
  }
  return false;
}
bool ps_declarative_start(ps_package *app, ps_view *view, uint32_t gen,
                          const ps_display_layout *display) {
  ps_declarative_stop();
  if(!(app->permissions&PS_UI))return false;
  package = app;
  output = view;
  generation = gen;
  display_layout = display;
  timer_ms = 0;
  document = ps_json_parse(app->code, strlen(app->code));
  if (!document || !ps_json_safe(document, 0) ||
      strcmp(text(document, "format"), "passport.ui/1"))
    return false;
  state = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(document, "state"),
                          true);
  if (!cJSON_IsObject(state))
    return false;
  char persisted[1025];
  if (ps_data_get(package, "state", persisted, sizeof(persisted))) {
    cJSON *p = ps_json_parse(persisted, strlen(persisted));
    if (cJSON_IsObject(p) && ps_json_safe(p, 0)) {
      cJSON_Delete(state);
      state = p;
      put("_status", cJSON_CreateString("Cached (age unknown)"));
    } else
      cJSON_Delete(p);
  }
  cJSON_DeleteItemFromObjectCaseSensitive(state, "display");
  cJSON *display_json = cJSON_CreateObject();
  cJSON_AddNumberToObject(display_json, "width", display_layout->viewport.width);
  cJSON_AddNumberToObject(display_json, "height", display_layout->viewport.height);
  cJSON_AddStringToObject(display_json, "mode", display_layout->mode == PS_DISPLAY_FULLSCREEN ? "fullscreen" : "standard");
  cJSON *insets = cJSON_CreateObject();
  cJSON_AddNumberToObject(insets, "left", 0); cJSON_AddNumberToObject(insets, "top", 0);
  cJSON_AddNumberToObject(insets, "right", 0); cJSON_AddNumberToObject(insets, "bottom", 0);
  cJSON_AddItemToObject(display_json, "safeInsets", insets);
  cJSON_AddItemToObject(state, "display", display_json);
  unsigned count = 0;
  output->text[0] = 0;
  output->image_count = 0;
  if (!nodes(cJSON_GetObjectItemCaseSensitive(document, "ui"), 0, &count))
    return false;
  cJSON *on = cJSON_GetObjectItemCaseSensitive(document, "on_start");
  if (on)
    action(on, 0);
  render();
  return true;
}
bool ps_declarative_event(const char *callback, int value) {
  if (!document || !state)
    return false;
  if (!strcmp(callback, "on_button") && value >= 0 && value < 3) {
    const char *keys[] = {"up", "down", "ok"};
    cJSON *actions = cJSON_GetObjectItemCaseSensitive(document, "actions");
    cJSON *a = cJSON_GetObjectItemCaseSensitive(actions, keys[value]);
    if (a && !action(a, 0))
      put("_status", cJSON_CreateString("Action failed / denied"));
    render();
  }
  if (!strcmp(callback, "on_tick") && (package->permissions & PS_TIMER)) {
    timer_ms += value;
    cJSON *timer = cJSON_GetObjectItemCaseSensitive(document, "timer"),
          *period = cJSON_GetObjectItemCaseSensitive(timer, "period_ms");
    if (cJSON_IsNumber(period) && period->valueint >= 1000 &&
        timer_ms >= (unsigned)period->valueint) {
      timer_ms = 0;
      action(cJSON_GetObjectItemCaseSensitive(timer, "action"), 0);
      render();
    }
  }
  return true;
}
void ps_declarative_result(int status, const char *data, size_t size) {
  if (!document || !state)
    return;
  cJSON *response =
      status == 200 && size <= 4096 ? ps_json_parse(data, size) : NULL;
  if (response && ps_json_safe(response, 0)) {
    put("response", response);
    put("_status", cJSON_CreateString("Fresh / Open-Meteo.com"));
    save();
  } else {
    cJSON_Delete(response);
    put("_status", cJSON_CreateString("Request failed; cached data"));
  }
  render();
}
void ps_declarative_stop(void) {
  cJSON_Delete(document);
  cJSON_Delete(state);
  document = state = NULL;
  output = NULL;
  package = NULL;
  display_layout = NULL;
}
