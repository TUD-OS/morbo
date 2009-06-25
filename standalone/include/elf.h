/*
 * \brief   elf structures
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

#include <mbi.h>


struct eh
{
  unsigned char  e_ident[16];
  unsigned short e_type;
  unsigned short e_machine;
  unsigned int   e_version;
  unsigned int   e_entry;
  unsigned int   e_phoff;
  unsigned int   e_shoff;
  unsigned int   e_flags;
  unsigned short e_ehsz;
  unsigned short e_phentsize;
  unsigned short e_phnum;
  unsigned short e_shentsize;
  unsigned short e_shnum;
  unsigned short e_shstrndx;
};

struct ph {
  unsigned int p_type;
  unsigned int p_offset;
  char * p_vaddr;
  char * p_paddr;
  unsigned int p_filesz;
  unsigned int p_memsz;
  unsigned int p_flags;
  unsigned int p_align;
};

int start_module(const struct mbi *mbi);

/* EOF */
