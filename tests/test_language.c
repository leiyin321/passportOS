#include "language.h"
#include <assert.h>
#include <string.h>

int main(void) {
  assert(ps_language_parse("en") == PS_LANGUAGE_EN);
  assert(ps_language_parse("zh-CN") == PS_LANGUAGE_ZH_CN);
  assert(ps_language_parse("zh") == PS_LANGUAGE_ZH_CN);
  assert(ps_language_parse("fr") == PS_LANGUAGE_EN);
  assert(!strcmp(ps_language_code(PS_LANGUAGE_ZH_CN), "zh-CN"));
  assert(!strcmp(ps_language_code(PS_LANGUAGE_EN), "en"));
  assert(!strcmp(ps_language_select(PS_LANGUAGE_ZH_CN, "English", "中文"),
                 "中文"));
  assert(!strcmp(ps_language_select(PS_LANGUAGE_EN, "English", "中文"),
                 "English"));
  return 0;
}
