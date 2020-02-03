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

enum {
  EAX = 0, ECX, EDX, EBX, ESP, EBP, ESI, EDI
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

int for_each_phdr(uint32_t const binary, uint8_t ** var,
                  int (*fn)(struct ph64 const *, uint32_t const, uint8_t **))
{
  // check elf header
  struct eh *elf = (struct eh *) binary;
  assert(memcmp(elf->e_ident, ELFMAG, SELFMAG) == 0, "ELF header incorrect");

#define ITER(EH, PH) {                                                  \
    struct EH *elfc = (struct EH *)elf;                                 \
    assert(elfc->e_type==2 && ((elfc->e_machine == EM_386) || (elfc->e_machine == EM_X86_64)) && elfc->e_version==1, "ELF type incorrect"); \
    assert(sizeof(struct PH) <= elfc->e_phentsize, "e_phentsize too small"); \
                                                                        \
    for (unsigned i = 0; i < elfc->e_phnum; i++) {                      \
      struct PH *ph = (struct PH *)(uintptr_t)(binary + elfc->e_phoff+ i*elfc->e_phentsize); \
      struct ph64 const p_tmp = {      \
        .p_type   = ph->p_type,        \
        .p_flags  = ph->p_flags,       \
        .p_offset = ph->p_offset,      \
        .p_vaddr  = ph->p_vaddr,       \
        .p_paddr  = ph->p_paddr,       \
        .p_filesz = ph->p_filesz,      \
        .p_memsz  = ph->p_memsz,       \
        .p_align  = ph->p_align        \
      };                               \
      int e = fn(&p_tmp, binary, var); \
      if (e)                           \
        return e;                      \
    }                                  \
  }

  switch (elf->e_ident[EI_CLASS]) {
  case ELFCLASS32:
    ITER(eh, ph);
    break;
  case ELFCLASS64:
    ITER(eh64, ph64);
    break;
  default:
    assert(false, "Invalid ELF class");
  }
  return 0;
}

static int
elf_phdr(struct ph64 const *ph, uint32_t const binary, uint8_t ** code)
{
  if (ph->p_type != ELF_PT_LOAD)
    return 0;

  gen_elf_segment(code, ph->p_paddr, (void *)(uintptr_t)(binary+ph->p_offset),
                  ph->p_filesz, ph->p_memsz - ph->p_filesz);

  return 0;
}

int
load_elf(void const * mbi, uint32_t const binary, uint32_t const magic,
         uint32_t const jump_code)
{
  uint8_t *code = (uint8_t *)jump_code;

  int error = for_each_phdr(binary, &code, elf_phdr);
  if (error)
    return error;

  gen_mov(&code, EAX, magic);

  struct eh *elf = (struct eh *) binary;
  switch (elf->e_ident[EI_CLASS]) {
  case ELFCLASS32:
  {
    struct eh *elfc = (struct eh *)elf;
    gen_mov(&code, EDX, elfc->e_entry);
    break;
  }
  case ELFCLASS64:
  {
    struct eh64 *elfc = (struct eh64 *)elf;
    gen_mov(&code, EDX, elfc->e_entry);
    break;
  }
  default:
    assert(false, "Invalid ELF class");
    return 1;
  }

  gen_jmp_edx(&code);
  asm volatile  ("jmp *%%edx" :: "a" (0), "d" (jump_code), "b" (mbi));

  /* NOT REACHED */
  return 0;
}

/* EOF */
