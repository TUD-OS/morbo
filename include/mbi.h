/*
 * \brief   multiboot structures
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


#pragma once

#include <stdint.h>

enum mbi_enum
  {
    MBI_MAGIC                  = 0x2badb002,
    MBI_FLAG_MEM               = 1 << 0,
    MBI_FLAG_CMDLINE           = 1 << 2,
    MBI_FLAG_MODS              = 1 << 3,
    MBI_FLAG_MMAP              = 1 << 6,
    MBI_FLAG_BOOT_LOADER_NAME  = 1 << 9,
  };


struct mbi
{
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t dummy0[4];
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t dummy1[3];
  uint32_t boot_loader_name;
};


struct module
{
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t reserved;
};

/* EOF */
