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

/* Globals */
struct mbi *multiboot_info = 0;

/* Configuration (set by command line parser) */
static bool be_verbose = true;
static bool force_enable_apic = true;
static bool keep_going = false;
static bool do_wait = false;
static bool posted_writes = false;

void
parse_cmdline(const char *cmdline)
{
  char *last_ptr = NULL;
  char cmdline_buf[256];
  char *token;
  unsigned i;

  strncpy(cmdline_buf, cmdline, sizeof(cmdline_buf));

  for (token = strtok_r(cmdline_buf, " ", &last_ptr), i = 0;
       token != NULL;
       token = strtok_r(NULL, " ", &last_ptr), i++) {

    /* Our name is not interesting. */
    if (i == 0)
      continue;

    if (strcmp(token, "noapic") == 0) {
      force_enable_apic = false;
    } else if (strcmp(token, "quiet") == 0) {
      be_verbose = false;
    } else if (strcmp(token, "keepgoing") == 0) {
      keep_going = true;
    } else if (strcmp(token, "postedwrites") == 0) {
      posted_writes = true;
    } else if (strcmp(token, "wait") == 0) {
      do_wait = true;
    } else if (strncmp(token, "mempatch=", sizeof("mempatch=")-1) == 0) {
      char *args_ptr = NULL;
      char *width = strtok_r(token + sizeof("mempatch=")-1, ",", &args_ptr);
      uintptr_t addr  = (uintptr_t)strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);
      uint32_t value = strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);

      switch (width[0]) {
      case 'b': *((uint8_t  *)addr) = (uint8_t)value; break;
      case 'w': *((uint16_t *)addr) = (uint16_t)value; break;
      case 'd': *((uint32_t *)addr) = (uint32_t)value; break;
      }
    } else {
      /* printf not possible yet. */
      //printf("Ignoring unrecognized argument: %s.\n", token);
    }
  }
}

int
main(uint32_t magic, struct mbi *mbi)
{
  /* Command line parsing */
  if (magic == MBI_MAGIC) {
    multiboot_info = mbi;
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);
  } else {
    serial_init();
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  serial_init();
  printf("\nMorbo %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  hexdump((const void *)0, 0x20);

  /* Check for APIC support */
  if (force_enable_apic && !has_apic()) {

    printf("Your CPU reports to have no APIC. Trying to enable it.\n");
    printf("Use the noapic parameter to disable this.\n");

    enable_apic();

    if (!has_apic()) {
      printf("Could not enable it. No APIC for you.\n");
      return 1;
    }

    printf("Yeah, I did it. The APIC is enabled. :-)");
  }

  if (posted_writes)
    printf("Posted writes will be enabled. Disable them, if you experience problems.\n");

  printf("Trying to find an OHCI controller... ");

  struct pci_device pci_ohci;
  struct ohci_controller ohci;

  if (!pci_find_device_by_class(PCI_CLASS_SERIAL_BUS_CTRL, PCI_SUBCLASS_IEEE_1394, &pci_ohci)) {
    printf("No OHCI found.\n");
    goto error;
  } else {
    printf("OK\n");
  }

  if (!ohci_initialize(&pci_ohci, &ohci, posted_writes)) {
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

  if ((mbi->mods_count == 0) || do_wait) {
    printf("Polling for events until we are kicked in the nuts.\n");
    volatile uint32_t *modules = &mbi->mods_count;
    /* Indicate that we are ready to be booted by setting
       mbi->mods_count to zero. This breaks if we load only one
       module... */
    *modules = 0;
    while (*modules == 0) {
      ohci_poll_events(&ohci);
    }
  }

  /* Will not return if successful. */
  return start_module(mbi);
}
