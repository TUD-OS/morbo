/* -*- Mode: C -*- */

#include <util.h>

size_t
strlen(const char *s)
{
  size_t size = 0;
  while (*(s++) != 0) { size++; }
  return size;
}
