/* -*- Mode: C -*- */
/*
 * OHCI definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pci.h>

#include <ohci-crm.h>

struct ohci_controller {

  const struct pci_device *pci;	/* PCI device info. */

  volatile uint32_t *ohci_regs;	/* Pointer to memory-mapped OHCI
				   registers. */
  ohci_config_rom_t *crom;
  uint32_t *selfid_buf;

  uint8_t total_ports;
  bool enhanced_phy_map;
  bool posted_writes;

};

enum link_speed {
  SPEED_S100 = 0U,
  SPEED_S200 = 1U,
  SPEED_S400 = 2U,

  SPEED_MAX  = ~0U,
};

void    ohci_poll_events(struct ohci_controller *ohci);
bool    ohci_initialize(const struct pci_device *pci_dev,
			struct ohci_controller *ohci,
			bool posted_writes,
			enum link_speed speed);

uint8_t ohci_wait_nodeid(struct ohci_controller *ohci);
void    ohci_force_bus_reset(struct ohci_controller *ohci);


/* EOF */
