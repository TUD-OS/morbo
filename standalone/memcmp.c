/* -*- Mode: C -*- */

#include <util.h>

int
memcmp(const void *s1, const void *s2, size_t n)
{
  const char *p1 = s1;
  const char *p2 = s2;
  int res = 0;
  while (n-- && (res == 0)) {
    res = *(p1++) - *(p2++);
  }
  return res;
}
