/* -*- Mode: C -*- */

#include <cpuid.h>

bool has_apic(void)
{
  cpu_id_t res;

  do_cpuid(1, &res);

  return (res.edx >> 9) & 1;
}

void enable_apic(void)
{
  uint64_t msr = read_msr(IA32_APIC_BASE);
  uint32_t hi  = msr >> 32;
  uint32_t low = msr & 0xFFFFFFFF;

  /* Set default physical base address. */
  low &= ~APIC_PHYS_BASE_MASK;
  low |= APIC_DEFAULT_PHYS_BASE;

  /* Set the enable bit. */
  low |= APIC_ENABLE;

  write_msr(IA32_APIC_BASE, ((uint64_t)hi<<32) | low);
}

/* EOF */
