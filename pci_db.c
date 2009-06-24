/* -*- Mode: C -*- */

#include <pci_db.h>

#define WILDCARD 0xFFFF

/* This list is searched from top to bottom. Wildcards always
   match. */
static const struct pci_db_entry vendor_db[] = {
  /* Texas Instruments */
  { 0x104c,   0x8023,   NO_QUIRKS, "Texas Instruments IEEE1394a-2000 OHCI PHY/Link-Layer Ctrlr" },
  { 0x104c,   WILDCARD, NO_QUIRKS, "Texas Instruments Unknown Device" },
  /* NEC */
  { 0x1033,   0x00e7,   NO_QUIRKS, "NEC Electronics IEEE1394 OHCI 1.1 2-port PHY-Link Ctrlr" },
  { 0x1033,   WILDCARD, NO_QUIRKS, "NEC Electronics Unknown Device" },
  /* Unknown (don't remove this item) */
  { WILDCARD, WILDCARD, NO_QUIRKS, "Unknown Device" }
};

const struct pci_db_entry *
pci_lookup_device(uint16_t vendor_id, uint16_t device_id)
{
  const struct pci_db_entry *db = vendor_db;

  /* And they say that Lisp has a lot of parens... */
  while (!(((db->vendor_id == WILDCARD) || (db->vendor_id == vendor_id)) &&
	   ((db->device_id == WILDCARD) || (db->device_id == device_id)))) {
    db++;
  }

  return db;
}


/* EOF */
