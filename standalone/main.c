/* -*- Mode: C -*- */

#include <stddef.h>
#include <stdbool.h>

#include <pci.h>
#include <pci_db.h>
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
static bool do_wait = false;

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
    } else if (strcmp(token, "wait") == 0) {
      do_wait = true;
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

  printf("Trying to find an OHCI controller... ");

  struct pci_device pci_ohci;
  struct ohci_controller ohci;

  if (!pci_find_device_by_class(PCI_CLASS_SERIAL_BUS_CTRL, PCI_SUBCLASS_IEEE_1394, &pci_ohci)) {
    printf("No OHCI found.\n");
    goto error;
  } else {
    printf("OK\n");
  }

  if (!ohci_initialize(&pci_ohci, &ohci)) {
    printf("Could not initialize controller.\n");
    goto error;
  } else {
    printf("Initialization complete.\n");
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

  if (do_wait) {
    printf("Waiting... (XXX No return from here... XXX)\n");
    ohci_poll_events(&ohci);
    /* XXX ohci_poll_events does not return yet. */
  }
  
  /* Will not return */
  start_module(mbi);
  
  /* Not reached */
  return 0;
}
