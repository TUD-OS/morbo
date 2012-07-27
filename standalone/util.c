/*
 * \brief   Utility functions for a bootloader
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


#include <stdarg.h>
#include <serial.h>
#include <util.h>

/**
 * Wait roughly a given number of milliseconds.
 *
 * We use the PIT for this.
 */
void
wait(int ms)
{
  /* the PIT counts with 1.193 Mhz */
  ms*=1193;

  /* initalize the PIT, let counter0 count from 256 backwards */
  outb(0x43,0x14);
  outb(0x40,0);

  unsigned char state;
  unsigned char old = 0;
  while (ms>0)
    {
      outb(0x43,0);
      state = inb(0x40);
      ms -= (unsigned char)(old - state);
      old = state;
    }
}

/**
 * Print the exit status and reboot the machine.
 */
void __attribute__((regparm(1), noreturn))
__exit(unsigned status)
{
  const unsigned delay = 300;

  printf("\nExit with status %u.\n"
         "Rebooting...\n", status);

  for (unsigned i=0; i<delay;i++) {
    wait(1000);
    out_char('.');
  }

  printf("-> OK, reboot now!\n");
  reboot();
  /* NOT REACHED */
}

/**
 * Output a single char.
 * Note: We allow only to put a char on the last line.
 */
int
out_char(unsigned value)
{
#define BASE(ROW) ((unsigned short *) (0xb8000+ROW*160))
  static unsigned int col;
  if (value!='\n')
    {
      unsigned short *p = BASE(24)+col;
      *p = 0x0f00 | value;
      col++;
    }
  if (col>=80 || value == '\n')
    {
      col=0;
      unsigned short *p=BASE(0);
      memcpy(p, p+80, 24*160);
      memset(BASE(24), 0, 160);
    }


  serial_send(value);

  if (value == '\n')
    serial_send('\r');

  return value;
}


/**
 * Output a string.
 */
void
out_string(const char *value)
{
  for(; *value; value++)
    out_char(*value);
}

/* EOF */
