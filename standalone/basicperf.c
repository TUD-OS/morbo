/* -*- Mode: C -*- */

#include <pci.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <mbi-tools.h>

static void
t_empty(void)
{
}

static void
t_vmcall(void)
{
  asm volatile ("vmcall" : : "a" (0));
}

static void
t_cpuid(void)
{
  uint32_t eax = 0;
  asm volatile ("cpuid" : "+a" (eax) :: "ecx", "edx", "ebx");
}

static void
t_portio(void)
{
  inb(0x60);                    /* keyboard */
}

static void
t_mmio(void)
{
  ((volatile uint32_t *)0xFEE00000)[1]; /* APIC reg 1 */
}

static void
t_fmmio(void)
{
  asm volatile ("fild %0" : "+m" (((volatile float *)0xFEE00000)[1]));
}

struct test {
  const char *name;
  void (*test_fn)(void);
};

static const struct test tests[] = {
  { "empty ",  t_empty },
  { "vmcall", t_vmcall },
  { "cpuid ", t_cpuid },
  { "portio", t_portio },
  { "mmio  ", t_mmio },
  /* Doesn't work. :) */
  //{ "fmmio ", t_fmmio },
};

static float sqrtf(float v)
{
  asm ("fsqrt" : "+t" (v));
  return v;
}

int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();

  if (magic != MBI_MAGIC) {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  printf("\nbasicperf %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("Testing \"Basic VM performance\" in %s:\n", __FILE__);

  static const unsigned max_stddev = 1000;
  static const unsigned tries = 2048;
  static uint32_t results[2048];
  memset(results, 0, sizeof(results)); /* Warmup */

  for (unsigned i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
    unsigned retries = 0;
    uint64_t start, end;

  again:
    /* Do a warmup round and then the real measurement. */
    for (unsigned w = 0; w < 2; w++)
      for (unsigned j = 0; j < tries; j++) {
        start = rdtsc();
        tests[i].test_fn();
        end = rdtsc();

        uint32_t dur = end - start;
        results[j] = dur;
      }

    uint64_t sum = 0;
    uint32_t min = ~0UL;
    uint32_t max =  0UL;

    for (unsigned j = 0; j < tries; j++) {
      sum += results[j];
      min  = MIN(min, results[j]);
      max  = MAX(max, results[j]);
    }

    float mean = (float)sum / tries;

    float sqdiff = 0;
    for (unsigned j = 0; j < tries; j++) sqdiff += (mean - results[j])*(mean - results[j]);
    float stddev = sqrtf(sqdiff/tries);

    if ((retries++ < 5) && (stddev > max_stddev)) {
      printf("Retry test %s because of instability: stddev %u\n", tests[i].name, (uint32_t)stddev);
      goto again;
    }

    printf("! PERF: %s %u cycles (retries=%u stddev=%u min=%u max=%u) ok\n",
           tests[i].name, (uint32_t)mean, retries, (uint32_t)stddev, min, max);
  }
  printf("wvtest: done\n");

  return 0;
}
