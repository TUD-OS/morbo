/* -*- Mode: C -*- */
/*
 * Multiboot tools checking image overlap
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

#include <mbi-tools.h>
#include <util.h>

void exclude_region(uint64_t *block_addr, uint64_t *block_len,
                    uintptr_t const image_start, uintptr_t const image_end)
{
  if (!*block_len)
    return;

  /* check that block is not completely part of the image */
  if (image_start <= *block_addr && (*block_addr + *block_len - 1) <= image_end) {
    *block_len = 0;
    return;
  }

  /* cut image out of block and determine largest block of the up to 2 new bender free blocks */
  if (*block_addr <= image_start && image_start < *block_addr + *block_len) {
    uint64_t front_len = image_start - *block_addr;
    if (*block_addr <= image_end && image_end < *block_addr + *block_len) {
      uint64_t back_len = *block_addr + *block_len - image_end - 1;
      if (back_len > front_len) {
        *block_addr = image_end + 1;
        *block_len = back_len;
      } else
        *block_len = front_len;
    } else
      *block_len = front_len;
  } else {
    if (*block_addr <= image_end && image_end < *block_addr + *block_len) {
      *block_len -= image_end + 1 - *block_addr;
      *block_addr = image_end + 1;
    }
  }
}

void exclude_bender_binary(uint64_t *block_addr, uint64_t *block_len)
{
  extern uintptr_t _image_start;
  extern uintptr_t _image_end;
  uintptr_t image_start = (uintptr_t)&_image_start;
  uintptr_t image_end   = (uintptr_t)&_image_end;

  exclude_region(block_addr, block_len, image_start, image_end);
}

bool overlap_bender_binary(struct ph64 const * p)
{
  extern uintptr_t _image_start;
  extern uintptr_t _image_end;
  uintptr_t image_start = (uintptr_t)&_image_start;
  uintptr_t image_end   = (uintptr_t)&_image_end;

  if (!overlap(image_start, image_end, p))
    return false;

  printf("Bender memory %lx+%lx overlaps with phdr %llx+%llx\n",
         image_start, image_end, p->p_paddr, p->p_memsz);
  return true;
}
