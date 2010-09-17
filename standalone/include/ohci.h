/* -*- Mode: C -*- */

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

void    ohci_poll_events(struct ohci_controller *ohci);
bool    ohci_initialize(const struct pci_device *pci_dev,
			struct ohci_controller *ohci,
			bool posted_writes,
			unsigned speed);

uint8_t ohci_wait_nodeid(struct ohci_controller *ohci);
void    ohci_force_bus_reset(struct ohci_controller *ohci);


/* EOF */
