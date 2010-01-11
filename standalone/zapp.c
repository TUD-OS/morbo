/* -*- Mode: C -*- */

#include <stdint.h>

#include <acpi.h>
#include <elf.h>
#include <mbi-tools.h>
#include <pci.h>
#include <util.h>
#include <serial.h>
#include <version.h>

#define MAX_FIXUPS 32

struct fixup {
  uint16_t bdf;
  uint64_t base;
  uint64_t size;
} fixups[MAX_FIXUPS];
unsigned fixup_count = 0;

struct add {
  uint16_t class;
  uint64_t base;
  uint64_t size;
} additions[MAX_FIXUPS];
unsigned additions_count = 0;

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
    char *args_ptr = NULL;

    /* Our name is not interesting. */
    if (i == 0)
      continue;

    if (strncmp(token, "fixrmrr=", sizeof("fixrmrr=")-1) == 0) {
      assert(fixup_count < MAX_FIXUPS, "Too many fixups.");
      struct fixup *f = fixups + fixup_count++;
      f->bdf = strtoull(strtok_r(token + sizeof("fixrmrr=")-1, ",", &args_ptr), NULL, 0);
      f->base = strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);
      f->size = strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);
    } else if (strncmp(token, "addrmrr=", sizeof("addrmrr=")-1) == 0) {
      assert(additions_count < MAX_FIXUPS, "Too many additional RMRRs.\n");
      struct add *a = additions + additions_count++;
      a->class = strtoull(strtok_r(token + sizeof("addrmrr=")-1, ",", &args_ptr), NULL, 0);
      a->base = strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);
      a->size = strtoull(strtok_r(NULL, ",", &args_ptr), NULL, 0);
    } else {
      printf("Ignoring unrecognized argument: %s.\n", token);
    }
  }
}

int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();
  printf("\nZapp %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  /* Command line parsing */
  if (magic == MBI_MAGIC) {
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);
  } else {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  struct rsdp *rsdp = acpi_get_rsdp();
  struct acpi_table *rsdt = (struct acpi_table *)(rsdp->rsdt);
  printf("RSDT at %p.\n", rsdt);

  struct dmar *dmar = *(struct dmar **)acpi_get_table_ptr(rsdt, "DMAR");
  if (!dmar) return -1;
  printf("DMAR at %p (0x%x bytes).\n", dmar, dmar->generic.size);

  for (struct dmar_entry *e = &dmar->first_entry;
       acpi_in_table(&dmar->generic, e);
       e = acpi_dmar_next(e)) {
    switch (e->type) {
    case TYPE_RMRR:
      printf("RMRR %08llx-%08llx %x:%x.%x (type %x)",
	     e->rmrr.base,
	     e->rmrr.limit+1,
	     e->rmrr.first_scope.start_bus,
	     e->rmrr.first_scope.path[0],
	     e->rmrr.first_scope.path[1],
	     e->rmrr.first_scope.type);

      uint16_t bdf = e->rmrr.first_scope.start_bus<<8 | e->rmrr.first_scope.path[0]<<3 |
	e->rmrr.first_scope.path[1];
      for (unsigned i = 0; i < fixup_count; i++) {
	if (fixups[i].bdf == bdf) {
	  e->rmrr.base = fixups[i].base;
	  e->rmrr.limit = fixups[i].base + fixups[i].size - 1;
	  printf(" FIXED");
	  break;
	}
      }
      printf("\n");

      break;
    default:
      {}
    }
  }
  acpi_fix_checksum(&dmar->generic);

  /* Do we need to add new entries to the DMAR table? */
  if (additions_count > 0) {
    void *alloc(size_t len, unsigned align) {
      /* Leave space for new entries. */
      return mbi_alloc_protected_memory(mbi, len + additions_count*sizeof(struct dmar_entry),
					align);
    }

    struct dmar *newdmar = (struct dmar *)acpi_dup_table(rsdt, "DMAR", alloc);
    printf("New DMAR at %p.\n", newdmar);

    for (unsigned i = 0; i < additions_count; i++) {
      struct dmar_entry *e = (struct dmar_entry *)((char *)newdmar + newdmar->generic.size
						   + i*sizeof(struct dmar_entry));
      memset(e, 0, sizeof(struct dmar_entry));
      e->type = TYPE_RMRR;
      e->size = sizeof(struct dmar_entry);
      e->rmrr.segment = 0;
      e->rmrr.base = additions[i].base;
      e->rmrr.limit = additions[i].base + additions[i].size - 1;
      e->rmrr.first_scope.type = SCOPE_PCI_ENDPOINT;
      e->rmrr.first_scope.size = sizeof(struct device_scope);
      e->rmrr.first_scope.enum_id = 0;

      struct pci_device dev;
      uint16_t class = additions[i].class;
      bool succ = pci_find_device_by_class(class >> 8, class & 0xFF, &dev);
      assert(succ, "Device with class %02x not found.", class);
      e->rmrr.first_scope.start_bus = dev.cfg_address >> 16;
      e->rmrr.first_scope.path[0]  = (dev.cfg_address >> 11) & 0x1F;
      e->rmrr.first_scope.path[1]  = (dev.cfg_address >>  8) & 0x07;

      printf("Added new RMRR for device %x:%x.%x: %s\n",
	     e->rmrr.first_scope.start_bus,
	     e->rmrr.first_scope.path[0],
	     e->rmrr.first_scope.path[1],
	     dev.db->device_name);
    }

    newdmar->generic.size += additions_count*sizeof(struct dmar_entry);
    acpi_fix_checksum(&newdmar->generic);


    if (rsdp->rev >= 2) {
      /* XXX Crude... */
      struct acpi_table *xsdt = (struct acpi_table *)(rsdp->xsdt);

      for (uint64_t *cur = (uint64_t*)(xsdt+1);
	   acpi_in_table(xsdt, cur);
	   cur += 1) {
	if ((*cur >> 32) != 0) {
	  printf("Table above 4GB. Skipping.\n");
	  continue;
	}

	struct acpi_table *tab = (struct acpi_table *)((uint32_t)*cur);
	if (memcmp(tab->signature, "DMAR", 4) == 0) {
	  printf("Fixing DMAR pointer in XSDT.\n");
	  *cur = (uint32_t)newdmar;
	}
      }
      acpi_fix_checksum(xsdt);
    }
  }

  printf("Starting next module.\n");
  return start_module(mbi);
}

/* EOF */
