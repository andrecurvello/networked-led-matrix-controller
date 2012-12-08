#ifndef _VARTEXT_H
#define _VARTEXT_H

#include <stdlib.h>
#include <string.h>

#define ARG(name) , const char *name

#define CONST(n, str)             \
  static const char *str_##n = str;             \
  c = sizeof(str)-1;                            \
  memcpy(dst, str_##n, c);                      \
  dst += c;

#define VAR(var)                                \
  c = strlen(var);                              \
  memcpy(dst, var, c);                          \
  dst += c;

#define TEXT_TEMPLATE(name, args, body)         \
  char *name##_expand(char *dst args) {         \
    int c;                                      \
    body;                                       \
    *dst = '\0';                                \
    return dst;                                 \
  }

#endif
