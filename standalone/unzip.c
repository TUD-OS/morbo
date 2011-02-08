/* -*- Mode: C -*- */

#include <pci.h>
#include <mbi.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>

int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();
  if (magic != MBI_MAGIC) {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nUnzip %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("Trying to relocate and uncompress all modules.\n"
         "This should be the first boot chainloader, otherwise our simplistic memory\n"
         "management will probably fail.\n");

  return start_module(mbi, true);
}
