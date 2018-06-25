/*
 * \brief   Elf extraction.
 * \date    2006-06-07
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

#include <elf.h>
#include <util.h>
#include <mbi-tools.h>

enum {
  EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
};

static void
byte_out(uint8_t **code, uint8_t byte)
{
  **code = byte;
  (*code)++;
  /* XXX Check overflow. */
}

static void
gen_mov(uint8_t **code, int reg, uint32_t constant)
{
  byte_out(code, 0xB8 | reg);
  *((uint32_t *)*code) = constant;
  *code += sizeof(uint32_t);
}

static void
gen_jmp_edx(uint8_t **code)
{
  byte_out(code, 0xFF); byte_out(code, 0xE2);
}

static void
gen_elf_segment(uint8_t **code, uintptr_t target, void *src, size_t len,
                size_t fill)
{
  gen_mov(code, EDI, (uint32_t)target);
  gen_mov(code, ESI, (uint32_t)src);
  gen_mov(code, ECX, len);
  byte_out(code, 0xF3);         /* REP */
  byte_out(code, 0xA4);         /* MOVSB */
  /* EAX is zero at this point. */
  gen_mov(code, ECX, fill);
  byte_out(code, 0xF3);         /* REP */
  byte_out(code, 0xAA);         /* STOSB */
}

int
start_module(struct mbi *mbi, bool uncompress)
{
  if (((mbi->flags & MBI_FLAG_MODS) == 0) || (mbi->mods_count == 0)) {
    printf("No module to start.\n");
    return -1;
  }

  mbi_relocate_modules(mbi, uncompress);

  // skip module after loading
  struct module *m  = (struct module *) mbi->mods_addr;
  mbi->mods_addr += sizeof(struct module);
  mbi->mods_count--;
  mbi->cmdline = m->string;

  // switch it on unconditionally, we assume that m->string is always initialized
  mbi->flags |=  MBI_FLAG_CMDLINE;

  // check elf header
  struct eh *elf = (struct eh *) m->mod_start;
  assert(memcmp(elf->e_ident, ELFMAG, SELFMAG) == 0, "ELF header incorrect");

  uint8_t *code = (uint8_t *)0x7C00;

#define LOADER(EH, PH) {                                                \
    struct EH *elfc = (struct EH *)elf;                                               \
    assert(elfc->e_type==2 && ((elfc->e_machine == EM_386) || (elfc->e_machine == EM_X86_64)) && elfc->e_version==1, "ELF type incorrect"); \
    assert(sizeof(struct PH) <= elfc->e_phentsize, "e_phentsize too small"); \
                                                                        \
    for (unsigned i = 0; i < elfc->e_phnum; i++) {                      \
      struct PH *ph = (struct PH *)(uintptr_t)(m->mod_start + elfc->e_phoff+ i*elfc->e_phentsize); \
      if (ph->p_type != 1)                                              \
        continue;                                                       \
      gen_elf_segment(&code, ph->p_paddr, (void *)(uintptr_t)(m->mod_start+ph->p_offset), ph->p_filesz, \
                      ph->p_memsz - ph->p_filesz);                      \
    }                                                                   \
                                                                        \
    gen_mov(&code, EAX, 0x2BADB002);                                    \
    gen_mov(&code, EDX, elfc->e_entry);                                 \
  }

  switch (elf->e_ident[EI_CLASS]) {
  case ELFCLASS32:
    LOADER(eh, ph);
    break;
  case ELFCLASS64:
    LOADER(eh64, ph64);
    break;
  default:
    assert(false, "Invalid ELF class");
  }

  gen_jmp_edx(&code);
  asm volatile  ("jmp *%%edx" :: "a" (0), "d" (0x7C00), "b" (mbi));

  /* NOT REACHED */
  return 0;
}

/* EOF */
