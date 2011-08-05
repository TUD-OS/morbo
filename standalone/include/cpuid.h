/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>
#include <stdbool.h>

enum IA32_MSRs {
  IA32_APIC_BASE = 0x001b,
};

enum IA32_APIC_MSR {
  APIC_DEFAULT_PHYS_BASE = 0xFEE00000,
  APIC_PHYS_BASE_MASK    = 0xFFFFF000,
  APIC_ENABLE            = 1<<11,
};

/**
 * Uses CPUID to find out if the CPU has an enabled APIC.
 */
static inline bool
has_apic(void)
{
  uint32_t eax = 1;
  uint32_t edx;

  asm ("cpuid" : "+a" (eax), "=d" (edx) :: "ebx", "ecx");

  return ((edx >> 9) & 1) != 0;
}

/**
 * Tries to enable the APIC. Should work for anything after the P6.
 */
static inline void
enable_apic(void)
{
  uint32_t hi, low;

  asm ("rdmsr" : "=d" (hi), "=a" (low) : "c" (IA32_APIC_BASE));

  /* Set default physical base address. */
  low &= ~APIC_PHYS_BASE_MASK;
  low |= APIC_DEFAULT_PHYS_BASE;

  /* Set the enable bit. */
  low |= APIC_ENABLE;

  asm volatile ("wrmsr" :: "a" (low), "d" (hi), "c" (IA32_APIC_BASE));
}

/* EOF */
