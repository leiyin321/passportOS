#include "wifi_setup_form.h"

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool decode_value(const char *src, size_t size, char *out, size_t cap,
                         size_t *written) {
  size_t n = 0;
  for (size_t i = 0; i < size; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == '+') c = ' ';
    else if (c == '%') {
      if (i + 2 >= size) return false;
      int hi = hex_value(src[++i]), lo = hex_value(src[++i]);
      if (hi < 0 || lo < 0) return false;
      c = (unsigned char)((hi << 4) | lo);
    }
    if (!c || c < 0x20 || c == 0x7f || n + 1 >= cap) return false;
    out[n++] = (char)c;
  }
  out[n] = 0;
  *written = n;
  return true;
}

bool ps_wifi_setup_parse_form(const char *body, size_t size, char *ssid,
                              size_t ssid_capacity, char *password,
                              size_t password_capacity) {
  if (!body || !ssid || !password || !ssid_capacity || !password_capacity ||
      size > 512)
    return false;
  ssid[0] = password[0] = 0;
  bool got_ssid = false, got_password = false;
  for (size_t start = 0; start < size;) {
    size_t end = start;
    while (end < size && body[end] != '&') end++;
    size_t equal = start;
    while (equal < end && body[equal] != '=') equal++;
    if (equal == end) return false;
    const char *key = body + start;
    size_t key_size = equal - start, value_size = end - equal - 1;
    size_t written = 0;
    if (key_size == 4 && key[0] == 's' && key[1] == 's' && key[2] == 'i' &&
        key[3] == 'd') {
      if (got_ssid || !decode_value(body + equal + 1, value_size, ssid,
                                    ssid_capacity, &written) || !written)
        return false;
      got_ssid = true;
    } else if (key_size == 8 && key[0] == 'p' && key[1] == 'a' && key[2] == 's' &&
               key[3] == 's' && key[4] == 'w' && key[5] == 'o' && key[6] == 'r' &&
               key[7] == 'd') {
      if (got_password || !decode_value(body + equal + 1, value_size, password,
                                        password_capacity, &written))
        return false;
      got_password = true;
    } else {
      return false;
    }
    start = end + 1;
  }
  if (!got_ssid || !got_password ||
      (password[0] && (password_capacity <= 8 ||
                       __builtin_strlen(password) < 8 ||
                       __builtin_strlen(password) > 63)))
    return false;
  return true;
}
