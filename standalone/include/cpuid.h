/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct cpu_id_t {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
} cpu_id_t;

/**
 * Runs CPUID and stores its result in the specifed structure.
 */
void do_cpuid(uint32_t eax, cpu_id_t *out) __attribute__((regparm(3)));

enum IA32_MSRs {
  IA32_APIC_BASE = 0x001b,
};

enum IA32_APIC_MSR {
  APIC_DEFAULT_PHYS_BASE = 0xFEE00000,
  APIC_PHYS_BASE_MASK    = 0xFFFFF000,
  APIC_ENABLE            = 1<<11,
};

uint64_t read_msr(uint32_t msr) __attribute__((regparm(3)));
void write_msr(uint32_t msr, uint64_t data) __attribute__((regparm(0)));

/**
 * Uses CPUID to find out if the CPU has an enabled APIC.
 */
bool has_apic(void);

/**
 * Tries to enable the APIC. Should work for anything after the P6.
 */
void enable_apic(void);

/* EOF */
