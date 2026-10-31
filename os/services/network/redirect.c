#include "redirect.h"
#include <stdio.h>
#include <string.h>
bool ps_redirect_status(int code) {
  return code == 301 || code == 302 || code == 303 || code == 307 ||
         code == 308;
}
bool ps_redirect_url(const char *base, const char *location, char *out,
                     size_t cap) {
  if (!base || !location || !*location || strlen(base) > 500 ||
      strlen(location) > 500)
    return false;
  for (const char *p = location; *p; p++)
    if ((unsigned char)*p <= 32 || *p == '\\' || *p == '@')
      return false;
  const char *scheme = strstr(base, "://");
  if (!scheme)
    return false;
  int n;
  if (strstr(location, "://"))
    n = snprintf(out, cap, "%s", location);
  else if (location[0] == '/' && location[1] == '/')
    n = snprintf(out, cap, "%.*s:%s", (int)(scheme - base), base, location);
  else {
    const char *host = scheme + 3, *path = strchr(host, '/');
    size_t origin = path ? (size_t)(path - base) : strlen(base);
    if (*location == '/')
      n = snprintf(out, cap, "%.*s%s", (int)origin, base, location);
    else if (*location == '?') {
      const char *q = strchr(base, '?');
      size_t end = q ? (size_t)(q - base) : strlen(base);
      n = snprintf(out, cap, "%.*s%s%s", (int)end, base, path ? "" : "/",
                   location);
    } else {
      const char *end = base + strlen(base), *q = strchr(base, '?');
      if (q)
        end = q;
      while (end > base + origin && end[-1] != '/')
        end--;
      if (end == base + origin)
        n = snprintf(out, cap, "%.*s/%s", (int)origin, base, location);
      else
        n = snprintf(out, cap, "%.*s%s", (int)(end - base), base, location);
    }
  }
  if (n < 0 || (size_t)n >= cap || n > 500)
    return false;
  char *fragment = strchr(out, '#');
  if (fragment)
    *fragment = 0;
  return true; // Caller MUST authorize and freshly resolve the resulting URL at
               // every hop.
}
