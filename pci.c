/*
 * \brief   DEV and PCI code.
 * \date    2006-10-25
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


#include <util.h>
#include <pci.h>

/**
 * Read a byte from the pci config space.
 */
unsigned char
pci_read_byte(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inb(PCI_DATA_PORT + (addr & 3));
}

/**
 * Read a word from the pci config space.
 */
unsigned short
pci_read_word(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT + (addr & 2));
}


/**
 * Read a long from the pci config space.
 */
unsigned
pci_read_long(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inl(PCI_DATA_PORT);
}


/**
 * Write a word to the pci config space.
 */
void
pci_write_word(unsigned addr, unsigned short value)
{
  outl(PCI_ADDR_PORT, addr);
  outw(PCI_DATA_PORT + (addr & 2), value);
}


/**
 * Write a long to the pci config space.
 */
void
pci_write_long(unsigned addr, unsigned value)
{
  outl(PCI_ADDR_PORT, addr);
  outl(PCI_DATA_PORT, value);
}


/**
 * Read a word from the pci config space.
 */
static inline
unsigned short
pci_read_word_aligned(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT);
}

static inline
void
pci_write_word_aligned(unsigned addr, unsigned short value)
{
  outl(PCI_ADDR_PORT, addr);
  outw(PCI_DATA_PORT, value);
}


/**
 * Return an pci config space address of a device with the given
 * class/subclass id or 0 on error.
 *
 * Note: this returns the last device found!
 */
uint32_t
pci_find_device_by_class(uint8_t class, uint8_t subclass)
{
  uint16_t full_class = class << 8 | subclass;

  uint32_t res = 0;
  for (unsigned i=0; i<1<<13; i++) {
    uint8_t maxfunc = 0;
    
    for (unsigned func = 0; func <= maxfunc; func++) {
      uint32_t addr = 0x80000000 | i<<11 | func<<8;

      if (!maxfunc && pci_read_byte(addr+14) & 0x80)
	maxfunc=7;
      if (full_class == (pci_read_long(addr+0x8) >> 16))
	res = addr;
    }
  }

  return res;
}


/**
 * Return an pci config space address of a device with the given
 * device/vendor id or 0 on error.
 */
uint32_t
pci_find_device_by_id(uint16_t id)
{
  uint32_t res = 0;

  for (unsigned i=0; i<1<<13; i++) {
    uint8_t maxfunc = 0;

    for (unsigned func = 0; func <= maxfunc; func++) {
      uint32_t addr = 0x80000000 | i<<11 | func<<8;

      if (!maxfunc && pci_read_byte(addr+14) & 0x80)
	maxfunc=7;
      if (id == (pci_read_long(addr+0x8)))
	res = addr;
    }
  }
  
  return res;
}


/**
 * Find a capability for a device in the capability list.
 * @param addr - address of the device in the pci config space
 * @param id   - the capability id to search.
 * @return 0 on failiure or the offset into the pci device of the capability
 */
static
uint8_t
pci_dev_find_cap(unsigned addr, unsigned char id)
{
  if ((pci_read_long(addr+PCI_CONF_HDR_CMD) & 0x100000) == 0) {
    return 0;
  }

  unsigned char cap_offset = pci_read_byte(addr+PCI_CONF_HDR_CAP);

  while (cap_offset) {
    if (id == pci_read_byte(addr+cap_offset)) {
      return cap_offset;
    } else {
      cap_offset = pci_read_byte(addr+cap_offset+PCI_CAP_OFFSET);
    }
  }

  return 0;
}
  

/**
 * Print pci bars.
 */
void
pci_print_bars(unsigned addr, unsigned count)
{
  unsigned bars[6];
  unsigned masks[6];

  //disable device
  short cmd = pci_read_word_aligned(addr + 0x4);
  pci_write_word_aligned(addr + 0x4, 0);

  // read bars and masks
  for (unsigned i=0; i < count; i++)
    {
      unsigned a = addr + 0x10 + i*4;
      bars[i] = pci_read_long(a);
      pci_write_long(a, ~0);
      masks[i] = ~pci_read_long(a);
      pci_write_long(a, bars[i]);
    }
  // reenable device
  pci_write_word_aligned(addr + 0x4, cmd);


  for (unsigned i=0; i < count; i++)
    {
      unsigned base, high_base = 0;
      unsigned size, high_size = 0;
      char ch;
      if (bars[i] & 0x1)
	{
	  base = bars[i] & 0xfffe;
	  size = (masks[i] & 0xfffe) | 1 | base;
	  ch = 'i';
	}
      else 
	{
	  ch = 'm';
	  base = bars[i] & ~0xf;
	  size = masks[i] | 0xf | base;
	  if ((bars[i] & 0x6) ==  4 && i<5)
	    {
	      high_base = bars[i+1];
	      high_size = masks[i+1] | high_base;
	      i++;
	    }
	}
      if (base)
	printf("    %c: %#x%x/%#x%x", ch, high_base, base, high_size, size);
    }
}


/**
 * Iterate over all devices in the pci config space.
 */
int
pci_iterate_devices()
{
  for (unsigned bus=0; bus < 255; bus++)
    for (unsigned dev=0; dev < 32; dev++)
      {
	unsigned char maxfunc = 0;
	for (unsigned func=0; func<=maxfunc; func++)
	  {
	    unsigned addr = 0x80000000 | bus << 16 | dev << 11 | func << 8;
	    unsigned value= pci_read_long(addr);
	    unsigned class = pci_read_long(addr+0x8) >> 16;

	    unsigned char header_type = pci_read_byte(addr+14);
	    if (!maxfunc && header_type & 0x80)
	      maxfunc=7;
	    if (!value || value==0xffffffff)
	      continue;

	    printf("%x:%x.%x %x: %x:%x %x\n", bus, dev, func, class, value & 0xffff, value >> 16, header_type);
	    //pci_print_bars(addr, header_type & 0x7f ? 2 : 6);
	  }
      }
  return 0;
}

/* EOF */
