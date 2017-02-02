/*
 * Multiboot 2 structures
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

#pragma once

#include <stdint.h>

enum mbi2_enum
  {
    MBI2_MAGIC       = 0x36d76289,
    MBI2_TAG_END     = 0,
    MBI2_TAG_CMDLINE = 1,
    MBI2_TAG_MODULE  = 3,
    MBI2_TAG_MEMORY  = 6,
    MBI2_TAG_FB      = 8,
  };

struct mbi2_tag
{
  uint32_t type;
  uint32_t size;
} __attribute__((packed));

struct mbi2_module
{
  uint32_t mod_start;
  uint32_t mod_end;
  char     string[];
} __attribute__((packed));

struct mbi2_memory
{
  uint64_t addr;
  uint64_t len;
  uint32_t type;
  uint32_t reserved;
} __attribute__((packed));

struct mbi2_fb {
  uint64_t addr;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t  bpp;
  uint8_t  type;
} __attribute__((packed));

static inline
struct mbi2_tag * mbi2_first(void *multiboot)
{
  struct mbi2_tag * header = (struct mbi2_tag *)multiboot;
  return header + 1;
}

static inline
struct mbi2_tag * mbi2_next(struct mbi2_tag *c)
{
  uint32_t offset = (c->size + (sizeof(*c) - 1)) & ~(sizeof(*c) - 1);
  struct mbi2_tag * n = (struct mbi2_tag *)((unsigned long)c + offset);
  return (n->type == 0) ? 0 : n;
}
/* EOF */
