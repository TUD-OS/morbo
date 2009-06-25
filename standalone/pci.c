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
static unsigned char
pci_read_byte(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inb(PCI_DATA_PORT + (addr & 3));
}

/**
 * Read a word from the pci config space.
 */
static unsigned short
pci_read_word(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT + (addr & 2));
}


/**
 * Read a long from the pci config space.
 */
static unsigned
pci_read_long(unsigned addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inl(PCI_DATA_PORT);
}


/**
 * Write a word to the pci config space.
 */
static void
pci_write_word(unsigned addr, unsigned short value)
{
  outl(PCI_ADDR_PORT, addr);
  outw(PCI_DATA_PORT + (addr & 2), value);
}


/**
 * Write a long to the pci config space.
 */
static void
pci_write_long(unsigned addr, unsigned value)
{
  outl(PCI_ADDR_PORT, addr);
  outl(PCI_DATA_PORT, value);
}


/* Fillout pci_device structure. */
static void
populate_device_info(uint32_t cfg_address, struct pci_device *dev)
{
  uint32_t vendor_id = pci_read_long(cfg_address + PCI_CFG_VENDOR_ID);
  uint32_t device_id = vendor_id >> 16;
  vendor_id &= 0xFFFF;
  
  dev->db = pci_lookup_device(vendor_id, device_id);
  dev->cfg_address = cfg_address;
}


bool
pci_find_device_by_class(uint8_t class, uint8_t subclass,
			 struct pci_device *dev)
{
  uint32_t res = 0;
  uint16_t full_class = class << 8 | subclass;

  assert(dev != NULL, "Invalid dev pointer");

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

  if (res != 0) {
    populate_device_info(res, dev);
    return true;
  } else {
    return false;    
  }
}

uint32_t
pci_cfg_read_uint32(const struct pci_device *dev, uint32_t offset)
{
  return pci_read_long(dev->cfg_address + offset);
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
  

/* EOF */
