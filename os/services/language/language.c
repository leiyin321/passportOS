#include "language.h"
#include <string.h>

ps_language_t ps_language_parse(const char *code) {
  return code && (!strcmp(code, "zh-CN") || !strcmp(code, "zh"))
             ? PS_LANGUAGE_ZH_CN
             : PS_LANGUAGE_EN;
}

const char *ps_language_code(ps_language_t language) {
  return language == PS_LANGUAGE_ZH_CN ? "zh-CN" : "en";
}

const char *ps_language_select(ps_language_t language, const char *english,
                               const char *simplified_chinese) {
  return language == PS_LANGUAGE_ZH_CN ? simplified_chinese : english;
}
