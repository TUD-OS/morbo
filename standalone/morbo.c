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
static bool multiboot_loader = false;

volatile const struct mbi *multiboot_info = 0;

/* Configuration (set by command line parser) */
static bool be_verbose = true;
static bool force_enable_apic = true;
static bool keep_going = false;
static bool do_wait = false;
static bool posted_writes = false;

void
parse_cmdline(const char *cmdline)
{
  char cmdline_buf[256];
  char *token;
  unsigned i;

  strncpy(cmdline_buf, cmdline, sizeof(cmdline_buf));

  for (token = strtok(cmdline_buf, " "), i = 0;
       token != NULL;
       token = strtok(NULL, " "), i++) {

    /* Our name is not interesting. */
    if (i == 0)
      continue;

    if (strcmp(token, "noapic") == 0) {
      printf("Disabling APIC magic.\n");
      force_enable_apic = false;
    } else if (strcmp(token, "quiet") == 0) {
      be_verbose = false;
    } else if (strcmp(token, "keepgoing") == 0) {
      printf("Errors will be ignored. You obviously like playing risky.\n");
      keep_going = true;
    } else if (strcmp(token, "postedwrites") == 0) {
      printf("Posted writes will be enabled. Disable them, if you experience problems.\n");
      posted_writes = true;
    } else if (strcmp(token, "wait") == 0) {
      do_wait = true;
    } else {
      printf("Ingoring unrecognized argument: %s.\n", token);
    }
  }
}

int
main(uint32_t magic, struct mbi *mbi)
{
  multiboot_loader = (magic == MBI_MAGIC);

  serial_init();
  printf("\nMorbo %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  /* Command line parsing */
  if (multiboot_loader) {
    multiboot_info = mbi;
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);

    /* Check modules structure. */
    if ((mbi->flags & MBI_FLAG_MODS) != 0) {
      struct module *mod = (struct module *)mbi->mods_addr;
      printf("%d modules.\n", mbi->mods_count);
      for (unsigned i = 0; i < mbi->mods_count; i++, mod++) {
    	printf("mod[%d]: '%s' 0x%x-0x%x\n", i, mod->string, mod->mod_start, mod->mod_end);
      }
    }
  } else {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

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

  if (do_wait) {
    printf("Polling for events until we are kicked in the nuts.\n");
    volatile uint32_t *modules = &mbi->mods_count;
    *modules = 0;
    while (*modules == 0) {
      ohci_poll_events(&ohci);
    }
  }

  /* Will not return if successful. */
  return start_module(mbi);
}
