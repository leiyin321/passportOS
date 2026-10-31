#pragma once

typedef enum {
  PS_LANGUAGE_EN = 0,
  PS_LANGUAGE_ZH_CN = 1,
  PS_LANGUAGE_COUNT
} ps_language_t;

ps_language_t ps_language_parse(const char *code);
const char *ps_language_code(ps_language_t language);
const char *ps_language_select(ps_language_t language, const char *english,
                               const char *simplified_chinese);
