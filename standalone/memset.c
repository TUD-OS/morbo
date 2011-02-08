/* -*- Mode: C -*- */

#include <util.h>

void *
memset(void *dst, int c, size_t n)
{
  const char *d = dst;
  asm volatile  ("rep stosb" : "+D" (d), "+c" (n) : "a" (c) : "memory");
  return dst;
}
