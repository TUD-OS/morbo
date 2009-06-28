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

/**
 * we want inlined stringops
 */
#define memcpy(x,y,z) __builtin_memcpy(x,y,z)
#define memset(x,y,z) __builtin_memset(x,y,z)
#define strlen(s) __builtin_strlen(s)
#define strcmp(s1, s2) __builtin_strcmp(s1, s2)
#define strncmp(s1, s2) __builtin_strncmp(s1, s2)
#define strcpy(d, s) __builtin_strcpy(d, s)
//#define strncpy(d, s, n) __builtin_strncpy(d, s, n)

char *strtok(char *s, const char *delim);
char *strncpy(char * __restrict dst, const char * __restrict src, size_t n);

/**
 * Returns result and prints the msg, if value is true.
 */
#define CHECK3(result, value, msg)			\
  {							\
    if (value)						\
      {							\
	out_info(msg);					\
	return result;					\
      }							\
  }

/**
 * Returns result and prints the msg and hex, if value is true.
 */
#define CHECK4(result, value, msg, hex)			\
  {							\
    if (value)						\
      {							\
	out_description(msg, hex);			\
	return result;					\
      }							\
  }



/**
 * lowlevel output functions
 */
int  out_char(unsigned value);
void out_unsigned(unsigned int value, int len, unsigned base, char flag);
void out_string(const char *value);
void out_hex(unsigned int value, unsigned int bitlen);
static inline void terpri(void) { out_char('\n'); }

/**
 * every message with out_description is prefixed with message_label
 */
extern const char message_label[];
void out_description(const char *prefix, unsigned int value);
void out_info(const char *msg);

/* Poor man's isspace */
#define isspace(c) ((c) == ' ')

void vprintf(const char *fmt, va_list ap);
void printf(const char *fmt, ...);

/**
 * Helper functions.
 */
void wait(int ms);
void __exit(unsigned status) __attribute__((noreturn));
void reboot(void) __attribute__((noreturn));

/* EOF */
