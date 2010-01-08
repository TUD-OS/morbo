/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct acpi_table {
  char signature[4];
  uint32_t size;
  uint8_t  rev;
  uint8_t  checksum;
  char oemid[6];
  char oemtabid[8];
  uint32_t oemrev;
  char creator[4];
  uint32_t crev;
} __attribute__((packed));

struct device_scope {
  uint8_t type;
  uint8_t size;
  uint16_t _res;
  uint8_t enum_id;
  uint8_t start_bus;
  uint8_t path[];
} __attribute__((packed));

enum {
  TYPE_RMRR = 1,
};

struct dmar_entry {
  uint16_t type;
  uint16_t size;
  
  union {
    struct rmrr {
      uint16_t _res;
      uint16_t segment;
      uint64_t base;
      uint64_t limit;
      struct device_scope first_scope;
    } rmrr;
  };
} __attribute__((packed));

struct dmar {
  struct acpi_table generic;
  uint8_t host_addr_width;
  uint8_t flags;
  char _res[10];
  struct dmar_entry first_entry;
};

char acpi_checksum(const char *table, size_t count);
char *acpi_get_rsdp(void);
struct acpi_table *acpi_get_table(char *rsdt_raw, const char signature[4]);

static inline struct dmar_entry *acpi_dmar_next(struct dmar_entry *cur)
{ return (struct dmar_entry *)((char *)cur + cur->size); }

static inline bool acpi_in_table(struct acpi_table *tab, void *p)
{ return ((uintptr_t)tab + tab->size) > (uintptr_t)p; }

/* EOF */
