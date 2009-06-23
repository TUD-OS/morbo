/* -*- Mode: C -*- */

/* Taken from FreeBSD. Original copyright notice below. */

/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2006 M. Warner Losh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 *
 * $FreeBSD$
 */

#include <stdarg.h>
#include <util.h>

static const char *hex = "0123456789abcdef";

void
vprintf(const char *fmt, va_list ap)
{
  char buf[10];
  char *s;
  unsigned u;
  int c;
  
  while ((c = *fmt++)) {
    if (c == '%') {
      c = *fmt++;
      switch (c) {
      case 'c':
	out_char(va_arg(ap, int));
	continue;
      case 's':
	for (s = va_arg(ap, char *); *s; s++)
	  out_char(*s);
	continue;
      case 'd':       /* A lie, always prints unsigned */
      case 'u':
	u = va_arg(ap, unsigned);
	s = buf;
	do
	  *s++ = '0' + u % 10U;
	while (u /= 10U);
      dumpbuf:;
	while (--s >= buf)
	    out_char(*s);
	continue;
      case 'x':
	u = va_arg(ap, unsigned);
	s = buf;
	do
	  *s++ = hex[u & 0xfu];
	while (u >>= 4);
	goto dumpbuf;
      }
    }
    out_char(c);
  }
  
  return;
  
}

void
printf(const char *fmt,...)
{
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

// EOF
