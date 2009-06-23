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

/* #define PCI_VERBOSE */

/**
 * Read a byte from the pci config space.
 */
uint8_t
pci_read_byte(uint32_t addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inb(PCI_DATA_PORT + (addr & 3));
}

/**
 * Read a word from the pci config space.
 */
uint16_t
pci_read_word(uint32_t addr)
{
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT + (addr & 2));
}


/**
 * Read a long from the pci config space.
 */
uint32_t
pci_read_long(uint32_t addr)
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
static
void
pci_write_long(unsigned addr, unsigned value)
{
  outl(PCI_ADDR_PORT, addr);
  outl(PCI_DATA_PORT, value);
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
  uint16_t complete_class = class << 8 | subclass;

  uint32_t res = 0;
  for (unsigned i=0; i<1<<16; i++) {
    uint32_t addr = 0x80000000 | i<<8;
    uint16_t dev_class = (pci_read_long(addr + PCI_CFG_REVID) >> 16);
    
    if (dev_class != 0xFFFF) {
#ifdef PCI_VERBOSE
      out_description("found device. class", dev_class);
#endif
      if (complete_class == dev_class)
        res = addr;
    }
  }
  return res;
}

static inline uint32_t
pci_config_address(uint8_t bus, uint8_t device, uint8_t function)
{
  /* Type 1 config transaction */
  return 0x80000000 | (bus << 16) | (device << 11) | (function << 8);
}

static inline bool
pci_is_bridge(uint32_t addr)
{
  return ((pci_read_long(addr | PCI_CFG_REVID) >> 24) == PCI_CLASS_BRIDGE_DEV);
}

static inline bool
pci_is_ieee1934(uint32_t addr)
{
  return ((pci_read_long(addr | PCI_CFG_REVID) >> 16) == 
          (PCI_CLASS_SERIAL_BUS_CTRL << 8 | PCI_SUBCLASS_IEEE_1394));
}

/**
 * Traverses the PCI bus across bridges and prints info.
 */
void
pci_bus_info(uint8_t bus)
{
  out_description("Inspecting PCI bus ", bus);
  for (unsigned device = 0; device < (1<<5); device++) {
    uint32_t addr = pci_config_address(bus, device, 0);

    if (pci_is_ieee1934(addr)) {
      out_info("GOTCHA!");
    }

    if (pci_is_bridge(addr)) {
      uint32_t dev_class_raw = pci_read_long(addr | PCI_CFG_REVID) >> 8;
      uint8_t subclass = (dev_class_raw >> 8) & 0xFF;
      const char *desc;

      switch (subclass) {
      case PCI_SUBCLASS_HOST_BRIDGE: desc = "Host/PCI bridge @"; break;
      case PCI_SUBCLASS_ISA_BRIDGE: desc = "PCI/ISA bridge @"; break;
      case PCI_SUBCLASS_PCI_BRIDGE: desc = "PCI/PCI bridge @"; break;
      case PCI_SUBCLASS_PCMCIA_BRIDGE: desc = "PCI/PCMCIA bridge @"; break;
      case PCI_SUBCLASS_CARDBUS_BRIDGE: desc = "PCI/Cardbus bridge @"; break;
      default: desc = "Unknown bridge @"; break;
      }

      out_description(desc, addr);

      if ((subclass == PCI_SUBCLASS_PCI_BRIDGE) ||
          (subclass == PCI_SUBCLASS_CARDBUS_BRIDGE)) {
        uint32_t bus_info = pci_read_long(addr | PCI_CFG_BAR2);
        uint8_t primary = bus_info & 0xFF;
        uint8_t secondary = (bus_info >> 8) & 0xFF;
        uint8_t subordinate = (bus_info >> 16) & 0xFF;

        out_description("Bus info: ", bus_info);
        out_info("Recursing...");
        pci_bus_info(secondary);
        out_info("... Done recursing");
      }
    }
  }
}

/* EOF */
