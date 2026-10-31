#include "package.h"
#include <string.h>
bool ps_json_safe(const cJSON *j, unsigned depth) {
  if (!j || depth > 12)
    return false;
  unsigned n = 0;
  for (const cJSON *a = j->child; a; a = a->next) {
    if (++n > 128)
      return false;
    if (cJSON_IsObject(j)) {
      if (!a->string || strlen(a->string) > 64)
        return false;
      for (const cJSON *b = a->next; b; b = b->next)
        if (b->string && !strcmp(a->string, b->string))
          return false;
    }
    if (cJSON_IsString(a) && strlen(a->valuestring) > 1024)
      return false;
    if (!ps_json_safe(a, depth + 1))
      return false;
  }
  return true;
}

cJSON *ps_json_parse_budget(const char *raw, size_t length,
                            unsigned token_limit) {
  if (!raw || length > 16384 || memchr(raw, 0, length))
    return NULL;
  bool quoted = false, escaped = false;
  unsigned depth = 0, tokens = 0;
  for (size_t i = 0; i < length; i++) {
    char c = raw[i];
    if (quoted) {
      if (escaped) {
        escaped = false;
        if (c == 'u' && i + 4 < length && !memcmp(raw + i + 1, "0000", 4))
          return NULL;
      } else if (c == '\\')
        escaped = true;
      else if (c == '"')
        quoted = false;
      continue;
    }
    if (c == '"') {
      quoted = true;
      tokens++;
    } else if (c == '{' || c == '[') {
      if (++depth > 12)
        return NULL;
      tokens++;
    } else if (c == '}' || c == ']') {
      if (!depth)
        return NULL;
      depth--;
    } else if (c == ',' || c == ':')
      tokens++;
    if (tokens > token_limit)
      return NULL;
  }
  if (quoted || depth)
    return NULL;
  cJSON *j = cJSON_ParseWithLengthOpts(raw, length + 1, NULL, true);
  if (j && !ps_json_safe(j, 0)) {
    cJSON_Delete(j);
    return NULL;
  }
  return j;
}

cJSON *ps_json_parse(const char *raw, size_t length) {
  return ps_json_parse_budget(raw, length, 512);
}

/* Bound parser recursion and allocation before entering cJSON. Object keys do
 * not count as values; syntax validation remains the parser's responsibility. */
bool ps_json_preflight(const char *raw, size_t n, unsigned max_depth,
                       unsigned max_values) {
  if (!raw || !n || memchr(raw, 0, n)) return false;
  unsigned depth = 0, values = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = raw[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == ',') continue;
    if (c == '{' || c == '[') {
      if (++depth > max_depth || ++values > max_values) return false;
    } else if (c == '}' || c == ']') {
      if (!depth) return false;
      depth--;
    } else if (c == '"') {
      bool closed = false;
      while (++i < n) {
        if (raw[i] == '"') { closed = true; break; }
        if (raw[i] == '\\') {
          if (++i >= n) return false;
          if (raw[i] == 'u' && i + 4 < n && !memcmp(raw + i + 1, "0000", 4)) return false;
        }
      }
      if (!closed) return false;
      size_t next = i + 1;
      while (next < n && (raw[next] == ' ' || raw[next] == '\t' || raw[next] == '\r' || raw[next] == '\n')) next++;
      if ((next == n || raw[next] != ':') && ++values > max_values) return false;
    } else {
      if (++values > max_values) return false;
      while (i + 1 < n && raw[i + 1] != ',' && raw[i + 1] != ']' && raw[i + 1] != '}' &&
             raw[i + 1] != '[' && raw[i + 1] != '{' && raw[i + 1] != '"' &&
             raw[i + 1] != ' ' && raw[i + 1] != '\t' && raw[i + 1] != '\r' && raw[i + 1] != '\n') i++;
    }
  }
  return !depth;
}
