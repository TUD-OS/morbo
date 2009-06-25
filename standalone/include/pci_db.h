/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>

struct pci_db_entry {
  uint16_t vendor_id;
  uint16_t device_id;
  uint32_t quirks;
  const char *device_name;
};

enum controller_quirks {
  NO_QUIRKS = 0,
  /* ... */
};

const struct pci_db_entry * pci_lookup_device(uint16_t vendor_id, uint16_t device_id);

/* EOF */
