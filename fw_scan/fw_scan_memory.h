/* -*- Mode: C -*- */

#include <stdbool.h>
#include <mbi.h>

struct memory_info {
  struct memory_info *next;
  uint32_t type;
  uint64_t addr;
  uint64_t length;
};

struct memory_info *parse_mbi_mmap(struct memory_map *mmap_buf, size_t mmap_length);
bool mem_range_available(struct memory_info *info, uint64_t start, uint64_t length);




/* EOF */
