/* -*- Mode: C -*- */
/*
 * Multiboot 2 interface.
 *
 * Copyright (C) 2017, Alexander Boettcher <alexander.boettcher@genode-labs.com>
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

#include <stdbool.h>
#include <mbi2.h>
#include <mbi-tools.h>
#include <elf.h>
#include <util.h>

static
uint64_t find_mbi2_memory(void * multiboot, size_t const binary_size,
                          bool highest, uint64_t mem_below)
{
  struct mbi2_fb * fb = 0;

  for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
    if (i->type != MBI2_TAG_FB)
      continue;

    fb = (struct mbi2_fb *) (i + 1);

    printf("framebuffer at [%llx+%llx) %ux%u@%u\n", fb->addr,
           fb->addr + fb->pitch * fb->height, fb->width, fb->height, fb->bpp);
    if (fb->type != 1)
      printf("warning - unknown framebuffer type\n");

    break;
  }

  uint64_t binary_start = 0;

  for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
    if (i->type != MBI2_TAG_MEMORY)
      continue;

    enum { MMAP_AVAIL = 1 };

    struct mbi2_memory_map {
      uint32_t entry_size;
      uint32_t entry_version;
    } const * h = (struct mbi2_memory_map const *) (i + 1);

    struct mbi2_memory const * m = (struct mbi2_memory const *)(h + 1);
    unsigned elements = (i->size - sizeof(i) - sizeof(h)) / h->entry_size;

    for (unsigned j = 0; j < elements; m++, j++)
    {
      /* align memory region to 4k */
      uint64_t mem_start = (m->addr + 0xFFFULL) & ~0xFFFULL;
      uint64_t mem_size = m->len - (mem_start - m->addr);

      if (mem_start > mem_below)
        continue;

      if (mem_start + mem_size > mem_below)
        mem_size = mem_below - mem_start;

      /* exclude bender image */
      exclude_bender_binary(&mem_start, &mem_size);
      /* exclude framebuffer */
      if (fb && mem_size)
        exclude_region(&mem_start, &mem_size, fb->addr,
                       fb->addr + fb->pitch * fb->height - 1);

      if (!(m->type == MMAP_AVAIL && binary_size <= mem_size))
        continue;

      if (mem_start > binary_start) {
        if (highest)
          binary_start = (mem_start + mem_size - binary_size) & ~0xFFFULL;
        else
          binary_start = mem_start;
      }

      if (!highest)
        return binary_start;
    }
  }
  return binary_start;
}


static
int check_mem (struct ph64 const * p, uint32_t const binary, uint8_t ** var)
{
  if (p->p_type != ELF_PT_LOAD)
    return 0;

  void * multiboot = (void *)var;
  uintptr_t mbi2_addr = (uintptr_t)multiboot;
  uint32_t mbi2_len = mbi2_size(multiboot);

  if (in_range(p, mbi2_addr, mbi2_len)) {
    printf("Multiboot struct %llx+%llx overlaps with phdr %llx+%llx\n",
           mbi2_addr, mbi2_len, p->p_paddr, p->p_memsz);
    return 1;
  }

  bool in_ram = false;
  for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
    if (i->type != MBI2_TAG_MEMORY)
      continue;

    enum { MMAP_AVAIL = 1 };

    struct mbi2_memory_map {
      uint32_t entry_size;
      uint32_t entry_version;
    } const * h = (struct mbi2_memory_map const *) (i + 1);

    struct mbi2_memory const * m = (struct mbi2_memory const *)(h + 1);
    unsigned elements = (i->size - sizeof(i) - sizeof(h)) / h->entry_size;

    for (unsigned j = 0; j < elements; m++, j++)
    {
      if (m->addr <= p->p_paddr && p->p_paddr + p->p_memsz < m->addr + m->len)
        in_ram = true;

      if (m->type == MMAP_AVAIL)
        continue;

      if (in_range(p, m->addr, m->len)) {
        printf("Reserved memory %llx+%llx type=%u overlaps with phdr %llx+%llx\n",
               m->addr, m->len, m->type, p->p_paddr, p->p_memsz);
        return 1;
      }
    }
  }

  /* check that p does not overlap with bender binary */
  if (overlap_bender_binary(p))
    return 1;

  if (in_ram)
    return 0;

  printf("phdr %llx+%llx is outside RAM !\n", p->p_paddr, p->p_memsz);
  return 2;
}

