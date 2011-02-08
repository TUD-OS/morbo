/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <mbi.h>

bool mbi_find_memory(const struct mbi *multiboot_info, size_t len,
                     void **block_start, size_t *block_len,
                     bool highest);

void *mbi_alloc_protected_memory(struct mbi *multiboot_info, size_t len, unsigned align);

void mbi_relocate_modules(struct mbi *mbi, bool uncompress);


/* EOF */
