/* -*- Mode: C -*- */

#include <stddef.h>
#include <acpi.h>
#include <util.h>

/* ACPI code inspired by Vancouver. */

/**
 * Calculate the ACPI checksum of a table.
 */
char acpi_checksum(const char *table, size_t count)
{
  char res = 0;
  while (count--)
    res += table[count];
  return res;
}

/**
 * Return the rsdp.
 */
char *acpi_get_rsdp(void)
{
  __label__ done;
  char *ret = 0;

  void check(char *p) {
    if ((memcmp(p, "RSD PTR ", 8) == 0) && 
	(acpi_checksum(p, 20) == 0)) {
      ret = (char *)(((uint32_t *)p)[4]);
      goto done;
    }
  }

  void find(uintptr_t start, size_t len) {
    for (uintptr_t cur = start; cur < start+len; cur += 16)
      check((char *)cur);
  }

  find(0x40e, 0x400);		/* BDA */
  find(0xe0000, 0x20000);	/* BIOS read-only memory */

 done:
  if (ret) {
    printf("RSDT 0x%x bytes.\n", ((struct acpi_table *)ret)->size);
  }

  return ret;
}

struct acpi_table *acpi_get_table(char *rsdt_raw, const char signature[4])
{
  struct acpi_table *rsdt = (struct acpi_table *)rsdt_raw;
  void *end = rsdt_raw + rsdt->size;

  for (char *cur = rsdt_raw + sizeof(struct acpi_table);
       cur < rsdt_raw + rsdt->size;
       cur += sizeof(uintptr_t)) {
    struct acpi_table *entry = *(struct acpi_table **)cur;
    if (acpi_checksum((const char *)entry, entry->size) != 0)
      continue;
    if (memcmp(signature, entry->signature, sizeof(signature)) == 0)
      return entry;
  }

  return 0;
}



/* EOF */
