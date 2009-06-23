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

#include <stddef.h>
#include <stdbool.h>

#include <pci.h>
#include <mbi.h>
#include <util.h>
#include <serial.h>
#include <version.h>
#include <ohci.h>
#include <cpuid.h>
#include <elf.h>

/* TODO: Select OHCI if there is more than one. */


/* Configuration */
static bool be_verbose = true;
static bool dont_enable_apic = false;
static bool keep_going = false;


void
parse_cmdline(const char *cmdline)
{
  const size_t max_cmdline = 512;
  char cmdline_buf[max_cmdline];
  char *token;
  unsigned i;

  strncpy(cmdline_buf, cmdline, max_cmdline);
  
  for (token = strtok(cmdline_buf, " "), i = 0;
       token != NULL;
       token = strtok(NULL, " "), i++) {

    /* Our name is not interesting. */
    if (i == 0)
      continue;

    if (strcmp(token, "noapic") == 0) {
      printf("Disabling APIC magic.\n");
    } else if (strcmp(token, "quiet") == 0) {
      be_verbose = false;
    } else if (strcmp(token, "keepgoing") == 0) {
      printf("Errors will be ignored. You obviously like playing risky.\n");
      keep_going = true;
    } else {
      printf("Ingoring unrecognized argument: %s.\n", token);
    }
  }
}

int
main(const struct mbi *mbi)
{
  serial_init();

  out_char('\n');
  out_info(version_str);

  /* Command line parsing */
  parse_cmdline((const char *)mbi->cmdline);

  /* Check for APIC support */
  if (!dont_enable_apic && !has_apic()) {

    printf("Your CPU reports to have no APIC. Trying to enable it.\n");
    printf("Use the noapic parameter to disable this.\n");

    enable_apic();

    if (!has_apic()) {
      printf("Could not enable it. No APIC for you.\n");
      return 1;
    }

    printf("Yeah, I did it. The APIC is enabled. :-)");
  }

  out_info("Trying to find an OHCI controller...");
  uint32_t ohci_addr;
  
  /* Try to find OHCI Controller */
  ohci_addr = pci_find_device_by_class(PCI_CLASS_SERIAL_BUS_CTRL,
                                       PCI_SUBCLASS_IEEE_1394);

  if (ohci_addr != 0) {
    volatile uint32_t *dev = (uint32_t *) pci_read_long(ohci_addr+PCI_CFG_BAR0);

    printf("OHCI is at 0x%x in PCI config space.\n", ohci_addr);

    if (dev == NULL) {
      printf("ohci1394 registers invalid.\n");
      goto error;
    }

    /* Check version of OHCI controller. */
    uint32_t ohci_version_reg = *dev;
    bool     guid_rom      = (ohci_version_reg >> 24) == 1;
    uint8_t  ohci_version  = (ohci_version_reg >> 16) & 0xFF;
    uint8_t  ohci_revision = ohci_version_reg & 0xFF;

    printf("GUID = %s, version = %x, revision = %x\n",
	   guid_rom ? "yes" : "no?!", ohci_version, ohci_revision);
    
    if ((ohci_version <= 1) && (ohci_revision < 10)) {
      printf("Controller implements OHCI %d.%d. But we require at least 1.10.\n",
	     ohci_version, ohci_revision);
      goto error;
    }

    if (!ohci_initialize(dev)) {
      printf("Controller could not be initialized.\n");
      goto error;
    }

    printf("Initialization complete.\n");
  } else {
    out_info("No OHCI controller found.");
    goto error;
  }

  goto no_error;
 error:
  if (!keep_going) {
    printf("An error occured. Bailing out. Use keepgoing to ignore this.\n");
    __exit(1);
  } else {
    printf("An error occured. But we continue anyway.\n");
  }
 no_error:
  
  /* Will not return */
  start_module(mbi);
  
  /* Not reached */
  return 0;
}
