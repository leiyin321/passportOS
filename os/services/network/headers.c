#include "headers.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
static bool reserved(const char *name) {
  return !strcasecmp(name, "Host") || !strcasecmp(name, "Content-Length") ||
         !strcasecmp(name, "Transfer-Encoding") || !strcasecmp(name, "Connection") ||
         !strcasecmp(name, "Upgrade") || !strncasecmp(name, "Sec-WebSocket-", 14);
}
bool ps_headers_parse(ps_headers *out, const char *raw, size_t n, unsigned max_count) {
  if (!out) return false;
  memset(out, 0, sizeof(*out));
  if (!n) return true;
  if (!raw || n > PS_HEADERS_BYTES || max_count > PS_HEADERS_COUNT || memchr(raw, 0, n))
    return false;
  memcpy(out->data, raw, n);
  size_t pos = 0;
  while (pos < n) {
    size_t end = pos;
    while (end + 1 < n && !(raw[end] == '\r' && raw[end + 1] == '\n')) end++;
    if (end + 1 >= n || out->count >= max_count) return false;
    size_t colon = pos;
    while (colon < end && raw[colon] != ':') colon++;
    if (colon == end || colon == pos || colon - pos > 64) return false;
    for (size_t i = pos; i < colon; i++) {
      unsigned char c = raw[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || strchr("!#$%&'*+-.^_`|~", c))) return false;
    }
    size_t first = colon + 1, last = end;
    while (first < last && (raw[first] == ' ' || raw[first] == '\t')) first++;
    while (last > first && (raw[last - 1] == ' ' || raw[last - 1] == '\t')) last--;
    if (last - first > 256) return false;
    for (size_t i = first; i < last; i++) {
      unsigned char c = raw[i];
      if ((c < 32 && c != '\t') || c == 127) return false;
    }
    out->data[colon] = 0;
    out->data[last] = 0;
    if (reserved(out->data + pos)) return false;
    for (unsigned i = 0; i < out->count; i++)
      if (!strcasecmp(out->data + out->names[i], out->data + pos)) return false;
    out->names[out->count] = (uint16_t)pos;
    out->values[out->count++] = (uint16_t)first;
    pos = end + 2;
  }
  return pos == n;
}
bool ps_headers_format(const ps_headers *h, char *out, size_t capacity, size_t *size) {
  if (!h || !out || !capacity || !size || h->count > PS_HEADERS_COUNT) return false;
  *size = 0;
  out[0] = 0;
  for (unsigned i = 0; i < h->count; i++) {
    int n = snprintf(out + *size, capacity - *size, "%s: %s\r\n",
                     h->data + h->names[i], h->data + h->values[i]);
    if (n < 0 || (size_t)n >= capacity - *size) return false;
    *size += (size_t)n;
  }
  return true;
}
