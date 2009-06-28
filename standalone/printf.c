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
  char buf[64];
  char *s;
  unsigned u;
  unsigned long long ull;
  int c;
  enum { PLAIN, INFMT } state = PLAIN;
  unsigned longness = 0;
  
  while ((c = *fmt++)) {
    
    switch (state) {
    case PLAIN:
      switch (c) {
      case '%':
        state = INFMT;
        longness = 0;
        break;
      default:
        out_char(c);
        break;
      }
      break;                    /* case PLAIN */
    case INFMT:
      switch (c) {
      case 'l':
        longness++;
        break;
      case 'c':
	out_char(va_arg(ap, int));
        state = PLAIN;
        break;
      case 's':
        out_string(va_arg(ap, char *));
        state = PLAIN;
	break;
      case 'd':       /* A lie, always prints unsigned */
      case 'u':
        /* XXX Support longness */
        u = va_arg(ap, unsigned);
	s = buf;
	do
	  *s++ = '0' + u % 10U;
	while (u /= 10U);
      dumpbuf:;
	while (--s >= buf)
	    out_char(*s);
        state = PLAIN;
	break;
      case 'x':
        if (longness < 2)
          ull = va_arg(ap, unsigned);
        else
          ull = va_arg(ap, unsigned long long);
	s = buf;
	do
	  *s++ = hex[ull & 0xF];
	while (ull >>= 4);
	goto dumpbuf;
      }
      break;                    /* case INFMT */
    }
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
