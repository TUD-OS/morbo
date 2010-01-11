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
  while (count--) res += *(table++);
  return res;
}

void acpi_fix_checksum(struct acpi_table *tab)
{
  tab->checksum -= acpi_checksum((const char *)tab, tab->size);
}

/**
 * Return the rsdp.
 */
struct rsdp *acpi_get_rsdp(void)
{
  __label__ done;
  struct rsdp *ret = 0;

  void check(char *p) {
    if ((memcmp(p, "RSD PTR ", 8) == 0) &&
	(acpi_checksum(p, 20) == 0)) {
      //ret = (char *)(((uint32_t *)p)[4]);
      ret = (struct rsdp *)p;
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
  return ret;
}

struct acpi_table **acpi_get_table_ptr(struct acpi_table *rsdt, const char signature[4])
{
  for (const char *cur = (const char*)rsdt + sizeof(struct acpi_table);
       acpi_in_table(rsdt, cur);
       cur += sizeof(uintptr_t)) {
    struct acpi_table *entry = *(struct acpi_table **)cur;
    if (acpi_checksum((const char *)entry, entry->size) != 0)
      continue;
    if (memcmp(signature, entry->signature, sizeof(signature)) == 0)
      return (struct acpi_table **)cur;
  }

  return 0;
}

/** Duplicate an ACPI table. */
struct acpi_table *acpi_dup_table(struct acpi_table *rsdt, const char signature[4],
				  memory_alloc_t alloc)
{
  struct acpi_table **tab   = acpi_get_table_ptr(rsdt, signature);
  struct acpi_table *newtab = alloc((*tab)->size, 0x1000); /* 4K aligned */
  memcpy(newtab, *tab, (*tab)->size);
  *tab = newtab;
  acpi_fix_checksum(rsdt);
  return newtab;
}

/* EOF */
