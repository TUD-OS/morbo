/* -*- Mode: C -*- */

#include <util.h>

void hexdump(const void *p, unsigned len)
{
  const unsigned chars_per_row = 16;
  const char *data = (const char *)(p);
  const char *data_end = data + len;

  for (unsigned cur = 0; cur < len; cur += chars_per_row) {
    printf("%08x:", cur);
    for (unsigned i = 0; i < chars_per_row; i++)
      if (data+i < data_end)
        printf(" %02x", ((const unsigned char *)data)[i]);
      else
        printf("   ");
    printf(" | ");
    for (unsigned i = 0; i < chars_per_row; i++) {
      if (data < data_end)
        printf("%c", ((data[0] >= 32) && (data[0] > 0)) ? data[0] : '.');
      else
        printf(" ");
      data++;
    }
    printf("\n");
  }
}

/* EOF */
