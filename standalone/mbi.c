/* -*- Mode: C -*- */

#include <mbi.h>
#include <stddef.h>
#include <util.h>

/** Allocates an aligned block of memory from the multiboot memory
    map. */
void *
mbi_alloc_protected_memory(struct mbi *multiboot_info, size_t len, unsigned align)
{
  uint32_t align_mask = ~((1<<align)-1);
  size_t mmap_len    = multiboot_info->mmap_length;
  memory_map_t *mmap = (memory_map_t *)multiboot_info->mmap_addr;
  
  while ((uint32_t)mmap < multiboot_info->mmap_addr + mmap_len) {
    uint64_t block_len  = (uint64_t)mmap->length_high<<32 | mmap->length_low;
    uint64_t block_addr = (uint64_t)mmap->base_addr_high<<32 | mmap->base_addr_low;

    if ((mmap->type == MMAP_AVAILABLE) && (block_len >= len) &&
	(((block_addr + block_len) >> 32) == 0 /* Block below 4GB? */) &&
	/* Still large enough with alignment? Don't use the block if
	   it fits exactly, otherwise we would have to remove it.*/
	(((block_addr + block_len - len) & align_mask) > block_addr)) {

      uint32_t aligned_len = block_addr + block_len - ((block_addr + block_len - len) & align_mask);
      
      /* Shorten block. */
      block_len -= aligned_len;
      mmap->length_high = block_len >> 32;
      mmap->length_low  = block_len & 0xFFFFFFFFU;

      return (void *)(uint32_t)(block_addr + block_len);
    }
  
    /* Skip to next entry. */
    mmap = (memory_map_t *)(mmap->size + (uint32_t)mmap + sizeof(mmap->size));
  }

  assert(0, "No space for ConfigROM.");
}

/* EOF */
