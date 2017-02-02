/* -*- Mode: C -*- */

#include <pci.h>
#include <mbi.h>
#include <mbi2.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <bda.h>

/* Configuration (set by command line parser) */
static bool be_promisc = false;
static uint64_t phys_max_relocate = 1ULL << 31; /* below 2G */

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
    if (strcmp(token, "phys_max=256M") == 0)
      phys_max_relocate = 256ULL * 1024 * 1024;
  }
}

static uint16_t apply_quirks(struct pci_device *pcid, uint16_t raw_iobase)
{
    uint32_t vendormodel = pci_cfg_read_uint32(pcid, 0);

    if (vendormodel == 0x32531c00) {
        printf("Found XR16850 chip. Adding 0xc0 to iobase offset.\n");
        return raw_iobase + 0xc0;
    }

    return raw_iobase;
}

int
main(uint32_t magic, void *multiboot)
{
  if (magic == MBI_MAGIC) {
    struct mbi * mbi = (struct mbi *)multiboot;
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);
  } else
  if (magic == MBI2_MAGIC) {
    for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
      if (i->type != MBI2_TAG_CMDLINE)
        continue;

      parse_cmdline((const char *)(i + 1));
      break;
    }
  } else {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nBender %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("Looking for serial controllers on the PCI bus...\n");

  struct pci_device serial_ctrl = { 0, 0 };

  printf("Promisc is %s.\n", be_promisc ? "on" : "off");
  if (pci_find_device_by_class(PCI_CLASS_SIMPLE_COMM,
			       be_promisc ? PCI_SUBCLASS_ANY : PCI_SUBCLASS_SERIAL_CTRL,
			       &serial_ctrl)) {
    printf("  found at %x.\n", serial_ctrl.cfg_address);
  } else {
    printf("  none found.\n");
  }

  uint16_t iobase          = 0;
  uint16_t *com0_port      = (uint16_t *)(get_bios_data_area());
  uint16_t *equipment_word = &get_bios_data_area()->equipment;

  if (serial_ctrl.cfg_address) {
    for (unsigned bar_no = 0; bar_no < 6; bar_no++) {
      uint32_t bar = pci_cfg_read_uint32(&serial_ctrl, PCI_CFG_BAR0 + 4*bar_no);
      if ((bar & PCI_BAR_TYPE_MASK) == PCI_BAR_TYPE_IO) {
        iobase = bar & PCI_BAR_IO_MASK;
        break;
      }
    }

    iobase = apply_quirks(&serial_ctrl, iobase);

    if (iobase != 0) {
      printf("Patching BDA with I/O port 0x%x.\n", iobase);
      *com0_port      = iobase;
      *equipment_word = (*equipment_word & ~(0xF << 9)) | (1 << 9); /* One COM port available */
    } else {
      printf("I/O ports for controller not found.\n");
    }
  }

  if (serial_ports(get_bios_data_area()))
    serial_init();

  printf("Bender: Hello World.\n");

  if (magic == MBI_MAGIC)
      return start_module((struct mbi *)multiboot, false, phys_max_relocate);
  else
  if (magic == MBI2_MAGIC)
      return start_module2(multiboot, false, phys_max_relocate);
  else
      return 1;
}
