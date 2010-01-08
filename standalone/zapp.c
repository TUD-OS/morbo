/* -*- Mode: C -*- */

#include <stdint.h>

#include <acpi.h>
#include <elf.h>
#include <util.h>
#include <serial.h>
#include <version.h>

struct fixup {
  uint16_t bdf;
  uint64_t base;
  uint64_t size;
} fixups[32];
unsigned fixup_count = 0;

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

    if (strncmp(token, "fixrmrr=", sizeof("fixrmrr=")-1) == 0) {
      char *args_ptr = NULL;
      struct fixup *f = fixups + fixup_count++;
      f->bdf = strtoul(strtok_r(token + sizeof("fixrmrr=")-1, ",", &args_ptr), NULL, 0);
      f->base = strtoul(strtok_r(NULL, ",", &args_ptr), NULL, 0);
      f->size = strtoul(strtok_r(NULL, ",", &args_ptr), NULL, 0);
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

  char *rsdt = acpi_get_rsdp();
  printf("RSDT at %p.\n", rsdt);

  struct dmar *dmar = (struct dmar *)acpi_get_table(rsdt, "DMAR");
  if (!dmar) return -1;
  printf("DMAR at %p (0x%x bytes).\n", dmar, dmar->generic.size);

  for (struct dmar_entry *e = &dmar->first_entry;
       acpi_in_table(&dmar->generic, e);
       e = acpi_dmar_next(e)) {
    switch (e->type) {
    case TYPE_RMRR:
      printf("RMRR %08llx-%08llx %x:%x (%d)\n",
	     e->rmrr.base,
	     e->rmrr.limit+1,
	     e->rmrr.first_scope.path[0],
	     e->rmrr.first_scope.path[1]);

      uint16_t bdf = e->rmrr.first_scope.start_bus<<8 | e->rmrr.first_scope.path[0]<<3 |
	e->rmrr.first_scope.path[1];
      for (unsigned i = 0; i < fixup_count; i++) {
	if (fixups[i].bdf == bdf) {
	  e->rmrr.base = fixups[i].base;
	  e->rmrr.limit = fixups[i].base + fixups[i].size - 1;
	  printf("Fixed!\n");
	}
      }

      break;
    default:
      {}
    }
  }

  dmar->generic.checksum -= acpi_checksum((const char *)dmar, dmar->generic.size);
 
  /* XXX Fix MBI */

  return start_module(mbi);
}

/* EOF */
