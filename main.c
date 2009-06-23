/*
 * \brief   fulda - use ieee1394 for debugging.
 * \date    2007-04-19
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2007  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the FULDA package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#include <pci.h>
#include <mbi.h>
#include <util.h>
#include <serial.h>
#include <version.h>
#include <ohci.h>
#include <cpuid.h>
#include <elf.h>

int
main(const struct mbi *mbi)
{
  serial_init();

  out_char('\n');
  out_info(version_str);

  /* Check for APIC support */
  if (!has_apic()) {
    out_info("Your CPU reports to have no APIC. Trying to enable it...");
    enable_apic();
    if (!has_apic()) {
      out_info("Enabling it failed. You're out of luck...");
      return 1;
    }
    out_info("Yeah, I did it. :)");
  }


  out_info("Trying to find an OHCI controller...");
  uint32_t ohci_addr;
  uint32_t cardbus_addr;
  
  /* Try to find OHCI Controller */
  ohci_addr = pci_find_device_by_class(PCI_CLASS_SERIAL_BUS_CTRL,
                                       PCI_SUBCLASS_IEEE_1394);

  if (ohci_addr != 0) {
    out_info("There it is.");
    uint32_t *dev = (uint32_t *) pci_read_long(ohci_addr+PCI_CFG_BAR0);
    CHECK3(11, !dev, "ohci1394 registers invalid");
    
    CHECK3(12, ((*dev) & 0xff00ff) == 0x10001, "invalid version");
    out_description("device", ohci_addr);
    CHECK3(13, !ohci_initialize(dev), "initialize failed");
    out_info("OHCI initialized.");
  } else {
    out_info("No OHCI controller found.");
    out_info("I just continue anyway.");
  }
  
  /* Will not return */
  start_module(mbi);
  
  /* Not reached */
  return 0;
}