static
int check_reloc (struct ph64 const * p, uint32_t const binary, uint8_t ** var)
{
  if (p->p_type != ELF_PT_LOAD)
    return 0;

  unsigned mod_relocate = 0;

  struct mbi2_module ** modules = (struct mbi2_module **)var;
  for (struct mbi2_module ** m = modules; *m; m++) {
    mod_relocate ++;

    struct mbi2_module * module = *m;

    if (overlap(module->mod_start, module->mod_end, p))
      return mod_relocate;
  }

  return 0;
}

int
start_module2(void *multiboot, bool uncompress, uint64_t phys_max)
{
  enum { MBI2_TAG_INVALID = 0xbad }; /* XXX */

  struct mbi2_module * modules[32];
  unsigned module_count = 0;

  /* collect modules and module count and invalidate current CMD_LINE */
  for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
    if (i->type == MBI2_TAG_MODULE) {
      struct mbi2_module * module = (struct mbi2_module *)(i + 1);
      modules[module_count] = module;
      module_count ++;
    }

    if (module_count + 1 >= sizeof(modules) / sizeof(modules[0])) {
      printf("too many modules\n");
      return 1;
    }

    if (i->type != MBI2_TAG_CMDLINE)
      continue;

    /* invalidate our current boot CMD_LINE */
    i->type = MBI2_TAG_INVALID;
  }

  if (!module_count) {
    printf("no module to load\n");
    return 1;
  }
  modules[module_count] = 0;

  /* sanity check that next module will be unpacked to free memory */
  int error = for_each_phdr(modules[0]->mod_start, (uint8_t **)multiboot, check_mem);
  if (error)
    return error;

  /**
   * Relocate all modules to high memory. Must be below 4G since mod_start is
   * solely 32bit value !
   */
  uint64_t mem_below = (phys_max > 1ULL << 32) ? 1ULL << 32 : phys_max;

  int i = 0;
  while ((i = for_each_phdr(modules[0]->mod_start, (uint8_t **)modules, check_reloc))) {
    struct mbi2_module * rel_module = modules[i - 1];
    uint32_t const binary_size = rel_module->mod_end - rel_module->mod_start + 1;
    uint64_t relocate_addr = find_mbi2_memory(multiboot, binary_size, true, mem_below);

    if (!relocate_addr) {
      printf("no memory for relocation found\n");
      return 1;
    }

    mem_below = relocate_addr;

    memcpy((char *)(uintptr_t)relocate_addr, (void *)rel_module->mod_start, binary_size);

    uint64_t const offset = relocate_addr - rel_module->mod_start;
    rel_module->mod_start += offset;
    rel_module->mod_end   += offset;
  }

  /* look up first module and start as kernel */
  for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
    if (i->type != MBI2_TAG_MODULE)
      continue;

    /* invalidate current module */
    uint32_t const size = i->size;

    i->type = MBI2_TAG_INVALID;
    i->size = sizeof(*i);

    /* setup module command line as kernel boot command line */
    struct mbi2_module * module = (struct mbi2_module *)(i + 1);
    uint32_t const binary = module->mod_start;

    module->mod_start = MBI2_TAG_CMDLINE;
    module->mod_end   = size - sizeof(*i);

    uint64_t jump_code = find_mbi2_memory(multiboot, 0x1000, true, mem_below);
    if (!jump_code) {
      printf("No address for jump code generation?\n");
      return 1;
    }

    /* load the next module */
    load_elf(multiboot, binary, MBI2_MAGIC, jump_code);
    break;
  }
  return 1;
}
