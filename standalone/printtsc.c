/* -*- Mode: C -*- */

#include <pci.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <mbi-tools.h>

int
main()
{
  serial_init();

  printf("! %s:%d PERF: tsc %llu cycles ok\n", __FILE__, __LINE__, rdtsc());
  printf("wvtest: done\n");

  return 0;
}
