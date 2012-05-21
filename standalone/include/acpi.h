/* -*- Mode: C -*- */
/*
 * ACPI definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2009-2012, Bernhard Kauer <kauer@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct rsdp {
  char signature[8];
  uint8_t checksum;
  char oem[6];
  uint8_t rev;
  uint32_t rsdt;
  uint32_t size;
  uint64_t xsdt;
  uint8_t ext_checksum;
  char _res[3];
} __attribute__((packed));

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
  /* XXX Hardcode PCI device scope: path = (device, function) */
  uint8_t path[2];
  //uint8_t path[];
} __attribute__((packed));

enum {
  TYPE_DMAR          = 0,
  TYPE_RMRR          = 1,
  SCOPE_PCI_ENDPOINT = 1,
};

struct dmar_entry {
  uint16_t type;
  uint16_t size;

  union {
    struct {
      uint32_t _res;
      uint64_t phys;
    } dmar;
    /* If we include more than RMRRs here, we need to fix the DMAR
       duplication code in zapp.c */
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
void acpi_fix_checksum(struct acpi_table *tab);

struct rsdp *acpi_get_rsdp(void);
struct acpi_table **acpi_get_table_ptr(struct acpi_table *rsdt, const char signature[4]);

static inline struct dmar_entry *acpi_dmar_next(struct dmar_entry *cur)
{ return (struct dmar_entry *)((char *)cur + cur->size); }

static inline bool acpi_in_table(struct acpi_table *tab, const void *p)
{ return ((uintptr_t)tab + tab->size) > (uintptr_t)p; }

typedef void *(*memory_alloc_t)(size_t len, unsigned align);

struct acpi_table *acpi_dup_table(struct acpi_table *rsdt, const char signature[4],
				  memory_alloc_t alloc);

/* EOF */
