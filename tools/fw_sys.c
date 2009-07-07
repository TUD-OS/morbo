#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <libraw1394/raw1394.h>

#define PAGE_SIZE   0x1000U


#define LOWER_BOUND 0x00000000ULL
#define UPPER_BOUND 0x01000000ULL 

#define LARGEST_ALIGNMENT  6
#define SMALLEST_ALIGNMENT 2

#define PROBE_SIZE  (18*8)

#define MASK_LOW(bits) ((1 << (bits))-1)
#define BITSET(x, n) ((x) & (1<<(n)) != 0)

static int port = 0;
static int node = 0;
static raw1394handle_t fw_handle;

/* Blacklisted memory areas. Sorted by starting address. */
struct blacklist_entry {
  uint64_t start;
  uint64_t length;
} blacklist[] = { 
  /* BIOS stuff */
  { .start = 0x8F000,
    .length = 0x100000 - 0x8F000
  },
};

bool
looks_like_system_desc(char *bbuf)
{
  uint32_t *buf = (uint32_t *)bbuf;

  uint32_t base  = (buf[0] >> 16) | ((buf[1] & 0xFF) << 16) | (buf[1] & 0xFF000000);
  uint32_t limit = (buf[0] & 0xFFFF) | (buf[1] & 0x000F0000);
  bool present = BITSET(buf[1], 15);
  bool gran = BITSET(buf[1], 23);

  return present && (base == 0) && (limit == 0xFFFFF);
}

bool
looks_like_gdt(char *buf)
{
  /* XXX B0rken? */
  return 
    looks_like_system_desc(buf + 8) &&
    looks_like_system_desc(buf + 16);
}

bool
looks_like_gate(char* buf)
{
  uint32_t *wbuf = (uint32_t *)buf;
  bool taskgate = (0x9F00 & wbuf[1]) == 0x8500;
  bool intgate  = (0x9FE0 & wbuf[1]) == 0x8E00;
  bool trapgate = (0x9FE0 & wbuf[1]) == 0x8F00;

  return taskgate || intgate || trapgate;
}

bool
looks_like_idt(char* buf)
{
  for (unsigned i = 0; i + 8 <= PROBE_SIZE; i += 8) {
    if (!looks_like_gate(buf + i))
      return false;
  }
  return true;
}

bool
page_blacklisted(uint64_t addr)
{
  for (unsigned i = 0;
       i < sizeof(blacklist)/sizeof(struct blacklist_entry);
       i++) {

    if ((blacklist[i].start <= addr) &&
        (addr < blacklist[i].start + blacklist[i].length))
      return true;

    /* The list is sorted by size. */
    if (blacklist[i].start > addr)
      break;
  }

  return false;
}

int
main(int argc, char **argv)
{
  /* Command line parsing */
  int opt;

  while ((opt = getopt(argc, argv, "n:p:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'n':
      node = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: %s [-n nodeid] [-p port]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  /* Connect to raw1394 device. */
  fw_handle = raw1394_new_handle_on_port(port);
  if (fw_handle == NULL) {
    perror("raw1394_new_handle_on_port");
    exit(EXIT_FAILURE);
  }

  printf("Scanning for descriptor tables...\n");

  /* Alignment is decremented */
  for (unsigned align_bits = LARGEST_ALIGNMENT;
       align_bits >= SMALLEST_ALIGNMENT; align_bits--) {
    unsigned align = 1 << align_bits;
    printf("Alignment: 0x%x\n", align);
    for (uint64_t addr = LOWER_BOUND; addr < UPPER_BOUND; addr += align) {
      
      if (((align_bits != LARGEST_ALIGNMENT) && ((addr & MASK_LOW(align_bits+1)) == 0)) ||
          (page_blacklisted(addr)))
        continue;
      
      char probe[PROBE_SIZE];
      int res = raw1394_read(fw_handle, node, addr, PROBE_SIZE, (quadlet_t *)probe);

      if (res == -1) {
        perror("raw1394_read");
        continue;
      }

      if (looks_like_gdt(probe))
        printf("GDT found at 0x%llx.\n", addr);
      
      if (looks_like_idt(probe))
        printf("IDT found at 0x%llx.\n", addr);
      
    }
  }

  return 0;
}
