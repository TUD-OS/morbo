/* -*- Mode: C -*- */

#include <util.h>

void *
memcpy(void *dest, const void *src, size_t n)
{
  char *d = dest;
  const char *s = src;
  asm volatile  ("rep movsb" : "+D" (d), "+S" (s) , "+c" (n) : : "memory");
  return dest;
}
