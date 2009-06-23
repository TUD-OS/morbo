/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>

typedef volatile uint32_t *ohci_dev_t;

bool    ohci_initialize(ohci_dev_t dev);
uint8_t ohci_wait_nodeid(ohci_dev_t dev);
void    ohci_force_bus_reset(ohci_dev_t dev);


/* EOF */
