/* -*- Mode: C -*- */
/*
 * Multiboot definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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
#include <stdbool.h>
#include <mbi.h>
#include <elf.h>

void exclude_region(uint64_t *block_addr, uint64_t *block_len,
                    uintptr_t const image_start, uintptr_t const image_end);
void exclude_bender_binary(uint64_t *block_addr, uint64_t *block_len);
bool overlap_bender_binary(struct ph64 const * p);

static inline bool overlap(uint64_t mod_start, uint64_t mod_end,
                           struct ph64 const * p)
{
  if (p->p_memsz == 0)
    return false;

  uint64_t const p_first = p->p_paddr;
  uint64_t const p_last  = p->p_paddr + p->p_memsz - 1;

  return !((mod_end < p_first) || (p_last < mod_start));
}

static inline bool in_range(struct ph64 const * p, uint64_t start, uint64_t size)
{
  if (size == 0)
    return false;

  return overlap(start, start + size - 1, p);
}

void *mbi_alloc_protected_memory(struct mbi *multiboot_info, size_t len, unsigned align);

/* EOF */
