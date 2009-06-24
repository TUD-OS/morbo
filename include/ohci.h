/* -*- Mode: C -*- */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pci.h>

struct ohci_controller {

  const struct pci_device *pci;	/* PCI device info. */

  volatile uint32_t *ohci_regs;	/* Pointer to memory-mapped OHCI
				   registers. */

  uint8_t total_ports;
  bool enhanced_phy_map;
};

bool    ohci_initialize(const struct pci_device *pci_dev,
			struct ohci_controller *ohci);

uint8_t ohci_wait_nodeid(struct ohci_controller *ohci);
void    ohci_force_bus_reset(struct ohci_controller *ohci);


/* EOF */
