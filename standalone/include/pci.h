/*
 * \brief header used for DEV protection
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

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pci_db.h>

enum pci_class {
  PCI_CLASS_BRIDGE_DEV      = 0x06,
  PCI_CLASS_SIMPLE_COMM     = 0x07,
  PCI_CLASS_SERIAL_BUS_CTRL = 0x0C,
};

enum pci_subclass {
  PCI_SUBCLASS_IEEE_1394    = 0x00,
  PCI_SUBCLASS_HOST_BRIDGE  = 0x00,
  PCI_SUBCLASS_ISA_BRIDGE   = 0x01,
  PCI_SUBCLASS_PCI_BRIDGE   = 0x04,
  PCI_SUBCLASS_PCMCIA_BRIDGE  = 0x05,
  PCI_SUBCLASS_CARDBUS_BRIDGE = 0x07,
  PCI_SUBCLASS_SERIAL_CTRL  = 0x00,
  PCI_SUBCLASS_SERIAL_OTHER = 0x80,

  PCI_SUBCLASS_ANY          = 0xFF,
};

enum pci_config_space {
  PCI_CFG_VENDOR_ID = 0x0,
  PCI_CFG_REVID = 0x08,         /* Read uint32 to get class code in upper 16bit */
  PCI_CFG_BAR0  = 0x10,
  PCI_CFG_BAR1  = 0x14,
  PCI_CFG_BAR2  = 0x18,
  PCI_CFG_BAR3  = 0x1C,
};

enum pci_constants {
  PCI_ADDR_PORT = 0xcf8,
  PCI_DATA_PORT = 0xcfc,
  PCI_CONF_HDR_CMD = 4,
  PCI_CONF_HDR_CAP = 52,
  PCI_CAP_OFFSET = 1,
  PCI_CAP_ID_EXP   = 0x10,
  PCI_EXP_TYPE_PCI_BRIDGE=0x7,
  PCI_BAR_TYPE_MASK = 1U,
  PCI_BAR_TYPE_IO   = 1U,
  PCI_BAR_IO_MASK   = ~1U,
};

struct pci_device {
  const struct pci_db_entry *db;
  uint32_t cfg_address;		/* Address of config space */
};

/* Low-Level PCI Access */
uint8_t pci_read_uint8(unsigned addr);
uint32_t pci_read_uint32(unsigned addr);


uint32_t pci_cfg_read_uint32(const struct pci_device *dev, uint32_t offset);

/* Find a device by its class. Always finds the last device of the
   given class. On success, returns true and fills out the given
   pci_device structure. If subclass is 0xFF, it will be
   ignored. Otherwise, returns false. */
bool pci_find_device_by_class(uint8_t class, uint8_t subclass,
			      struct pci_device *dev);

unsigned char pci_find_cap(unsigned addr, unsigned char id);

/* EOF */
