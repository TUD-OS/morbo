/* -*- Mode: C -*- */

#include <pci.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <mbi-tools.h>


static uint32_t
ioapic_indirect_read(volatile void *ioapic, uint8_t reg)
{
  volatile char *r = ioapic;
  *r = reg;
  return *(volatile uint32_t *)(r + 0x10);
}


int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();

  volatile uint32_t *ioapic_base = (uint32_t*)0xFEC00000;
  
  printf("IOAPIC ID  %08x\n", ioapic_indirect_read(ioapic_base, 0));
  printf("IOAPIC VER %08x\n", ioapic_indirect_read(ioapic_base, 1));

  printf("Done\n");
  while (true)
    asm volatile ("hlt");
}
