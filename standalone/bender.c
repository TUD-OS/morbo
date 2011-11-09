/* -*- Mode: C -*- */

#include <pci.h>
#include <mbi.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>

/* Configuration (set by command line parser) */
static bool be_promisc = false;

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

    if (strcmp(token, "promisc") == 0) {
      be_promisc = true;
    }
  }
}

int
main(uint32_t magic, struct mbi *mbi)
{
  if (magic == MBI_MAGIC) {
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);
  } else {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nBender %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("Looking for serial controllers on the PCI bus...\n");

  struct pci_device serial_ctrl;

  printf("Promisc is %s.\n", be_promisc ? "on" : "off");
  if (pci_find_device_by_class(PCI_CLASS_SIMPLE_COMM,
			       be_promisc ? PCI_SUBCLASS_ANY : PCI_SUBCLASS_SERIAL_CTRL,
			       &serial_ctrl)) {
    printf("  found at %x.\n", serial_ctrl.cfg_address);
  } else {
    printf("  none found.\n");
    goto boot_next;
  }

  uint16_t iobase = 0;
  for (unsigned bar_no = 0; bar_no < 6; bar_no++) {
    uint32_t bar = pci_cfg_read_uint32(&serial_ctrl, PCI_CFG_BAR0 + 4*bar_no);
    if ((bar & PCI_BAR_TYPE_MASK) == PCI_BAR_TYPE_IO) {
      iobase = bar & PCI_BAR_IO_MASK;
      break;
    }
  }

  if (iobase != 0) {
    printf("Patching BDA with I/O port 0x%x.\n", iobase);
    *(uint16_t *)(0x400) = iobase;
    serial_init();
    printf("Hello World.\n");
  } else {
    printf("I/O ports for controller not found.\n");
  }

 boot_next:
  return start_module(mbi, false);
}
