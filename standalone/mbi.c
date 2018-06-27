/* -*- Mode: C -*- */

#include <mbi.h>
#include <stddef.h>
#include <util.h>
#include <tinf.h>


/** Find a sufficiently large block of free memory that is page aligned.
 */
bool
mbi_find_memory(const struct mbi *multiboot_info, size_t len,
                void **block_start_out, size_t *block_len_out,
                bool highest, uint64_t const limit_to)
{
  bool found         = false;
  size_t mmap_len    = multiboot_info->mmap_length;
  memory_map_t *mmap = (memory_map_t *)multiboot_info->mmap_addr;

  /* be paranoid */
  if (limit_to <= len)
    return false;

  while ((uint32_t)mmap < multiboot_info->mmap_addr + mmap_len) {
    uint64_t block_len  = (uint64_t)mmap->length_high<<32 | mmap->length_low;
    uint64_t block_addr = (uint64_t)mmap->base_addr_high<<32 | mmap->base_addr_low;

    /* Memory blocks may not be page aligned. Round length and address
       to page granularity. */
    uint64_t nblock_addr = (block_addr + 0xFFF) & ~0xFFF;
    if (nblock_addr > (block_addr + block_len)) continue;
    block_len -= (nblock_addr - block_addr);
    block_len  = block_len & ~0xFFF;
    block_addr = nblock_addr;

    if ((mmap->type == MMAP_AVAILABLE) && (block_len >= len) &&
        (block_addr <= limit_to - len)) {

      if ((found == true) && ((uintptr_t)*block_start_out > block_addr))
        continue;

      found = true;

      /* take care that (block_addr+len) is below limit_to */
      uint64_t top_addr = block_addr + block_len;
      if (top_addr > limit_to)
        top_addr = limit_to;

      *block_start_out = (void *)(uintptr_t)top_addr - len;
      *block_len_out   = (size_t)len;

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
 * Returns true of module is compressed and can be inflated. Inflated
 * size is returned in uncompressed.
 */
static bool
gzip_info(struct module *mod, size_t *uncompressed)
{
  int ret = tinf_gzip_uncompress(NULL, uncompressed,
                                 (void *)mod->mod_start,
                                 mod->mod_end - mod->mod_start);
  return (ret == TINF_OK);    
}

/**
 * Push all modules to the highest location in memory.  This is
 * somewhat EXPERIMENTAL. If uncompress is true, we transparently
 * uncompress all modules. If uncompress is set and relocation fails,
 * we consider this as fatal error (panic).
 */
void
mbi_relocate_modules(struct mbi *mbi, bool uncompress, uint64_t phys_max)
{
  size_t size = 0;
  bool need_inflate = false;

  if (uncompress)
    tinf_init();

  struct module *mods = (struct module *)mbi->mods_addr;
  struct { 
    size_t modlen;
    size_t slen;
    size_t inflated_size;
    bool   do_inflate;
  } minfo[mbi->mods_count];

  for (unsigned i = 0; i < mbi->mods_count; i++) {

    minfo[i].modlen = mods[i].mod_end - mods[i].mod_start;
    minfo[i].slen   = strlen((const char *)mods[i].string) + 1;

    minfo[i].do_inflate = uncompress && gzip_info(&mods[i], &minfo[i].inflated_size);
    if (minfo[i].do_inflate)
      need_inflate = true;

    size += minfo[i].do_inflate ? minfo[i].inflated_size : minfo[i].modlen;
    size += minfo[i].slen;

    /* Round up to page size */
    size = (size + 0xFFF) & ~0xFFF;
  }

  void *block;
  size_t block_len;

  if (mbi_find_memory(mbi, size, &block, &block_len, true, phys_max)) {
    /* Check for overlap */
    uintptr_t reladdr = (uintptr_t)block + block_len - size;
    for (unsigned i = 0; i < mbi->mods_count; i++) {
      if (mods[i].mod_end > reladdr) {
        if (reladdr == mods[i].mod_start) {
          printf("Modules seem to be relocated. Good.\n");
          goto silent_fail;
        } else {
          printf("Modules might overlap.\nRelocate to %p, but module at %8x-%8x.\n",
                 reladdr, mods[i].mod_start, mods[i].mod_end-1);
          goto fail;
        }
      }
    }

    printf("Need %8x bytes to relocate modules.\n", size);
    printf("Relocating to %8x: \n", reladdr);

    for (int i = mbi->mods_count - 1; i >= 0; i--) {
      size_t target_len = minfo[i].do_inflate ? minfo[i].inflated_size : minfo[i].modlen;
      block_len -= (minfo[i].slen + target_len + 0xFFF) & ~0xFFF;
    
      if (minfo[i].do_inflate) {
        size_t uncompressed;
        printf("Inflating %u -> %u bytes...\n", minfo[i].modlen, target_len);
        int res = tinf_gzip_uncompress((char *)block + block_len, &uncompressed,
                                       (void *)mods[i].mod_start, minfo[i].modlen);
        assert((res == TINF_OK) && (uncompressed == target_len),
               "Error decompressing data.");
      } else {
        printf("Copying %u bytes...\n", minfo[i].modlen);
        memcpy((char *)block + block_len, (void *)mods[i].mod_start,
               minfo[i].modlen);
      }
      mods[i].mod_start = (size_t)((char *)block + block_len);
      mods[i].mod_end = mods[i].mod_start + target_len;

      memcpy((char *)block + block_len + target_len, 
             (void *)mods[i].string, minfo[i].slen);
      mods[i].string = (uintptr_t)((char *)block + block_len + target_len);
    }
    printf("\n");

  } else {
  fail:
    printf("Cannot relocate.\n");
  silent_fail:
    assert(!need_inflate, "Couldn't relocate, which is required for decompressing.");
  }
}


/* EOF */
