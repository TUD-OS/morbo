/* -*- Mode: C -*- */

#include <mbi.h>
#include <stddef.h>
#include <util.h>


/** Find a sufficiently large block of free memory.
 */
bool
mbi_find_memory(const struct mbi *multiboot_info, size_t len,
                void **block_start_out, size_t *block_len_out,
                bool highest)
{
  bool found         = false;
  size_t mmap_len    = multiboot_info->mmap_length;
  memory_map_t *mmap = (memory_map_t *)multiboot_info->mmap_addr;

  while ((uint32_t)mmap < multiboot_info->mmap_addr + mmap_len) {
    uint64_t block_len  = (uint64_t)mmap->length_high<<32 | mmap->length_low;
    uint64_t block_addr = (uint64_t)mmap->base_addr_high<<32 | mmap->base_addr_low;

    if ((mmap->type == MMAP_AVAILABLE) && ((block_addr >> 32) == 0ULL) &&
        (block_len >= len)) {

      if ((found == true) && ((uintptr_t)*block_start_out > block_addr))
        continue;

      found = true;
      *block_start_out = (void *)(uintptr_t)block_addr;
      *block_len_out   = (size_t)block_len;

      if (!highest) return true;
    }
  
    /* Skip to next entry. */
    mmap = (memory_map_t *)(mmap->size + (uint32_t)mmap + sizeof(mmap->size));
  }
  
  return found;
}

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
	(((block_addr + block_len) >> 32) == 0ULL /* Block below 4GB? */) &&
	/* Still large enough with alignment? Don't use the block if
	   it fits exactly, otherwise we would have to remove it. */
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

/**
 * Push all modules to the highest location in memory.
 * This is somewhat EXPERIMENTAL.
 */
void
mbi_relocate_modules(struct mbi *mbi)
{
  size_t size = 0;

  struct module *mods = (struct module *)mbi->mods_addr;
  struct { 
    size_t modlen;
    size_t slen;
  } minfo[mbi->mods_count];

  for (unsigned i = 0; i < mbi->mods_count; i++) {

    minfo[i].modlen = mods[i].mod_end - mods[i].mod_start;
    minfo[i].slen   = strlen((const char *)mods[i].string + 1);
    size += minfo[i].modlen;
    size += minfo[i].slen;

    /* Round up to page size */
    size = (size + 0xFFF) & ~0xFFF;
  }

  printf("Need %8x bytes to relocate modules.\n", size);

  void *block;
  size_t block_len;

  if (mbi_find_memory(mbi, size, &block, &block_len, true)) {
    /* Check for overlap */
    for (unsigned i = 0; i < mbi->mods_count; i++) {
      if (mods[i].mod_end > ((uintptr_t)block + block_len - size)) {
        printf("Modules might overlap.\nRelocate to %p, but module at %8x-%8x.\n",
               (char *)block + block_len - size, mods[i].mod_start, mods[i].mod_end-1);
        goto fail;
      }
    }

    printf("Relocating to %8x: ", (uintptr_t)block + block_len - size);

    for (unsigned i = 0; i < mbi->mods_count; i++) {
      printf(".");
      block_len -= (minfo[i].slen + minfo[i].modlen + 0xFFF) & ~0xFFF;
      memcpy((char *)block + block_len, (void *)mods[i].mod_start,
             minfo[i].modlen);
      mods[i].mod_start = (size_t)((char *)block + block_len);
      mods[i].mod_end = mods[i].mod_start + minfo[i].modlen;

      memcpy((char *)block + block_len + minfo[i].modlen, 
             (void *)mods[i].string, minfo[i].slen);
      mods[i].string = (uintptr_t)((char *)block + block_len + minfo[i].modlen);
    }
    printf("\n");

  } else {
    fail:
    printf("Cannot relocate.\n");
  }
}


/* EOF */
