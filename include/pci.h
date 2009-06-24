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

enum pci_class {
  PCI_CLASS_BRIDGE_DEV      = 0x06,
  PCI_CLASS_SERIAL_BUS_CTRL = 0x0C,
};

enum pci_subclass {
  PCI_SUBCLASS_IEEE_1394    = 0x00,
  PCI_SUBCLASS_HOST_BRIDGE  = 0x00,
  PCI_SUBCLASS_ISA_BRIDGE   = 0x01,
  PCI_SUBCLASS_PCI_BRIDGE   = 0x04,
  PCI_SUBCLASS_PCMCIA_BRIDGE  = 0x05,
  PCI_SUBCLASS_CARDBUS_BRIDGE = 0x07,
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
};

uint8_t  pci_read_byte(uint32_t addr);
uint16_t pci_read_word(uint32_t addr);
uint32_t pci_read_long(uint32_t addr);

uint32_t pci_find_device_by_class(uint8_t class, uint8_t subclass);
uint32_t pci_find_device_by_id(uint16_t id);
/* EOF */
