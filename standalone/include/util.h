/*
 * \brief   utility macros and headers for util.c
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#include "asm.h"

#define assert(X, msg, args...)						\
  do {									\
    if (!(X)) {								\
      printf ("Assertion \"%s\" failed at %s:%d\n" msg, #X,		\
	      __FILE__, __LINE__, ## args);				\
      __exit(0xbad);							\
    }									\
  } while (0)


#define MAX(a, b) ({ typeof (a) _a = (a); \
      typeof (b) _b = (b);		  \
      _a > _b ? _a : _b; })

#define MIN(a, b) ({ typeof (a) _a = (a); \
      typeof (b) _b = (b);		  \
      _a > _b ? _b : _a; })

char *strtok_r(char *s, const char *delim, char **save_ptr);
char *strtok(char *s, const char *delim);
unsigned long long strtoull(const char * __restrict nptr, char ** __restrict endptr, int base);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);
char *strcpy(char * __restrict dst, const char * __restrict src);
char *strncpy(char * __restrict dst, const char * __restrict src, size_t n);
size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

/* Low-level output functions */
int  out_char(unsigned value);
void out_string(const char *value);

/* Poor man's isspace */
#define isspace(c) ((c) == ' ')

void vprintf(const char *fmt, va_list ap);
void printf(const char *fmt, ...);
void hexdump(const void *p, unsigned len);

/* Helper functions. */
void wait(int ms);
void __exit(unsigned status) __attribute__((regparm(3), noreturn));
void reboot(void) __attribute__((noreturn));
void torealmode(uint32_t addr) __attribute__((regparm(3), noreturn));

/* Boot info */
extern struct mbi *multiboot_info;

/* EOF */
